#!/usr/bin/env bash
# ALB名の一覧から、所属するCloudFormationスタックとテンプレートを取得する。
# AWSの設定変更は行わない。取得に失敗した場合も、次のALBへ進む。
#
# 前提:
#   ・WSLでAWS CLIにログイン済みであること。
#   ・awsとjqがインストールされていること。
#   ・同じアカウント・リージョンのALB名を、1行に1台記載すること。
#
# 入力ファイル例: alb_names.txt
#   test-web-alb
#   prod-web-alb
#   prod-api-alb
#
# 実行例:
#   bash export_alb_templates.sh alb_names.txt ap-northeast-1
#
# 出力:
#   results.tsv          ALB名・スタック名・取得結果の対応表
#   001/template.yaml    1台目のテンプレート（JSONの場合はtemplate.json）
#   002/template.yaml    2台目のテンプレート
#   各番号フォルダ       AWS CLIの取得結果、コマンド履歴、エラーログ
#
# 終了コード:
#   0 = 全件保存成功
#   1 = 未確認または失敗あり
#   2 = 準備または認証エラー

set -uo pipefail
umask 077

# 引数を確認する。
if [[ $# -ne 2 ]]; then
    printf '使い方: bash %s ALB名一覧ファイル リージョン\n' "$0" >&2
    exit 2
fi

alb_input=$1
alb_region=$2

if [[ ! -r "$alb_input" ]]; then
    printf '入力ファイルを読めません: %s\n' "$alb_input" >&2
    exit 2
fi

for alb_tool in aws jq; do
    if ! command -v "$alb_tool" >/dev/null 2>&1; then
        printf '必要なコマンドがありません: %s\n' "$alb_tool" >&2
        exit 2
    fi
done

# 実行ごとに別フォルダを作成し、既存の証跡を上書きしない。
alb_output=$(mktemp -d "./alb_templates_$(date +%Y%m%d_%H%M%S)_XXXXXX") || exit 2

alb_options=(
    --region "$alb_region"
    --output json
    --no-cli-pager
    --no-cli-auto-prompt
)

aws --version > "$alb_output/aws_version.txt" 2>&1

printf 'Started=%s\nRegion=%s\nInput=%s\n' \
    "$(date '+%Y-%m-%dT%H:%M:%S%z')" \
    "$alb_region" \
    "$alb_input" \
    > "$alb_output/run_info.txt"

cp -- "$alb_input" "$alb_output/input_names.txt" || exit 2

# AWS CLIの実行日時・コマンド・終了コードを記録する。
# 標準出力はJSON、標準エラーはstderr.logへ保存する。
record_aws() {
    local destination=$1
    local step=$2
    local rc

    shift 2

    {
        printf '\n[%s] ' "$(date '+%Y-%m-%dT%H:%M:%S%z')"
        printf '%q ' aws "${alb_options[@]}" "$@"
        printf '\n'
    } >> "$destination/commands.log"

    if aws "${alb_options[@]}" "$@" \
        > "$destination/$step.json" \
        2> "$destination/$step.stderr.log"; then
        rc=0
    else
        rc=$?
    fi

    printf 'exit_code=%s\n' "$rc" >> "$destination/commands.log"
    return "$rc"
}

# 接続先アカウントを記録する。認証できない場合は終了する。
if ! record_aws "$alb_output" identity sts get-caller-identity; then
    printf '認証情報を確認してください。証跡: %s\n' "$alb_output" >&2
    exit 2
fi

alb_account=$(jq -er '.Account' "$alb_output/identity.json") || exit 2

printf 'アカウント: %s\nリージョン: %s\n保存先: %s\n' \
    "$alb_account" "$alb_region" "$alb_output"

printf 'ALBName\tALBArn\tStackName\tStackId\tLogicalResourceId\tStatus\tTemplateFile\tDetail\n' \
    > "$alb_output/results.tsv"

# 1台分のALBについて、所属スタックとテンプレートを取得する。
collect_alb() {
    local found_ids
    local resource_state
    local stack_state
    local body_type

    # ALB名からARNを取得する。
    alb_status=ALB_LOOKUP_FAILED
    alb_detail='ALBの取得に失敗。エラーログを確認。'

    record_aws "$alb_dir" 01_alb \
        elbv2 describe-load-balancers \
        --names "$alb_name" || return 1

    if ! jq -e --arg name "$alb_name" \
        '(.LoadBalancers | length) == 1
         and .LoadBalancers[0].LoadBalancerName == $name
         and .LoadBalancers[0].Type == "application"' \
        "$alb_dir/01_alb.json" >/dev/null; then

        alb_status=NOT_AN_ALB
        alb_detail='指定名がApplication Load Balancerであることを確認できない。'
        return 1
    fi

    alb_arn=$(jq -er \
        '.LoadBalancers[0].LoadBalancerArn' \
        "$alb_dir/01_alb.json") || return 1

    # ARNから所属するCloudFormationスタックを調べる。
    alb_status=STACK_LOOKUP_UNCONFIRMED
    alb_detail='所属スタック未確認。権限不足や管理外等の可能性はログで確認。'

    record_aws "$alb_dir" 02_stack_lookup \
        cloudformation describe-stack-resources \
        --physical-resource-id "$alb_arn" || return 1

    found_ids=$(jq -c \
        '[.StackResources[].StackId | select(type == "string")] | unique' \
        "$alb_dir/02_stack_lookup.json") || return 1

    if [[ $(jq 'length' <<< "$found_ids") != 1 ]]; then
        return 1
    fi

    alb_stack=$(jq -r '.[0]' <<< "$found_ids") || return 1

    # スタック名・状態・親子関係を取得する。
    alb_status=STACK_DETAILS_FAILED
    alb_detail='スタックの状態を取得できない。'

    record_aws "$alb_dir" 03_stack \
        cloudformation describe-stacks \
        --stack-name "$alb_stack" || return 1

    alb_stack_name=$(jq -er \
        '.Stacks[0].StackName' \
        "$alb_dir/03_stack.json") || return 1

    stack_state=$(jq -er \
        '.Stacks[0].StackStatus' \
        "$alb_dir/03_stack.json") || return 1

    # 削除関連のスタックは、現在管理されているものと断定しない。
    if [[ $stack_state == DELETE_* ]]; then
        alb_status=DELETING_OR_DELETED_STACK
        alb_detail="スタック状態: $stack_state。現在の管理状態は要確認。"
        return 1
    fi

    # 全リソース一覧から対象ALBを照合する。
    # CLIの自動ページングを利用し、100件を超えるスタックにも対応する。
    alb_status=RESOURCE_UNCONFIRMED
    alb_detail='スタック内で対象ALBの現在の登録を確認できない。'

    record_aws "$alb_dir" 04_resources \
        cloudformation list-stack-resources \
        --stack-name "$alb_stack" || return 1

    jq --arg arn "$alb_arn" \
        '[.StackResourceSummaries[]
          | select(
              .PhysicalResourceId == $arn
              and .ResourceType == "AWS::ElasticLoadBalancingV2::LoadBalancer"
            )]' \
        "$alb_dir/04_resources.json" \
        > "$alb_dir/alb_resource.json" || return 1

    if [[ $(jq 'length' "$alb_dir/alb_resource.json") != 1 ]]; then
        return 1
    fi

    alb_logical_id=$(jq -er \
        '.[0].LogicalResourceId' \
        "$alb_dir/alb_resource.json") || return 1

    resource_state=$(jq -er \
        '.[0].ResourceStatus' \
        "$alb_dir/alb_resource.json") || return 1

    if [[ $resource_state == DELETE_* ]]; then
        alb_detail="対象ALBのリソース状態: $resource_state。現在の管理状態は要確認。"
        return 1
    fi

    # 所属スタックの元テンプレートを取得する。
    # ネスト構造の場合は、ALBが所属する子スタックが対象。
    # 親・ルートのIDは03_stack.jsonに保存される。
    alb_status=TEMPLATE_FAILED
    alb_detail='テンプレートの取得または書き出しに失敗。'

    record_aws "$alb_dir" 05_template_response \
        cloudformation get-template \
        --stack-name "$alb_stack" \
        --template-stage Original || return 1

    body_type=$(jq -er \
        '.TemplateBody | type' \
        "$alb_dir/05_template_response.json") || return 1

    # TemplateBodyがJSONオブジェクトか文字列かを判定して書き出す。
    case "$body_type" in
        object)
            alb_template="$alb_dir/template.json"

            jq '.TemplateBody' \
                "$alb_dir/05_template_response.json" \
                > "$alb_template" || return 1
            ;;

        string)
            if jq -e \
                '.TemplateBody | fromjson | type == "object"' \
                "$alb_dir/05_template_response.json" \
                >/dev/null 2>&1; then

                alb_template="$alb_dir/template.json"
            else
                alb_template="$alb_dir/template.yaml"
            fi

            jq -r '.TemplateBody' \
                "$alb_dir/05_template_response.json" \
                > "$alb_template" || return 1
            ;;

        *)
            return 1
            ;;
    esac

    if [[ ! -s "$alb_template" ]]; then
        return 1
    fi

    alb_status=OK
    alb_detail="保存完了。スタック状態: $stack_state / リソース状態: $resource_state"
}

# 入力ファイルを1行ずつ処理する。
alb_total=0
alb_failed=0

while IFS= read -r alb_line || [[ -n $alb_line ]]; do

    # 入力ファイルのWindows改行・UTF-8 BOM・前後の空白を除去する。
    alb_line=${alb_line%$'\r'}
    alb_line=${alb_line#$'\xef\xbb\xbf'}
    alb_name="${alb_line#"${alb_line%%[![:space:]]*}"}"
    alb_name="${alb_name%"${alb_name##*[![:space:]]}"}"

    # 空行とコメント行を読み飛ばす。
    if [[ -z $alb_name || $alb_name == \#* ]]; then
        continue
    fi

    alb_total=$((alb_total + 1))
    printf -v alb_index '%03d' "$alb_total"

    alb_dir="$alb_output/$alb_index"
    mkdir "$alb_dir" || exit 2

    alb_arn=''
    alb_stack=''
    alb_stack_name=''
    alb_logical_id=''
    alb_template=''

    # ARNや不正な文字列が入力されていないか確認する。
    if [[ ! $alb_name =~ ^[A-Za-z0-9][A-Za-z0-9-]{0,31}$ || $alb_name == *- ]]; then

        alb_status=INVALID_NAME
        alb_detail='ALB名の形式を確認。ARNではなく名前を1行に1台記載する。'
        alb_failed=$((alb_failed + 1))

    elif ! collect_alb; then

        alb_failed=$((alb_failed + 1))
    fi

    # 成功・失敗の両方を対応表へ記録する。
    jq -rn \
        --arg name "$alb_name" \
        --arg arn "$alb_arn" \
        --arg stack_name "$alb_stack_name" \
        --arg stack "$alb_stack" \
        --arg logical "$alb_logical_id" \
        --arg status "$alb_status" \
        --arg template "$alb_template" \
        --arg detail "$alb_detail" \
        '[$name, $arn, $stack_name, $stack, $logical, $status, $template, $detail] | @tsv' \
        >> "$alb_output/results.tsv" || exit 2

    printf '[%s] %s: %s\n' "$alb_index" "$alb_name" "$alb_status"

done < "$alb_input"

# 最終結果を記録する。
printf 'Finished=%s\nTotal=%s\nFailedOrUnconfirmed=%s\n' \
    "$(date '+%Y-%m-%dT%H:%M:%S%z')" \
    "$alb_total" \
    "$alb_failed" \
    >> "$alb_output/run_info.txt"

printf '\n対象: %s件 / 保存成功: %s件 / 未確認・失敗: %s件\n結果: %s/results.tsv\n' \
    "$alb_total" \
    "$((alb_total - alb_failed))" \
    "$alb_failed" \
    "$alb_output"

if [[ $alb_total -eq 0 ]]; then
    printf '入力ファイルにALB名がありません。\n' >&2
    exit 2
fi

if [[ $alb_failed -gt 0 ]]; then
    exit 1
fi

exit 0
