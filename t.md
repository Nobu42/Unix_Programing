# ElastiCacheのバージョン差がCloudFormationのドリフトに出ない場合

確認日：2026年11月6日

## AWS公式スキーマで確認した内容

AWS公式の東京リージョン向けスキーマを取得して確認したところ、`AWS::ElastiCache::ReplicationGroup`の以下の項目は、`writeOnlyProperties`に含まれていた。

| 項目 | 内容 |
|---|---|
| `EngineVersion` | Redisのエンジンバージョン |
| `CacheParameterGroupName` | Redisに適用するパラメータグループ名 |

出典：

- [公式スキーマの配布案内](https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/resource-type-schemas.html)
- [確認した東京リージョンの公式ZIP](https://schema.cloudformation.ap-northeast-1.amazonaws.com/CloudformationSchema.zip)

ZIP内の確認対象ファイルは、`aws-elasticache-replicationgroup.json`。

## writeOnlyPropertiesの意味

`writeOnlyProperties`は、CloudFormationから設定できる一方、リソースプロバイダーの読み取り処理では値を返さない項目を指す。

そのため、これらの項目について、ドリフト比較で差を確認できる前提にはできない。

出典：[AWS公式の定義](https://docs.aws.amazon.com/cloudformation-cli/latest/userguide/resource-type-schema.html#resource-type-schema-properties)

## 今回の状況との関係

以下の状態は起こり得る。

- CloudFormationに保存された定義：**6.2**
- ElastiCacheの実環境：**7.1**
- ドリフト結果：**差分なし**

**`IN_SYNC`でも、エンジンバージョンまで一致している証明にはならない。**

「テンプレートでバージョンを明示すれば、必ずドリフトで確認できる」という前提は置かない。明示されていても、CloudFormation側で読み取れない項目は比較できない。

## 確認方法

ドリフト結果だけで判断せず、以下を直接照合する。

| 確認項目 | 比較する内容 |
|---|---|
| エンジンバージョン | ElastiCacheの実バージョンと、CloudFormationの保存済みテンプレート・入力パラメータ |
| 適用パラメータグループ | ElastiCacheで実際に適用されているグループ名と、CloudFormation側の参照先 |

## 未確認事項

- ドリフト結果にRedis本体の行そのものが表示されない理由までは、このプロパティ制限だけでは確定できない。
- 今回確認したのは東京リージョン向けの公開スキーマ。現場のリージョンと使用中のリソースプロバイダースキーマは未確認。
- 公開スキーマは更新されるため、必要に応じて対象リージョンのスキーマを再確認する。

## 手順書への反映

新規テスト用の手順書には、バージョン差がドリフトとして検出されることを必須条件にせず、実設定とCloudFormationの定義を直接照合する確認を追加した。

[新規テスト用Redisの更新手順書](/Users/nobu/Documents/Codex/2026-10-03/cloudformation-aws-cloudformation-2-1-yaml/outputs/新規テスト用Redis_更新手順書_下書き.md)

# AWS CloudFormation ElastiCache ドリフト検知および仕様に関する公式ドキュメント

AWS CloudFormationのドリフト検知機能や、ElastiCacheリソースの仕様に関する公式ドキュメントの該当ページは以下の通りです。

- **[AWS CloudFormation ユーザーガイド - スタックとリソースのドリフト検知](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/detect-drift-stack.html)**
  - CloudFormationのドリフト検知がどのような仕組みでテンプレートと実環境の差分を比較・検出しているかの基本仕様が記載されています。

- **[AWS CloudFormation ユーザーガイド - 個別スタックリソースのドリフト検知](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/detect-drift-resource.html)**
  - リソースごとの期待値（テンプレート）と実環境（実際のプロパティ値）の差異を確認する方法や、変更されたプロパティがどのように表示されるかの解説があります。

- **[AWS::ElastiCache::ReplicationGroup テンプレートリファレンス](https://docs.aws.amazon.com/ja_jp/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-replicationgroup.html)**
  - ElastiCacheのレプリケーショングループ（Redis）で管理されるプロパティ（`EngineVersion` や `CacheParameterGroupName` など）の定義や、アップデート時の挙動仕様を確認できます。
