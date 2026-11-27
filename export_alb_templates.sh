#!/usr/bin/env bash
# ALB名の一覧を読み、所属するCloudFormationスタックとテンプレートを保存する。
# AWS CLI v2でログイン済みの認証情報を利用する。必要コマンド: aws、jq。
# 同じアカウント・リージョンのALBを1行に1台記載する（26台以上にも対応）。
# AWSへの操作は取得のみ。失敗したALBは記録し、次のALBへ進む。
# 使用例: bash export_alb_templates.sh alb_names.txt ap-northeast-1
# 終了コード: 0=全件保存、1=未確認/失敗あり、2=準備・認証エラー。
set -uo pipefail
umask 077

if [[ $# -ne 2 ]]; then
  printf '使い方: bash %s ALB名一覧ファイル リージョン\n' "$0" >&2
  exit 2
fi
alb_input=$1
alb_region=$2
[[ -r "$alb_input" ]] || { printf '入力ファイルを読めません。\n' >&2; exit 2; }
for alb_tool in aws jq; do
  command -v "$alb_tool" >/dev/null || { printf '%sが必要です。\n' "$alb_tool" >&2; exit 2; }
done

# 実行ごとに別フォルダを作り、既存の証跡を上書きしない。
alb_output=$(mktemp -d "./alb_templates_$(date +%Y%m%d_%H%M%S)_XXXXXX") || exit 2
alb_options=(--region "$alb_region" --output json --no-cli-pager --no-cli-auto-prompt)
aws --version > "$alb_output/aws_version.txt" 2>&1
printf 'Started=%s\nRegion=%s\nInput=%s\n' \
  "$(date '+%Y-%m-%dT%H:%M:%S%z')" "$alb_region" "$alb_input" > "$alb_output/run_info.txt"
cp -- "$alb_input" "$alb_output/input_names.txt" || exit 2

# 実行日時、コマンド、終了コードと、標準出力・標準エラーを分けて記録する。
record_aws() {
  local destination=$1 step=$2 rc
  shift 2
  {
    printf '\n[%s] ' "$(date '+%Y-%m-%dT%H:%M:%S%z')"
    printf '%q ' aws "${alb_options[@]}" "$@"
    printf '\n'
  } >> "$destination/commands.log"
  if aws "${alb_options[@]}" "$@" > "$destination/$step.json" 2> "$destination/$step.stderr.log"; then
    rc=0
  else
    rc=$?
  fi
  printf 'exit_code=%s\n' "$rc" >> "$destination/commands.log"
  return "$rc"
}

# 認証できない場合は全件処理を開始しない。接続先の証跡も残す。
if ! record_aws "$alb_output" identity sts get-caller-identity; then
  printf '認証情報を確認してください。証跡: %s\n' "$alb_output" >&2
  exit 2
fi
alb_account=$(jq -er '.Account' "$alb_output/identity.json") || exit 2
printf 'アカウント: %s / リージョン: %s\n保存先: %s\n' "$alb_account" "$alb_region" "$alb_output"
printf 'ALBName\tALBArn\tStackName\tStackId\tLogicalResourceId\tStatus\tTemplateFile\tDetail\n' > "$alb_output/results.tsv"

# 各ALBの取得処理。スタック検索結果だけでは現在の管理状態を断定しない。
collect_alb() {
  local found_ids resource_state stack_state body_type
  alb_status=ALB_LOOKUP_FAILED
  alb_detail='ALBの取得に失敗。エラーログを確認。'
  record_aws "$alb_dir" 01_alb elbv2 describe-load-balancers --names "$alb_name" || return 1
  if ! jq -e --arg name "$alb_name" \
    '(.LoadBalancers | length) == 1 and .LoadBalancers[0].LoadBalancerName == $name and .LoadBalancers[0].Type == "application"' \
    "$alb_dir/01_alb.json" >/dev/null; then
    alb_status=NOT_AN_ALB
    alb_detail='指定名がApplication Load Balancerであることを確認できない。'
    return 1
  fi
  alb_arn=$(jq -er '.LoadBalancers[0].LoadBalancerArn' "$alb_dir/01_alb.json") || return 1

  alb_status=STACK_LOOKUP_UNCONFIRMED
  alb_detail='所属スタック未確認。権限不足や管理外等の可能性はログで確認。'
  record_aws "$alb_dir" 02_stack_lookup cloudformation describe-stack-resources --physical-resource-id "$alb_arn" || return 1
  found_ids=$(jq -c '[.StackResources[].StackId | select(type == "string")] | unique' "$alb_dir/02_stack_lookup.json") || return 1
  [[ $(jq 'length' <<< "$found_ids") == 1 ]] || return 1
  alb_stack=$(jq -r '.[0]' <<< "$found_ids") || return 1

  alb_status=STACK_DETAILS_FAILED
  alb_detail='スタックの状態を取得できない。'
  record_aws "$alb_dir" 03_stack cloudformation describe-stacks --stack-name "$alb_stack" || return 1
  alb_stack_name=$(jq -er '.Stacks[0].StackName' "$alb_dir/03_stack.json") || return 1
  stack_state=$(jq -er '.Stacks[0].StackStatus' "$alb_dir/03_stack.json") || return 1
  if [[ $stack_state == DELETE_* ]]; then
    alb_status=DELETING_OR_DELETED_STACK
    alb_detail="削除中・削除関連のスタック状態: $stack_state。現在の管理状態は要確認。"
    return 1
  fi

  # describe-stack-resourcesの100件制限を避け、完全な一覧でARNを照合する。
  # AWS CLIの自動ページングを利用する（--no-paginateは指定しない）。
  alb_status=RESOURCE_UNCONFIRMED
  alb_detail='スタック内で対象ALBの現在の登録を確認できない。'
  record_aws "$alb_dir" 04_resources cloudformation list-stack-resources --stack-name "$alb_stack" || return 1
  jq --arg arn "$alb_arn" \
    '[.StackResourceSummaries[] | select(.PhysicalResourceId == $arn and .ResourceType == "AWS::ElasticLoadBalancingV2::LoadBalancer")]' \
    "$alb_dir/04_resources.json" > "$alb_dir/alb_resource.json" || return 1
  [[ $(jq 'length' "$alb_dir/alb_resource.json") == 1 ]] || return 1
  alb_logical_id=$(jq -er '.[0].LogicalResourceId' "$alb_dir/alb_resource.json") || return 1
  resource_state=$(jq -er '.[0].ResourceStatus' "$alb_dir/alb_resource.json") || return 1
  if [[ $resource_state == DELETE_* ]]; then
    alb_detail="対象ALBのリソース状態: $resource_state。削除・保持後の管理状態は要確認。"
    return 1
  fi

  # ALBが所属するスタックの元テンプレートを取得する。
  # ネスト構造の場合は所属する子スタック。親・ルートIDは03_stack.jsonに残る。
  alb_status=TEMPLATE_FAILED
  alb_detail='テンプレートの取得または書き出しに失敗。'
  record_aws "$alb_dir" 05_template_response cloudformation get-template \
    --stack-name "$alb_stack" --template-stage Original || return 1
  body_type=$(jq -er '.TemplateBody | type' "$alb_dir/05_template_response.json") || return 1
  case "$body_type" in
    object)
      alb_template="$alb_dir/template.json"
      jq '.TemplateBody' "$alb_dir/05_template_response.json" > "$alb_template" || return 1
      ;;
    string)
      # JSON文字列ならJSON、そうでなければYAMLとしてそのまま取り出す。
      if jq -e '.TemplateBody | fromjson | type == "object"' "$alb_dir/05_template_response.json" >/dev/null 2>&1; then
        alb_template="$alb_dir/template.json"
      else
        alb_template="$alb_dir/template.yaml"
      fi
      jq -r '.TemplateBody' "$alb_dir/05_template_response.json" > "$alb_template" || return 1
      ;;
    *) return 1 ;;
  esac
