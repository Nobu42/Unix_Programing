# CloudTrail証跡ログ停止可否の調査方針・手順

## リーダーへの報告：調査方針

対象4証跡について、既存のログ記録を停止しても、組織証跡で必要なログの記録・閲覧・監視を継続できるか確認します。

- 組織側のログを当方で閲覧できるかを優先して確認します。
- 既存証跡と組織証跡の記録対象を比較し、停止によるログ欠落がないか確認します。
- 監視・通知・CodePipelineへの影響と、ログ容量・費用・保存期間を確認します。
- 調査結果を証跡ごとに整理し、停止可否と実施前に必要な対応を報告します。

今回の作業は参照調査です。証跡の停止・削除、設定変更、動作試験のためのパイプライン起動は行いません。

---

## 1. 依頼内容・対象・現時点の状況

- 作成日：2026年11月26日
- チケット：CloudTrail証跡のログ停止依頼
- 依頼内容：Control Towerで組織証跡を有効化したため、重複する既存証跡のログ記録を停止できるか確認する。
- 継続利用する組織証跡：`aws-controltower-BaselineCloudTrail`
- 現時点：公式仕様に基づく調査手順を整理済み。実環境での取得結果は未確認であり、停止可否は未判定。

| アカウント名 | 停止検討対象の既存証跡 | 停止可否 | 個別の確認事項 |
|---|---|---|---|
| TPA_AWS(Linked) | EfsTrl001 | 未判定 | 記録対象・閲覧・監視連携 |
| FCL_AWS(Linked) | codepipeline-source-trail | 未判定 | CodePipelineのソース変更検知との関係を重点確認 |
| FCL_AWS(Linked) | ProdFclCloudTrail | 未判定 | 記録対象・閲覧・監視連携 |
| WOS_AWS(Linked) | FclTrl001 | 未判定 | 記録対象・閲覧・監視連携 |

証跡名から用途や対象サービスを確定しません。アカウントID・証跡ARN・HomeRegionと実際の設定を確認します。

Control Tower導入後も独自の証跡が残る場合、重複料金が発生する可能性があります。ただし、組織証跡の存在だけでは記録範囲や後続処理が同等であるとは判断できません。

根拠：[AWS Control Towerでのログ記録](https://docs.aws.amazon.com/ja_jp/controltower/latest/userguide/about-logging.html)、[組織証跡の作成](https://docs.aws.amazon.com/ja_jp/awscloudtrail/latest/userguide/creating-trail-organization.html)

## 2. 調査の進め方

| 順番 | 調査項目 | 完了条件 |
|---|---|---|
| 1 | 対象と組織証跡の特定 | ARN・HomeRegion・保存先が分かる |
| 2 | 記録・配信状態 | 有効状態と対象アカウントの実ログ到着を確認できる |
| 3 | 記録範囲の比較 | 既存側で必要なイベントが組織側に含まれるか分かる |
| 4 | 組織ログの閲覧 | 実際の運用ロールで対象ログの本文を読める、または運用上認められた提供経路が確定する |
| 5 | 後続処理への影響 | CodePipeline・監視・外部収集などの継続方法が確認できる |
| 6 | 容量・費用・保存期間 | 削減対象と停止後も残る費用・保存義務が整理できる |
| 7 | 設定管理・停止方針 | 管理者・変更方法・承認事項・再開方法が整理できる |

**優先順位は1〜4です。FCLのcodepipeline-source-trailについては5も並行して確認します。**

## 3. CLIの共通準備

### 3.1 実行上の前提

- Bash、AWS CLI v2、jq、gzipを使用します。PowerShellにそのまま貼り付ける構文ではありません。
- 現場で認められた参照用認証を使用します。アカウント名はAWS CLIのプロファイル名とは限りません。
- 値が未確定の変数は、管理資料または前段の取得結果から設定します。
- コードブロックは確認しながら順番に実行します。エラーが出た箇所は、解消または管理側確認が済むまで後続の判定に使いません。
- 証跡ごとに保存先を分けます。FCLの2証跡についても、対象証跡名と保存先を切り替えて確認します。
- 本手順のローカル保存ファイルは、現場の情報管理ルールに従って取り扱います。

### 3.2 実行対象の指定

以下はTPAを調査する場合の記入例です。実際のアカウントID・プロファイル・リージョンに置換します。

```bash
# 対象アカウントの参照用プロファイルを指定する。
# CloudShellの現在の認証を使う場合は「-」を指定する。
CT_PROFILE='現場のTPA参照用プロファイル'
CT_ACCOUNT_ID='確認した12桁のアカウントID'
CT_REGION='ap-northeast-1'  # 東京の例。実際の調査対象リージョンに変更する。
CT_TRAIL_NAME='EfsTrl001'

# 証跡ごと・調査日ごとに保存場所を分ける。
CT_DIR='./cloudtrail-research-20261126/TPA_EfsTrl001'
umask 077
mkdir -p "$CT_DIR"

# AWS CLIの共通オプションと結果保存をまとめる参照用関数。
# 引数：保存ファイル、プロファイル、リージョン、AWSサービス名、操作名、各オプション。
# 失敗時は成功JSONを作らず、.error.txtに原因を残す。
ct_read() {
  local file=$1 profile=$2 region=$3
  shift 3
  local args=(--region "$region" --output json --no-cli-pager --no-cli-auto-prompt)
  [[ "$profile" == '-' ]] || args+=(--profile "$profile")
  if aws "${args[@]}" "$@" > "${file}.tmp" 2> "${file}.error.txt"; then
    mv "${file}.tmp" "$file"
    rm -f "${file}.error.txt"
    echo "取得成功: $file"
  else
    rm -f "${file}.tmp" "$file"
    echo "取得失敗: ${file}.error.txt を確認してください" >&2
    return 1
  fi
}

# 認証の実行先を確認する。AccountとArnを管理資料と照合する。
ct_read "$CT_DIR/identity.json" "$CT_PROFILE" "$CT_REGION" \
  sts get-caller-identity
jq '{Account,Arn}' "$CT_DIR/identity.json"
```

**判断**：`Account`が対象アカウントIDと一致し、`Arn`が想定する運用ロールであることを確認してから先へ進みます。不一致の場合はプロファイルを修正します。

`ct_read`はAWS CLIを実行する関数です。以降の各コマンドのサービス名・操作名・オプションは、そのままAWS CLIに渡されます。

根拠：[AWS CLI：get-caller-identity](https://docs.aws.amazon.com/cli/latest/reference/sts/get-caller-identity.html)

## 4. 既存証跡と組織証跡の特定

```bash
# 組織証跡のメンバーアカウント側コピーも含め、証跡設定を取得する。
# --include-shadow-trailsを外して除外すると、組織証跡を見落とす可能性がある。
ct_read "$CT_DIR/trails.json" "$CT_PROFILE" "$CT_REGION" \
  cloudtrail describe-trails --include-shadow-trails

# 比較する項目を読みやすく表示する。元のJSONは保存しておく。
jq '.trailList[] | {
  Name,TrailARN,HomeRegion,IsOrganizationTrail,IsMultiRegionTrail,
  IncludeGlobalServiceEvents,S3BucketName,S3KeyPrefix,KmsKeyId,
  CloudWatchLogsLogGroupArn,SnsTopicARN,HasInsightSelectors
}' "$CT_DIR/trails.json"

# 同名候補をARNで重複排除し、1件に特定できた場合だけ保存する。
# 0件・複数件の場合はエラーとなるため、リージョン・権限・名称を確認する。
jq -e --arg name "$CT_TRAIL_NAME" '
  [.trailList[] | select(.Name == $name)] | unique_by(.TrailARN) |
