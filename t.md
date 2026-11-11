# Redis 7.1更新後のCloudFormation整合化方針
確認日：2026年11月12日

## 1. 今回採用を勧める方法

稼働中のRedis 7.1と、現在使用しているAWS提供のデフォルトパラメータグループを維持する。

旧カスタムパラメータグループとExportは変更せず、先にRedis本体側のCloudFormation定義を実環境に合わせる。旧グループとExportの整理は、依存関係を解消した後の別作業に分ける。

これはAWS公式の制約・解決手順を踏まえた、今回の構成に対する提案である。AWSがこの個別構成について更新成功や無停止を保証しているという意味ではない。

前提条件：
- 現在のデフォルトパラメータグループを継続利用することが、現場の運用方針として認められる。
- 独自パラメータグループを使うことが必須ではない。
- 現在の実環境と、親・子テンプレートの参照関係を確認してから実施する。

## 2. 現場から共有された状況

以下はAWS公式資料ではなく、現場の状況として共有された情報。

- Redisの6.2.6から7.1.0への更新は、Webコンソールから実施して成功した。
- 現在は、クラスターモード有効のAWS提供デフォルトパラメータグループを使用している。
- CloudFormationは、ルートスタックと、Redis本体・パラメータグループの子スタックで構成されている。
- 旧パラメータグループ名はExportされ、別のスタックからImportされている。
- 親テンプレートでEngineVersionとパラメータグループFamilyを変更したところ、パラメータグループの置換が計画された。
- 更新を実行すると、Exportが使用中である旨のエラーが発生し、CloudFormationがロールバックした。

エラー全文と実際のテンプレートは本資料では未確認。そのため、失敗原因の最終確定にはCloudFormationイベントとの照合が必要。

## 3. この方法を選んだ根拠

### 根拠① ImportされているExport値は変更できない

AWS公式では、別スタックにImportされている間、Export値の変更やExport元スタックの削除はできないと説明されている。

先にすべてのImportを解除する必要がある。

したがって、今回の対象スタックが旧ExportをImportしている状態で、旧パラメータグループを置換してExport値を変更する進め方は避ける。

参考：AWS CloudFormation「Fn::ImportValue」
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/intrinsic-function-reference-importvalue.html

### 根拠② AWS公式にも「Import側を先に更新する」解決手順がある

AWS Knowledge Centerには、次のエラーの解決手順がある。

「Export EXPORT_NAME cannot be updated as it is in use by STACK_NAME」

記事では、Importしているスタックを特定し、そのテンプレートのImportValueを出力実値に置き換えて更新した後、Export側を変更・削除する流れを説明している。

今回も、この「Import側を先に更新する」という順序を採用する。

ただし、記事の例は既存Exportの実値への置き換えである。
今回、Redis本体の設定を現在使用中のRedis 7用デフォルトグループ名へ合わせる部分は、稼働状態を維持するための個別提案であり、記事が直接指定しているものではない。

旧Exportの値であるRedis 6用グループ名を、そのままRedis 7.1本体へ指定し直す手順ではない。

参考：AWS Knowledge Center「Exportが使用中で更新できない場合の解決」
https://repost.aws/knowledge-center/cloudformation-stack-export-name-error

### 根拠③ パラメータグループFamilyの変更は置換になる

AWS::ElastiCache::ParameterGroupのCacheParameterGroupFamilyは、変更時にReplacementが必要なプロパティである。

そのため、旧グループのFamilyをredis6.xからredis7へ変更する操作を、今回の整合化と同時に行わない。

旧グループを残すのは依存関係を維持するためであり、Redis 7.1へ旧グループを適用するためではない。

参考：AWS CloudFormation「AWS::ElastiCache::ParameterGroup」
確認箇所：CacheParameterGroupFamily → Update requires: Replacement
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-parametergroup.html

### 根拠④ Redis 7にはクラスターモード有効用のデフォルトグループがある

AWS公式では、Redis OSS 7のデフォルトグループを次のように区別している。

- default.redis7：クラスターモード無効用
- default.redis7.cluster.on：クラスターモード有効用

ただし、実環境で使用中の名前はコンソール等で確認し、その正確な値をテンプレートへ反映する。名前を推測して設定しない。

独自設定がcluster-enabled=yesのみという説明だけで、その他の設定まで完全に同一と断定しない。

参考：Amazon ElastiCache「Engine specific parameters」
確認箇所：Redis OSS 7 parameter changes
https://docs.aws.amazon.com/AmazonElastiCache/latest/dg/ParameterGroups.Engine.html

## 4. 段階的な進め方

### 段階1：旧パラメータグループとExportを維持する

- 旧パラメータグループのFamilyはredis6.xのままにする。
- 旧パラメータグループの論理IDを変更しない。
- 旧Exportの名前・値を変更しない。
- 旧パラメータグループを定義している子スタックを削除しない。

この段階では、旧グループの置換・削除を計画しない。

### 段階2：対象Redis本体の定義を実環境に合わせる

対象Redisの定義を次の状態にする。

| 項目 | 設定方針 |
| --- | --- |
| EngineVersion | 今回の更新先である"7.1" |
| CacheParameterGroupName | 実環境で現在使用しているRedis 7用デフォルトグループの正確な名前 |
| 旧ExportへのFn::ImportValue | 対象Redisの設定から外す |
| 旧パラメータグループとExport | 維持する |

確認すること：

- ImportValueが親・子のどちらに記載されているかを確認する。
- 子テンプレートにImportValueが直接書かれている場合は、親の入力値変更だけでは解除できないため、子テンプレートも修正する。
- 親から子へパラメータでグループ名を渡す構成は継続できる。固定値を子へ直接記載することが必須ではない。
- 他のRedisが旧Exportを利用していても、今回の対象と同じ設定へ一括変更しない。
- ネスト構成の更新はルートスタックから行う。

AWS公式では、ネストされたスタックの更新などは、子スタックを直接更新するのではなく、ルートスタックから開始するよう説明されている。

参考：AWS CloudFormation「Nested stacks」
確認箇所：Performing stack operations on nested stacks
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-nested-stacks.html

### 段階3：旧グループとExportの整理を別作業で行う

段階2の更新が完了してから、旧資産の削除が必要か判断する。

削除する場合は、次の両方を確認する。

1. CloudFormation上で、旧ExportをImportするスタックが残っていない。
2. ElastiCache上で、旧パラメータグループを使用するキャッシュが残っていない。

Importがゼロであることと、実際のキャッシュで未使用であることは、別々に確認する。

AWS公式では、キャッシュに関連付けられているパラメータグループは削除できないと説明されている。

参考：Amazon ElastiCache API「DeleteCacheParameterGroup」
https://docs.aws.amazon.com/AmazonElastiCache/latest/APIReference/API_DeleteCacheParameterGroup.html

他のImport先や使用中のキャッシュが残っている場合は、旧グループとExportを維持する。今回の対象Redisの整合化と同時に、すべてを整理する必要はない。

## 5. 変更セットで確認すること

実行前に、ルート・子スタックのロールバックが完了しているか確認する。

変更セットでは、親の行だけでなく、子スタック内のリソース変更まで確認する。

今回の整合化で確認する内容：

- Redis本体のEngineVersionが意図した値になっている。
- 適用するパラメータグループ名が、現在の実環境と一致している。
- 対象スタックから旧Importが外れている。
- 旧パラメータグループの置換・削除が計画されていない。
- 旧パラメータグループの子スタックが削除されない。
- 旧Exportの名前・値が変わらない。
- Redis本体の置換・削除や、今回対象外の変更が含まれていない。

Replacement: Falseは、リソースを置換しないという意味であり、更新処理がないことや無停止を保証する意味ではない。

参考：AWS CloudFormation API「ResourceChange」
確認箇所：Replacement
https://docs.aws.amazon.com/AWSCloudFormation/latest/APIReference/API_ResourceChange.html

また、AWS公式は、変更セットが作成できてもスタック更新の成功を保証するものではないと説明している。実行時のサービス側制約などで失敗する場合がある。

実環境と同じ値を指定した場合でも、テンプレートの修正が必ず「管理情報だけの変更」になるとは扱わない。

参考：AWS CloudFormation「Update stacks using change sets」
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-updating-stacks-changesets.html

## 6. 独自パラメータグループが必須の場合

現場の方針で独自パラメータグループが必須の場合は、デフォルトグループの継続利用案を採用しない。

その場合の案：

1. 旧Redis 6用グループと旧Exportを維持する。
2. 別の論理IDでRedis 7用の新しいグループを追加する。
3. Exportする場合は旧Exportとは別の名前を使う。
4. 対象Redisの参照先を新しいグループへ変更する。
5. 旧Importと実際の利用がなくなった後、旧資産を整理する。

これも、参照中の旧Export値を変更せずに移行するという、同じ制約を踏まえた提案である。

## 7. 採用理由の整理

AWS公式が直接裏付けていること：

- Import中のExport値は変更できない。
- Export側を変更する前にImport側を更新する解決手順がある。
- パラメータグループFamilyの変更は置換になる。
- 使用中のパラメータグループは削除できない。

今回の状況を踏まえた提案：

- 更新に成功したRedis 7.1と、現在のデフォルトグループを継続する。
- 旧グループの置換と本体の整合化を同時に行わない。
- 旧資産の整理を後日に分ける。

この順序により、今回問題になったExportの参照制約を避けながら、更新済みの実環境にCloudFormation定義を合わせることを目指す。
