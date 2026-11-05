# AWS CloudFormation ElastiCache ドリフト検知および仕様に関する公式ドキュメント

AWS CloudFormationのドリフト検知機能や、ElastiCacheリソースの仕様に関する公式ドキュメントの該当ページは以下の通りです。

- **[AWS CloudFormation ユーザーガイド - スタックとリソースのドリフト検知](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/detect-drift-stack.html)**
  - CloudFormationのドリフト検知がどのような仕組みでテンプレートと実環境の差分を比較・検出しているかの基本仕様が記載されています。

- **[AWS CloudFormation ユーザーガイド - 個別スタックリソースのドリフト検知](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/detect-drift-resource.html)**
  - リソースごとの期待値（テンプレート）と実環境（実際のプロパティ値）の差異を確認する方法や、変更されたプロパティがどのように表示されるかの解説があります。

- **[AWS::ElastiCache::ReplicationGroup テンプレートリファレンス](https://docs.aws.amazon.com/ja_jp/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-replicationgroup.html)**
  - ElastiCacheのレプリケーショングループ（Redis）で管理されるプロパティ（`EngineVersion` や `CacheParameterGroupName` など）の定義や、アップデート時の挙動仕様を確認できます。
