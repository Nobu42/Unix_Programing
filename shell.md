# EFSライフサイクル調査用シェル

## 1. 作成するシェル

| ファイル名 | 役割 |
|---|---|
| extract_efs_targets.sh | EFS一覧のJSONからFileSystemIdとNameを抽出する。AWSには接続しない |
| collect_efs_research.sh | 抽出した一覧を使ってAWS CLIを実行し、調査情報をJSONファイルへ保存する |

調査シェルで取得する情報：

- 実行先AWSアカウントと実行主体
- EFSの現在設定・容量・タグ
- 現在のライフサイクル設定
- 所属CloudFormationスタックと親・ルートスタックの情報
- CloudFormationテンプレートとリソース一覧
- ストレージクラス別の容量推移
- 読み出し・書き込み量
- Bursting利用時の許容スループット・バーストクレジット
- 任意でCost ExplorerのEFS費用内訳

AWSの設定変更・スタック更新・変更セット作成は行わない。
調査結果のファイルを、実行端末に保存する。

## 2. 前提

- Bash、jq、AWS CLI v2を使用する。
- macOS・Linux・AWS CloudShellなどのBash向け。
- PowerShellに直接貼り付ける用途ではない。
- 現場で使用を認められた認証と参照権限を使用する。
- 対象一覧は、同じAWSアカウント・リージョンのEFSで構成する。
- 調査結果にはアカウント情報・構成情報が含まれるため、現場の管理場所へ保存する。

入力元は、次のコマンドのJSON出力。

```bash
aws efs describe-file-systems --output json
```

`describe-lifecycle-configuration` の応答には、
FileSystemIdとNameが含まれないため、抽出元には使用できない。

## 3. 一覧抽出シェル

次の内容を `extract_efs_targets.sh` として保存する。

```bash
#!/usr/bin/env bash
# describe-file-systemsのJSONからFileSystemIdとNameだけを抽出する。
# AWSには接続しない。結果は標準出力へJSON配列として出力する。
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "使い方: bash extract_efs_targets.sh describe-file-systems.json" >&2
  exit 1
fi
command -v jq >/dev/null || { echo "jqが必要です" >&2; exit 1; }

# 誤ったAPIの出力や、不正なEFS IDを受け付けない。
if ! jq -e '
  (.FileSystems | type == "array") and
  all(.FileSystems[];
    (.FileSystemId | type == "string") and
    (.FileSystemId | test("^fs-[0-9a-f]{8,40}$")))
' "$1" >/dev/null; then
  echo "describe-file-systems --output json の出力を指定してください。" >&2
  echo "describe-lifecycle-configurationの応答にはID・Nameはありません。" >&2
  exit 1
fi

# Name未設定は空文字。Nameタグも補助的に確認する。
# JSONを使うため、名前の空白・タブ・改行を壊さず受け渡せる。
jq '[.FileSystems[] | {
  FileSystemId,
  Name: (.Name // ([.Tags[]? | select(.Key == "Name") | .Value][0]) // "")
}] | unique_by(.FileSystemId)' "$1"
```

出力例：

```json
[
  {
    "FileSystemId": "fs-0123456789abcdef0",
    "Name": "example-efs"
  },
  {
    "FileSystemId": "fs-1234567890abcdef0",
    "Name": ""
  }
]
```

Nameが未設定でも、FileSystemIdを使って調査できる。

## 4. 調査情報取得シェル

次の内容を `collect_efs_research.sh` として保存する。

```bash
#!/usr/bin/env bash
# 対象一覧を読み、EFS設定・CloudWatch・CloudFormation情報を保存する。
# AWSの設定は変更しない。Cost ExplorerはINCLUDE_COST=1のときだけ取得。
# Bash、jq、AWS CLI v2が必要。空き容量のあるローカル作業場所で実行する。
set -euo pipefail

if [[ $# -lt 7 || $# -gt 8 ]]; then
  echo "使い方: bash collect_efs_research.sh 対象JSON リージョン アカウントID 開始日 終了日 出力先 プロファイル CW取得有無" >&2
  echo "例: bash collect_efs_research.sh targets.json ap-northeast-1 123456789012 2026-10-01 2026-11-01 result work-readonly 1" >&2
  echo "プロファイルに - を指定すると現在の認証を使用。CW取得有無は省略時1。" >&2
  exit 1
fi
TARGETS=$1; REGION=$2; EXPECTED_ACCOUNT=$3
START=$4; END=$5; OUT=$6; PROFILE=$7; WITH_CW=${8:-1}
INCLUDE_COST=${INCLUDE_COST:-0}

for tool in aws jq; do
  command -v "$tool" >/dev/null || { echo "$tool が必要です" >&2; exit 1; }
done
[[ "$EXPECTED_ACCOUNT" =~ ^[0-9]{12}$ ]] || {
  echo "アカウントIDは12桁です" >&2; exit 1;
}
[[ "$WITH_CW" =~ ^[01]$ && "$INCLUDE_COST" =~ ^[01]$ ]] || exit 1

# 日付と期間を検証。UTCの0時を境界とし、終了日は含めない。
# CloudWatchの日次傾向を見るため、期間は最大455日とする。
jq -en --arg s "$START" --arg e "$END" '
  def epoch: strptime("%Y-%m-%d") | mktime;
  ($s|epoch) as $a | ($e|epoch) as $b |
  (($a|strftime("%Y-%m-%d")) == $s) and
  (($b|strftime("%Y-%m-%d")) == $e) and
  ($b > $a) and (($b-$a) <= 455*86400)
' >/dev/null || {
  echo "開始日・終了日をYYYY-MM-DD形式で確認してください" >&2; exit 1;
}

# JSON配列のIDを検証。Nameは保存先名には使用しない。
jq -e 'type == "array" and length > 0 and
  all(.[]; (.FileSystemId | type == "string") and
    (.FileSystemId | test("^fs-[0-9a-f]{8,40}$")))' "$TARGETS" >/dev/null || {
  echo "対象JSONが不正、または対象が0件です" >&2; exit 1;
}

AWS_ARGS=(--output json --no-cli-pager --no-cli-auto-prompt)
[[ "$PROFILE" == "-" ]] || AWS_ARGS+=(--profile "$PROFILE")

# 最初に実行先アカウントを照合。不一致なら後続の参照を実行しない。
IDENTITY=$(aws "${AWS_ARGS[@]}" --region "$REGION" sts get-caller-identity)
[[ $(jq -r '.Account' <<< "$IDENTITY") == "$EXPECTED_ACCOUNT" ]] || {
  echo "実行先アカウントが指定値と一致しません" >&2; exit 1;
}

# 新規ディレクトリに保存し、以前の調査結果を上書きしない。
umask 077
mkdir "$OUT"
mkdir "$OUT/stacks"
printf '%s\n' "$IDENTITY" > "$OUT/identity.json"
cp "$TARGETS" "$OUT/targets.json"
printf 'Status\tFile\n' > "$OUT/status.tsv"

jq -n --arg region "$REGION" --arg start "$START" --arg end "$END" \
  --arg cw "$WITH_CW" --arg cost "$INCLUDE_COST" \
  '{Region:$region,Start:$start,End:$end,CloudWatch:$cw,CostExplorer:$cost}' \
  > "$OUT/request.json"
ISSUES=0

# AWS CLIの共通ラッパー。失敗はerror.txtへ保存し、他の調査を続ける。
# 空のCloudWatch結果をゼロと扱わずEMPTYとして記録する。
fetch() {
  local file=$1 region=$2 status
  shift 2
  if aws "${AWS_ARGS[@]}" --region "$region" "$@" \
      > "$file.tmp" 2> "$file.error.txt"; then
    mv "$file.tmp" "$file"
    rm -f "$file.error.txt"
    status=OK

    if jq -e 'has("Datapoints") and (.Datapoints | length == 0)' \
        "$file" >/dev/null; then
      status=EMPTY
      ISSUES=1
    fi

    printf '%s\t%s\n' "$status" "${file#"$OUT/"}" >> "$OUT/status.tsv"
    return 0
  fi

  rm -f "$file.tmp"
  printf 'ERROR\t%s\n' "${file#"$OUT/"}" >> "$OUT/status.tsv"
  echo "取得失敗: $file.error.txt" >&2
  ISSUES=1
  return 1
}

# 所属スタックからParentIdをたどり、ルートまで参照する。
# 同じスタックのinfo.json取得済みなら再取得しない。
# テンプレート等の取得失敗はstatus.tsvで確認する。
collect_stacks() {
  local stack=$1 dir
  while [[ -n "$stack" ]]; do
    dir="$OUT/stacks/${stack##*/}"
    mkdir -p "$dir"

    if [[ ! -f "$dir/info.json" ]]; then
      fetch "$dir/info.json" "$REGION" cloudformation describe-stacks \
        --stack-name "$stack" || break
      fetch "$dir/template.json" "$REGION" cloudformation get-template \
        --stack-name "$stack" --template-stage Original || true
      fetch "$dir/resources.json" "$REGION" cloudformation list-stack-resources \
        --stack-name "$stack" || true
    fi

    stack=$(jq -r '.Stacks[0].ParentId // empty' "$dir/info.json")
  done
}

# 入力一覧の各EFSについて情報を取得する。
while IFS= read -r id; do
  dir="$OUT/$id"
  mkdir "$dir"
  echo "調査中: $id"

  # 設定・容量・タグ、および現在のライフサイクル設定。
  fetch "$dir/filesystem.json" "$REGION" efs describe-file-systems \
    --file-system-id "$id" || true
  fetch "$dir/lifecycle.json" "$REGION" efs describe-lifecycle-configuration \
    --file-system-id "$id" || true

  # タグから所属スタックを確認。得られない場合は物理IDで検索する。
  stack=""
  if [[ -f "$dir/filesystem.json" ]]; then
    stack=$(jq -r '[.FileSystems[0].Tags[]? |
      select(.Key == "aws:cloudformation:stack-id") | .Value][0] // empty' \
      "$dir/filesystem.json")
  fi

  if [[ -z "$stack" ]]; then
    if fetch "$dir/stack-lookup.json" "$REGION" cloudformation describe-stack-resources \
        --physical-resource-id "$id"; then
      stack=$(jq -r '[.StackResources[].StackId] | unique |
        if length == 1 then .[0] else empty end' "$dir/stack-lookup.json")
    fi
  fi

  if [[ -n "$stack" ]]; then
    printf '%s\n' "$stack" > "$dir/stack-id.txt"
    collect_stacks "$stack"
  else
    printf 'UNRESOLVED\t%s/CloudFormation\n' "$id" >> "$OUT/status.tsv"
    ISSUES=1
  fi

  # 引数でCloudWatch取得を無効化した場合は、次のEFSへ進む。
  [[ "$WITH_CW" == 1 ]] || continue

  CW_ARGS=(--namespace AWS/EFS --start-time "${START}T00:00:00Z"
    --end-time "${END}T00:00:00Z" --period 86400
    --query '{Label:Label,Datapoints:sort_by(Datapoints,&Timestamp)}')

  # クラス別の日平均容量。IA/Archiveの丸め込み分も別に取得する。
  for class in Total Standard IA IASizeOverhead Archive ArchiveSizeOverhead; do
    fetch "$dir/storage-$class.json" "$REGION" cloudwatch get-metric-statistics \
      "${CW_ARGS[@]}" --metric-name StorageBytes --statistics Average \
      --dimensions "Name=FileSystemId,Value=$id" "Name=StorageClass,Value=$class" || true
  done

  # 1日あたりの読み出し・書き込み総量。単位はBytes。
  for metric in DataReadIOBytes DataWriteIOBytes; do
    fetch "$dir/$metric.json" "$REGION" cloudwatch get-metric-statistics \
      "${CW_ARGS[@]}" --metric-name "$metric" --statistics Sum \
      --dimensions "Name=FileSystemId,Value=$id" || true
  done

  # Burstingの場合だけ、許容スループットとクレジットも確認する。
  if [[ -f "$dir/filesystem.json" ]] &&
      [[ $(jq -r '.FileSystems[0].ThroughputMode' "$dir/filesystem.json") == bursting ]]; then
    for metric in PermittedThroughput BurstCreditBalance; do
      fetch "$dir/$metric.json" "$REGION" cloudwatch get-metric-statistics \
        "${CW_ARGS[@]}" --metric-name "$metric" --statistics Minimum Average Maximum \
        --dimensions "Name=FileSystemId,Value=$id" || true
    done
  fi
done < <(jq -r '.[].FileSystemId' "$TARGETS" | sort -u)

# 任意の費用取得。
# 特定EFS IDではなく、アカウント・リージョンのEFS合計を取得する。
# Cost Explorerは参照にも料金が発生する。
# ページが残る間は順に取得し、別ファイルとして保存する。
if [[ "$INCLUDE_COST" == 1 ]]; then
  FILTER=$(jq -cn --arg account "$EXPECTED_ACCOUNT" --arg region "$REGION" '{And:[
    {Dimensions:{Key:"SERVICE",Values:["Amazon Elastic File System"]}},
    {Dimensions:{Key:"LINKED_ACCOUNT",Values:[$account]}},
    {Dimensions:{Key:"REGION",Values:[$region]}}
  ]}')

  token=""
  page=1
  while :; do
    CE_ARGS=(ce get-cost-and-usage --time-period "Start=$START,End=$END"
      --granularity MONTHLY --metrics UnblendedCost --filter "$FILTER"
      --group-by Type=DIMENSION,Key=USAGE_TYPE)

    [[ -z "$token" ]] || CE_ARGS+=(--next-page-token "$token")
    file="$OUT/cost-page-$page.json"

    fetch "$file" us-east-1 "${CE_ARGS[@]}" || break
    token=$(jq -r '.NextPageToken // empty' "$file")
    [[ -n "$token" ]] || break
    page=$((page+1))
  done
fi

echo "保存先: ${OUT}（status.tsvで取得結果を確認してください）"

# 0: API取得完了。
# 2: エラー・空メトリクス・所属スタック未特定あり。
# 2は必ずしも障害を意味しない。未使用クラスのメトリクス等を確認する。
[[ "$ISSUES" == 0 ]] || exit 2
```

## 5. 実行方法

### 5.1 EFS一覧を取得する

以下のプロファイルとリージョンは例。
現場の値へ置き換える。

既に `describe-file-systems --output json` の出力を保存している場合、
一覧の再取得は不要。

```bash
# 調査に使用する認証とリージョンを指定する。
EFS_PROFILE='work-readonly'
EFS_REGION='ap-northeast-1'

# アカウントIDと実行主体を確認する。
aws sts get-caller-identity \
  --profile "$EFS_PROFILE" \
  --region "$EFS_REGION" \
  --output json \
  --no-cli-pager

# 対象アカウント・リージョンのEFS一覧を保存する。
# --queryによる加工はせず、FileSystems配列を含むJSONを取得する。
aws efs describe-file-systems \
  --profile "$EFS_PROFILE" \
  --region "$EFS_REGION" \
  --output json \
  --no-cli-pager \
  > describe-file-systems.json
```

CloudShellの既存認証を使用する場合は、
上記の `--profile "$EFS_PROFILE"` を省く。

### 5.2 IDとNameを抽出する

```bash
# 一覧JSONから、調査シェルに渡す対象一覧を作る。
bash extract_efs_targets.sh describe-file-systems.json > targets.json

# 調査対象を確認する。
jq . targets.json
```

調査シェルは `targets.json` 内のすべてのEFSを対象にする。
対象外のEFSがある場合は、その要素を一覧から除外する。

### 5.3 調査シェルの引数

| 順番 | 内容 | 例 |
|---|---|---|
| 1 | 抽出した対象一覧 | targets.json |
| 2 | 対象リージョン | ap-northeast-1 |
| 3 | 対象AWSアカウントID | 123456789012 |
| 4 | 調査開始日 | 2026-10-01 |
| 5 | 調査終了日。この日は含まない | 2026-11-01 |
| 6 | 新規の出力先ディレクトリ | efs-result-20261126 |
| 7 | プロファイル。現在の認証を使う場合は「-」 | work-readonly |
| 8 | CloudWatch取得。1＝取得、0＝省略 | 1 |

CloudWatchの時刻はUTC。
開始日・終了日はメトリクスとCost Explorerに使用する。
EFS設定とCloudFormation情報は、実行時点の情報を取得する。

出力先ディレクトリは、まだ存在しない名前を指定する。
親ディレクトリは存在している必要がある。

### 5.4 設定・構成・CloudWatchを取得する

```bash
# 2026年10月分のメトリクスと、現在の設定・構成を取得する例。
# アカウントID・リージョン・プロファイルを現場の値へ置き換える。
# INCLUDE_COST=0を明示し、Cost Explorerを呼び出さない。
INCLUDE_COST=0 bash collect_efs_research.sh \
  targets.json \
  ap-northeast-1 \
  123456789012 \
  2026-10-01 \
  2026-11-01 \
  efs-result-20261126 \
  work-readonly \
  1
```

CloudShellの現在の認証で実行する場合は、
`work-readonly` の部分を `-` に置き換える。

設定・構成だけを確認したい場合は、最後の `1` を `0` にする。

### 5.5 費用も取得する場合

Cost Explorerが利用可能で、費用参照権限がある場合に使用する。
参照APIには利用料金が発生する。

```bash
# 2026年9月・10月の費用を月別・利用タイプ別で取得する例。
# EFS設定・CloudFormation・CloudWatchも取得する。
# 前の結果を上書きしないよう、別の出力先を指定する。
INCLUDE_COST=1 bash collect_efs_research.sh \
  targets.json \
  ap-northeast-1 \
  123456789012 \
  2026-09-01 \
  2026-11-01 \
  efs-result-with-cost-20261126 \
  work-readonly \
  1
```

## 6. 出力ファイルの見方

| ファイル | 調査で確認する内容 |
|---|---|
| identity.json | 実行先アカウントと実行主体 |
| request.json | 指定リージョン・調査期間・取得オプション |
| targets.json | 調査に使用したID・Name一覧 |
| status.tsv | 各情報の取得結果 |
| EFS ID/filesystem.json | 現在の容量・性能モード・スループットモード・タグ |
| EFS ID/lifecycle.json | 現在の移行日数・Standard復帰条件 |
| EFS ID/stack-id.txt | 所属するCloudFormationスタック |
| EFS ID/stack-lookup.json | 物理IDによるスタック検索結果。検索した場合のみ |
| EFS ID/storage-*.json | クラス別の日平均容量と丸め込み分 |
| EFS ID/DataReadIOBytes.json | 日別の読み出し総量 |
| EFS ID/DataWriteIOBytes.json | 日別の書き込み総量 |
| EFS ID/PermittedThroughput.json | Bursting利用時の許容スループット |
| EFS ID/BurstCreditBalance.json | Bursting利用時のクレジット |
| stacks/スタック識別子/info.json | スタック状態・ParentId・RootId |
| stacks/スタック識別子/template.json | 提出されたテンプレート |
| stacks/スタック識別子/resources.json | スタック内のリソース一覧 |
| cost-page-*.json | 月別・利用タイプ別のEFS費用。任意取得 |
| *.error.txt | 参照に失敗したAPIのエラー内容 |

### status.tsvの判定

| 状態 | 意味 |
|---|---|
| OK | API呼び出しに成功 |
| EMPTY | CloudWatchのデータポイントが空 |
| ERROR | API呼び出しに失敗。error.txtを確認 |
| UNRESOLVED | 所属CloudFormationスタックを特定できなかった |

`EMPTY` は必ずしも問題を意味しない。
未使用のIA／Archiveなどで、データが返らない場合もある。
容量ゼロやアクセスゼロと自動的に判断しない。

`UNRESOLVED` は「CloudFormation管理外」の確定ではない。
権限・対象リージョン・タグ・管理資料も確認する。

## 7. 調査上の限界

- 保存容量とI/Oの集計だけでは、30日以上未アクセスの容量は確定できない。
- 日次集計は傾向把握用。短時間のピーク分析には別の集計が必要。
- 費用取得は対象アカウント・リージョンのEFS合計であり、個別EFS IDの費用ではない。
- Cost Explorerの当月データは途中集計であり、反映遅延や見積値も確認する。
- テンプレート取得だけでは、実環境との一致やドリフトの有無は確定できない。
- CloudFormationテンプレートなどの参照権限がない場合も、取得できた他の情報は保存する。
- 移行日数は未定として扱い、利用状況と費用試算を踏まえて決める。

## 8. 動作確認状況

AWSへ接続しない模擬環境で、以下を確認済み。

- Bashの構文
- FileSystemId・Nameの抽出
- Name未設定や空白・改行を含む名前
- 誤ったAPIの応答や不正JSONの拒否
- アカウント不一致時の停止
- 一部APIが権限エラーでも他の取得を継続すること
- 空メトリクスの記録
- 共有する親・ルートスタックの取得
- Cost Explorerの任意実行と複数ページ取得

現場AWSでの実行結果は未確認。
