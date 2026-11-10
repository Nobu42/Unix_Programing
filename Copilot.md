以下は、今回のRedis更新とCloudFormation整合化に関する引き継ぎ資料です。

まず資料を読み、現場共有事項・AWS公式仕様・未確認事項を区別して理解してください。
URLへアクセスできない場合も、本文に記載された仕様を根拠として扱ってください。
推測で未確認事項を埋めないでください。

この後に、親子テンプレートの該当箇所と変更セットの内容を提示します。
それらを確認してから、想定される変更と判断できない点を説明してください。
コードの変更やAWS操作は行わず、確認と説明だけをしてください。

資料だけでは実環境の更新可否は判断できない
1. 親テンプレートの、子へ値を渡している部分
2. 子テンプレートの、バージョン・ファミリー・グループ参照の部分
3. 変更セットの`ResourceType`・`Action`・`Replacement`・`PolicyAction`

# ElastiCache Redis更新後のCloudFormation整合化

公式資料確認日：2026年11月11日
対象：東京リージョンのAmazon ElastiCache for Redis OSS／CloudFormationのネスト構成

## 0. この資料の読み方

- 「現場共有事項」は担当者からの説明であり、AWSが個別環境を確認した結果ではない。
- 「AWS公式仕様」は公式ドキュメント・公式公開スキーマで確認した内容。
- 「未確認事項」は、この資料から推測して補完しない。
- 本資料はAWS公式仕様の整理であり、AWS Supportによる個別構成への回答ではない。
- 参考URLへアクセスできなくても判断できるよう、重要な仕様は本文に記載している。
- この資料だけで、変更セットの実行成功やアプリへの無影響を保証しない。

## 1. 現場共有事項

C01. Redis OSS 6.2.6から7.1.0へのメジャーバージョンアップを行っている。

C02. RedisはCloudFormationで管理されている。エンジン更新操作はElastiCacheのWebコンソールで行い、その後にCloudFormationの管理定義を実環境へ合わせる方針。

C03. CloudFormationはネスト構成。子スタックにレプリケーショングループとパラメータグループがある。

C04. 子テンプレートの設定には値の参照が使われている。担当者は親テンプレート側でEngineVersionを7.1、パラメータグループファミリーをredis7へ変更することを検討している。

C05. パラメータグループの独自設定として「Cluster Enable = yes」があると共有されている。実際のプロパティ名・値・参照経路は現物コードで照合する。

C06. 作成した変更セットでは、レプリケーショングループのReplacementはFalse。パラメータグループにはDeleteに関する表示があった。ただし、Action・Replacement・PolicyActionの正確な組合せは未提示。

C07. この資料の作成時点では、実テンプレート全文、変更セット詳細、現在の実リソース情報は確認していない。

## 2. AWS公式仕様

### F01. パラメータグループのファミリー変更は置換になる

対象：
AWS::ElastiCache::ParameterGroup

プロパティ：
CacheParameterGroupFamily

公式の更新分類：
Replacement

有効値にはredis6.xとredis7が含まれる。

同リソースに対するRefは、パラメータグループのリソース名を返す。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-parametergroup.html

### F02. Redis OSS 7.1には互換性のある7系グループが必要

AWSは、Redis OSS 7.1ではRedis OSS 7ファミリーのパラメータグループを使用すると説明している。

ファミリーの指定値はredis7。
エンジンバージョン7.1と、ファミリー名redis7は別の設定項目。

パラメータグループを変更する場合、条件付きで変更可能なパラメータの値は、現在のグループと新グループで同じである必要がある。

公式URL：
https://docs.aws.amazon.com/AmazonElastiCache/latest/dg/ParameterGroups.Management.html

### F03. 置換では新旧の物理リソースが別になる

CloudFormationの一般的な置換動作は次のとおり。

1. 新しい物理リソースを作成する。
2. 依存リソースの参照を、新しい物理リソースへ変更する。
3. 古い物理リソースを削除する。

公式説明には「通常」という限定がある。
この一般仕様だけで、今回の参照切替・旧グループ削除の成功を保証することはできない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-updating-stacks-update-behaviors.html

### F04. 変更セットのAction・Replacement・PolicyActionは別項目

| 項目 | 値 | 意味 |
|---|---|---|
| Action | Add | リソースを追加する |
| Action | Modify | リソースを変更する |
| Action | Remove | リソースを削除する |
| Action | SyncWithActual | 実リソースを変更せず、CloudFormation側のメタデータを変更する |
| Replacement | True | Modifyによりリソースを置換する |
| Replacement | False | Modifyによるリソースの置換は行わない |
| Replacement | Conditional | 置換の有無が条件付き |
| PolicyAction | Delete | 物理リソースを削除する |
| PolicyAction | ReplaceAndDelete | 置換し、旧物理リソースを削除する |
| PolicyAction | ReplaceAndRetain | 置換し、旧物理リソースを保持する |

ReplacementはModifyに対する置換判定。

Replacement=Falseは、変更しないという意味ではない。
また、更新成功やアプリ無停止を保証する項目でもない。

画面上の「Delete」という文字だけでは、上記のどの項目・処置か確定できない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/APIReference/API_ResourceChange.html

### F05. 使用中のパラメータグループは削除できない

ElastiCacheは、いずれかのキャッシュクラスターに関連付けられているパラメータグループを削除できない。

デフォルトのパラメータグループも削除できない。

公式URL：
https://docs.aws.amazon.com/AmazonElastiCache/latest/APIReference/API_DeleteCacheParameterGroup.html

### F06. 置換時の旧リソース保持と、単純削除時の保持は別設定

UpdateReplacePolicy：
スタック更新でリソースが置換されたときの、旧物理リソースの扱いを制御する。

DeletionPolicy：
スタック削除時、またはテンプレートからリソース定義を除去したときの扱いを制御する。

DeletionPolicyは、置換される旧物理リソースには適用されない。

UpdateReplacePolicy=Retainでは旧物理リソースを残すが、その旧物理リソースはCloudFormationの管理範囲から外れる。

この仕様の説明は、現場で保持ポリシーを変更する指示ではない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-attribute-updatereplacepolicy.html

https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-attribute-deletionpolicy.html

### F07. 親から子への値の受け渡しには明示的な定義がある

親テンプレートのAWS::CloudFormation::Stackには、子スタックへ渡すParametersを指定できる。

子スタックの出力を親から参照するときは、GetAttでOutputs.出力名を参照できる。

AWSは、ネストしたスタックの更新を親スタックから行うことを強く推奨している。

この仕様だけでは、現場の親から子への参照経路が正しく接続されていると確認したことにはならない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-cloudformation-stack.html

### F08. Defaultの変更と、実際に渡すパラメータ値は別

ParametersのDefaultは、別の値を指定しなかった場合に使用する値。

更新要求のUsePreviousValue=trueは、スタックが使用中の既存パラメータ値を保持する指定。

そのため、「Defaultを書き換えた」という情報と、「更新時に子へ渡される値が変わった」という情報を同一に扱わない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/parameters-section-structure.html

https://docs.aws.amazon.com/AWSCloudFormation/latest/APIReference/API_Parameter.html

### F09. ネストの親の置換判定と、子リソースの置換判定は別

ネストした変更セットでは、子の変更セットへ移動して、子内部の変更内容を確認できる。

ResourceType=AWS::CloudFormation::Stackの行と、
ResourceType=AWS::ElastiCache::ReplicationGroupの行は、異なるリソースの判定。

階層全体の変更セットの実行・削除は、その階層のルート変更セットから行う。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/change-sets-for-nested-stacks.html

### F10. 東京向けスキーマではEngineVersionとグループ参照がwrite-only

2026年11月11日、AWS公式の東京リージョン向け公開スキーマZIPを取得して再確認した。

確認ファイル：
aws-elasticache-replicationgroup.json

対象リソース：
AWS::ElastiCache::ReplicationGroup

writeOnlyPropertiesに含まれる項目の抜粋：

- /properties/EngineVersion
- /properties/CacheParameterGroupName

writeOnlyPropertiesは、ユーザーが設定できるが、リソースプロバイダーのread/list処理では返されない項目を意味する。

ElastiCacheのWebコンソールやサービスAPIでも読めない、という意味ではない。

また、この確認は「東京だけがこの仕様」という主張ではない。

公式スキーマ配布案内：
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/resource-type-schemas.html

確認した東京リージョンの公式ZIP：
https://schema.cloudformation.ap-northeast-1.amazonaws.com/CloudformationSchema.zip

writeOnlyPropertiesの公式定義：
https://docs.aws.amazon.com/cloudformation-cli/latest/userguide/resource-type-schema.html

### F11. IN_SYNCはバージョン・グループ参照まで一致する証明ではない

CloudFormationには、比較できないためドリフト検出結果へ含められないプロパティがある。

リソースがドリフト検出対応であることと、その全プロパティを比較できることは別。

したがって、F10の項目について、IN_SYNCだけを根拠に実設定とテンプレートの一致を判定しない。

本資料では、現場のドリフト結果に本体の行が表示されない理由までは確定していない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-stack-drift.html

### F12. drift-aware変更セットでもwrite-only項目は実値比較にならない

通常の変更セット：
新しいテンプレートと、前回デプロイのテンプレートを比較する。

drift-aware変更セット：
実状態・前回デプロイ状態・新しい希望状態を比較する。

ただし、write-onlyプロパティについては、実値ではなく前回デプロイ値と比較する。

したがって、drift-awareを選んだという理由だけで、今回のEngineVersionとCacheParameterGroupNameが実値比較されるとは扱わない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/drift-aware-change-sets.html

### F13. 本体のエンジン更新とパラメータグループの置換は別の更新分類

AWS::ElastiCache::ReplicationGroupでは、EngineVersionとCacheParameterGroupNameの更新分類はNo interruption。

AWS::ElastiCache::ParameterGroupでは、CacheParameterGroupFamilyの更新分類はReplacement。

この2種類のリソースの更新分類を混同しない。

また、ElastiCacheのエンジン更新資料には、更新時に既存クライアント接続を切断すると記載されている。CloudFormationの更新分類だけでアプリ無影響を判定しない。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-replicationgroup.html

https://docs.aws.amazon.com/AmazonElastiCache/latest/dg/VersionManagement-upgrade-considerations.html

### F14. Global Datastoreには追加制約がある

ReplicationGroupがGlobal Datastoreに関連付いている場合、CacheParameterGroupNameの変更に制約がある。

公式リファレンスでは、Global Datastoreへの関連付け後にこのプロパティを変更しようとするCloudFormation更新は失敗すると説明されている。

現場の対象がGlobal Datastoreに属するかは、本資料では未確認。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-elasticache-replicationgroup.html

### F15. 変更セットの作成成功は更新成功の保証ではない

AWSは、変更セットがスタック更新の成功を保証しないと明記している。

事前検証が通っても、実行時のサービス固有制約などにより失敗する可能性がある。

公式URL：
https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/using-cfn-updating-stacks-changesets.html

## 3. この情報だけでは確定できないこと

U01. パラメータグループのDelete表示が、Action=Removeなのか、PolicyAction=Deleteなのか、ReplaceAndDeleteなのか。

U02. 現在Redisが使用している7系用グループと、今回の更新で参照されるグループが同じ物理リソースか。

U03. 親の変更値が、どの子パラメータ・プロパティへ渡されるか。

U04. 親の変更箇所がDefaultなのか、子へ渡すParametersなのか、別の値の定義なのか。

U05. 削除対象の旧グループを、他のRedisが使用しているか。

U06. Global Datastoreの関連付けの有無。

U07. 作成した変更セットの方式が通常かdrift-awareか。

U08. 現在の実エンジンバージョン・適用グループと、更新後に解決される値の組合せ。

U09. 今回の変更セットが実際に成功するか、アプリに影響がないか。

## 4. 現場AIへの依頼

上記の公式仕様を前提として、提示された現物コードと変更セットだけを確認してください。

回答は以下の形式にしてください。

1. 確認できた現物の事実
2. 対応する公式仕様の番号（F01～F15）
3. 未確認事項
4. 判断に必要な追加情報

特に、次の値を区別してください。

- 変更セットのResourceType
- Action
- Replacement
- PolicyAction
- 変更前後のCacheParameterGroupFamily
- 更新時に実際に渡すEngineVersion
- RedisのCacheParameterGroupNameの参照先
- 現在使用中のグループと削除対象グループの物理名

禁止する断定：

- 「親だけの変更だから、子リソースは置換されない」
- 「Replacement=Falseだから、設定変更も接続影響もない」
- 「Deleteとあるから、必ず単純削除である」
- 「Family変更だから、今回のDeleteは必ず正常な旧グループ削除である」
- 「IN_SYNCだから、実バージョンとテンプレートは一致している」
- 「Defaultを変更したので、子へ渡す値も必ず変わる」
- 「変更セットを作成できたので、実行すれば必ず成功する」
- 「独自設定が1項目だけなので、更新全体に問題はない」

未確認事項を推測で埋めず、不足するコードの該当箇所や変更セット項目だけを求めてください。
この資料を受け取っただけでは、AWS変更の実行やコード変更を開始しないでください。
