# ネストスタック更新：AWS公式ドキュメント対応表

| 今回の方法 | 公式ドキュメント | 確認箇所 | URL |
|---|---|---|---|
| 子テンプレートをS3に配置し、親の`TemplateURL`から参照する | AWS::CloudFormation::Stack | Properties → TemplateURL | [公式ページ](https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-cloudformation-stack.html) |
| 修正した親テンプレートで変更セットを作成する | 変更セットの作成手順 | Create a change set (console) | [公式ページ](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-updating-stacks-changesets-create.html) |
| 親から子の変更セットまで確認する | ネストされた変更セット | Working with change sets for nested stacks (console) | [公式ページ](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/change-sets-for-nested-stacks.html) |

# 子テンプレートを修正し、親スタックから変更セットを作成する手順

確認日：2026年11月12日

## 1. 対象と現在の状況

| 識別名 | 内容 |
|---|---|
| 6 | 親となるルートスタックのYAML |
| 6-1 | ReplicationGroupの子テンプレート |
| 6-2 | ParameterGroupの子テンプレート |

現在の状況：

- 子スタック6-1を選択し、修正したローカルファイルをアップロードして変更セットを作成した。
- 変更セットは未実行。
- ReplacementがFalseであることを確認した。
- リーダから、親スタック6を起点に変更セットを作成するよう指示された。

今回行うこと：

変更版6-1をS3へ配置し、親6から参照する。その親6を使い、ネストされた子を含む変更セットを作成する。

AWS公式は、ネストスタックの更新を親から行うことを強く推奨している。[S1]

## 2. 作業の流れ

変更版6-1をS3へアップロード
    ↓
親6の、6-1を参照するTemplateURLを変更
    ↓
親スタック6を選択して変更セットを作成
    ↓
親6と子6-1・6-2の変更内容を確認
    ↓
確認した親の変更セットから実行

ここで説明するのは、子テンプレートの配布と、親から変更セットを作成する方法。
6-1の設定内容が正しいかどうかは、実環境・入力パラメータと別途照合する。

## 3. 変更版6-1をS3へアップロードする

1. ローカルの変更版6-1が、今回レビューするファイルであることを確認する。
2. 現場でテンプレートの配布に使用しているS3バケットを開く。
3. 現場の権限・配布ルールに従い、変更版6-1をアップロードする。
4. アップロードしたオブジェクトのHTTPS URLを記録する。
5. S3上のファイル内容が、変更版6-1と一致することを確認する。

旧版と区別できる別のオブジェクトキーを使う方法を推奨する。
これは変更内容を確認しやすくするための提案で、AWSの必須条件ではない。

例：

- 旧版：nested/6-1.yml
- 変更版：nested/6-1-reconcile.yml

S3オブジェクトを公開する必要はない。CloudFormationが読み取れる権限は、現場の既存構成に従って確認する。

根拠：
AWS::CloudFormation::StackのTemplateURLは、S3内のテンプレートを指すHTTPS URLとして定義されている。[S1]

## 4. 親6のTemplateURLを変更する

1. 親6のResourcesから、6-1に対応するAWS::CloudFormation::Stackを探す。
2. そのPropertiesにあるTemplateURLを確認する。
3. 変更版6-1のS3 HTTPS URLを参照するように変更する。

変更箇所の例：

```yaml
TemplateURL: https://example-bucket.s3.ap-northeast-1.amazonaws.com/nested/6-1-reconcile.yml
```

上記URLは説明用。実環境のURLへ置き換える。

TemplateURLをパラメータや!Sub等で組み立てている場合は、組み立て元の値を確認して変更する。
共通の入力値を変更すると、6-2など他の子の参照先まで変わる可能性があるため、変更範囲を確認する。

今回のExportエラー解消方針では、次を維持する。

- 6-2の旧パラメータグループ。
- 6-2の旧Family。
- 旧Exportの名前と値。

6-2に渡すFamilyをredis7へ変更する操作は、今回の整合化に混ぜない。

重要：
親6をWebコンソールへアップロードしても、ローカルの6-1が自動的に同梱されるわけではない。
CloudFormationが使う子テンプレートは、親のTemplateURLが参照するファイルである。[S1]

## 5. 親6から変更セットを作成する

1. AWSアカウントとリージョンを確認する。
2. CloudFormationで親スタック6を選択する。
3. 「スタックアクション」から変更セットを作成する操作を選ぶ。
4. 更新するテンプレートとして、修正した親6をローカルからアップロードする。
5. 入力パラメータを確認する。
6. ネストされたスタックを含む変更セットの設定が有効であることを確認する。
7. 内容を確認し、変更セットの作成を完了する。
8. 作成完了後も、レビューが終わるまで実行しない。

アップロードするのは親テンプレート6。
子テンプレート6-1を、親テンプレートの代わりとしてアップロードしない。

AWS公式の変更セット作成手順には、ローカルファイルを選択する操作と、ネストされた変更セットを有効にする設定が記載されている。[S3]

## 6. 親と子の変更内容を確認する

親6の変更セットから、子6-1の変更セットを開いて確認する。[S2]

| 確認対象 | 確認する内容 |
|---|---|
| 親6 | 6-1のTemplateURLが変更版を参照している |
| 6-1への入力 | エンジンバージョンとグループ名が意図した値 |
| 6-1内のReplicationGroup | 置換・削除がなく、変更対象のプロパティが想定どおり |
| 6-2 | 旧グループのFamily変更・置換・削除がない |
| 旧Export | 名前・値を変更しない |
| その他 | 今回対象外の変更がない |

親のAWS::CloudFormation::Stack行だけで確認を終えない。
子6-1内のReplicationGroupまで確認する。

AWS公式では、TemplateURLの更新による中断の有無は、子に含まれる更新対象リソースによると説明している。[S1]
したがって、親や子のReplacement: Falseだけで無停止を保証することはできない。

## 7. 実行する場合

1. 作業者と確認者で、実行する親の変更セットを照合する。
2. 親6から作成したルートの変更セットを実行する。
3. 親と関連子スタックのイベントを確認する。
4. 更新完了後、Redisの実バージョン・適用グループ・動作を確認する。

AWS公式では、階層化された変更セットの実行は、ルートの変更セットから行うと説明している。[S2]

## 8. 現在作成済みの子変更セットの扱い

今回、子6-1を直接選択して作成した変更セットは、まだ実行しない。

その変更内容は確認用として記録し、親6から新しい変更セットを作成する。

不要になった変更セットを削除する場合は、次を区別する。

- 子6-1を起点に単独で作成した変更セット。
- 親から作成された階層内の子変更セット。

親から作成された階層内の変更セットは、削除もルートの変更セットから行う。[S2]

## 9. 別の方法：AWS CLIのpackageを使用する

AWS公式には、ローカルの子テンプレートをS3へアップロードし、親のTemplateURLをS3の参照へ書き換えるpackageの使用例がある。[S4]

流れ：

1. パッケージ化する親テンプレートのTemplateURLに、ローカルの子ファイルのパスを指定する。
2. aws cloudformation packageを実行する。
3. 子ファイルがS3へアップロードされる。
4. S3参照へ書き換えられた親テンプレートが出力される。
5. その出力された親テンプレートを使い、親6から変更セットを作成する。

packageは配布用テンプレートを生成する処理であり、それ自体でスタック更新を実行するものではない。

今回はWebコンソール中心の作業であるため、まずは3～7章の方法を候補とする。
CLIの利用が現場で認められ、配布方法を統一できる場合はpackageも検討できる。

## 10. AWS公式URLと、どこを読むか

### [S1] 親からの更新とTemplateURLの仕様

URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-cloudformation-stack.html

確認箇所①：
ページ冒頭のAWS::CloudFormation::Stackの説明。

確認できること：
- ネストスタックの更新を親から行うことを強く推奨している。
- 親の更新によって、変更のある子のリソースが更新される。

確認箇所②：
「Properties」内の「TemplateURL」。

確認できること：
- 子テンプレートのURLを指定する項目である。
- S3バケット内のテンプレートを指すHTTPS URLが必要。
- 更新による中断の有無は、更新対象のリソースによる。

ページ内検索：
TemplateURL

### [S2] ネストされた変更セットの作成・確認・実行

URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/change-sets-for-nested-stacks.html

確認する見出し：
Working with change sets for nested stacks (console)

確認できること：
- コンソールでは、ネストされた変更セットが既定で有効。
- 親の変更セットから子の変更セットをたどって確認できる。
- 階層化された変更セットの実行・削除は、ルートの変更セットから行う。

見出し直下のNoteに、ルートの変更セットに関する説明がある。

ページ内検索：
root change set

### [S3] Webコンソールでの変更セット作成操作

URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-updating-stacks-changesets-create.html

確認する箇所：
Create a change set (console)の操作手順。

確認できること：
- 対象スタックを選択し、Stack actionsから変更セットを作成する。
- ローカルファイルはUpload a template fileで選択する。
- スタックオプションの説明に、ネストされた変更セットの有効・無効の設定がある。

ページ内検索：
Upload a template file

### [S4] 子テンプレートをS3へ配置する公式の自動化例

URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-nested-stacks.html

確認箇所①：
Step 3: Package and deploy the templates

確認できること：
- packageでローカルの子テンプレートをS3へアップロードする。
- 親のTemplateURLがS3の場所へ書き換えられる。
- 書き換え後の親テンプレートが出力される。

確認箇所②：
Performing stack operations on nested stacks

確認できること：
- スタック更新などの操作は、子を直接操作するのではなく、ルートスタックから開始する方針。

ページ内検索：
package
