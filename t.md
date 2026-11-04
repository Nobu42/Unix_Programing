# ElastiCache変更後のCloudFormationドリフト確認

確認日: 2026-11-05 / 根拠: AWS公式ドキュメント

## 1. 今回の状況と結論

作業者からの報告内容。実環境・テンプレート・画面は未確認。

- CloudFormationでRedis 6.2、2シャード・レプリカ0のテスト環境を作成。
- Redis 7用パラメータグループのテンプレートを作成。
- コンソールからレプリカ1へ変更し、エンジンを7.1.0へ更新。
- ドリフト確認でパラメータグループの画面が表示され、差分なし。報告上は「実際の値」に6.2と表示。

**Redis本体を管理するスタックを選んでドリフト検出する手順は正しい。ただし、パラメータグループの差分なしは、Redis本体のバージョン・レプリカ数が一致している証明にはならない。**

「別リソースの結果を見ている可能性」はあるが、原因は未確定。6.2が表示される項目名とリソースタイプを確認する。

## 2. Redis本体とパラメータグループの違い

| リソースタイプ | 管理する対象・主な項目 |
| --- | --- |
| `AWS::ElastiCache::ReplicationGroup` | Redis本体。`EngineVersion`、`NumNodeGroups`、`ReplicasPerNodeGroup`、`CacheParameterGroupName`など |
| `AWS::ElastiCache::ParameterGroup` | パラメータグループ。`CacheParameterGroupFamily`、個々のパラメータ値など |

`CacheParameterGroupFamily`の`redis6.x`・`redis7`は対応するファミリーであり、実行中のエンジンバージョンではない。**「6.2」と「redis6.x」は同じ表示ではないため、読み替えずに項目名と値を記録する。**

旧パラメータグループ自体を変更していなければ、それが差分なしでも矛盾しない。Redisにどのグループが関連付いているかは別途確認する。

公式: [ReplicationGroupの定義・Properties](https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-replicationgroup.html)、[ParameterGroupの定義・Properties](https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-parametergroup.html)

## 3. コンソールでの確認手順

1. 対象AWSアカウント・リージョンでCloudFormationを開き、Redis本体を作成したスタックを選択する。
2. 「リソース」で`AWS::ElastiCache::ReplicationGroup`を探す。物理IDがElastiCacheで7.1.0になっている対象と一致するか確認する。
3. パラメータグループしかない場合、Redis本体を管理するスタックを探す。ネストされたスタックの場合は、Redis本体が存在する子スタックで検出する。
4. ElastiCache側の変更完了を確認し、「スタックのアクション」→「ドリフトの検出」を実行する。
5. 検出完了と最終チェック日時を確認してから、「スタックのアクション」→「ドリフト結果を表示」を開く。
6. リソースごとの論理ID・物理ID・検出日時・ステータスを確認する。`ParameterGroup`の行だけで判断しない。
7. `MODIFIED`の対象行を選び「ドリフトの詳細を表示」で期待値・現在値を比較する。これはAWS公式に記載された詳細表示手順。物理IDリンクからElastiCache側へ移動する操作とは区別する。

公式: [スタック全体のドリフト検出 / AWS Management Consoleの手順](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/detect-drift-stack.html)

## 4. IN_SYNCの意味と限界

- `IN_SYNC`は、検出対象となった設定について差異がないという結果。すべての実設定の一致を保証するものではない。
- 原則としてテンプレートまたはスタックパラメータで明示した値を検出対象とする。省略した既定値は対象外。
- 取得・比較できないプロパティもある。対応リソースでも、全項目が検出されるとは限らない。
- 親スタックの検出では、ネストされた子スタック内部までは検出しない。

公式: [ドリフト検出の仕様 / What is drift?・状態コード・Considerations](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-stack-drift.html)

`ReplicationGroup`と`ParameterGroup`は、公式の対応表では両方ともドリフト検出対応。ただし、対応表だけでは今回の`EngineVersion`やレプリカ数が検出されなかった理由までは特定できない。

公式: [リソースタイプの対応表 / ElastiCache](https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/resource-import-supported-resources.html)

## 5. 解消しない場合

同一リソース・変更完了後・最新の検出結果でも不一致が残る場合は、次を保存して基盤担当またはAWSサポートへ確認する。

- スタック名、論理ID、物理ID、リソースタイプ
- 検出日時・完了状態、期待値と現在値の項目名・値
- デプロイ済みテンプレートとスタックパラメータ
- ElastiCache側の実バージョン・各シャードのレプリカ数・適用中パラメータグループ

**調査のために旧テンプレートを再適用しない。** エンジンの旧バージョンへのダウングレードは非対応。ドリフト結果と実環境を確認したうえで、変更内容と手順をレビューする。

公式: [ReplicationGroup / EngineVersionの制約](https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-replicationgroup.html#cfn-elasticache-replicationgroup-engineversion)
