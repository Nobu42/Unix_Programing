# Redis更新後のCloudFormation Export参照エラー解消手順書

作成日：2026年11月12日  
対象：Redis OSS 7.1への更新後、CloudFormationの整合化でExport参照エラーが発生した環境  
位置付け：実名を伏せた作業手順案。現場で記入欄を補完し、変更セットを確認して使用する。

## 1. 対象と今回の方針

| 識別名 | 役割 | 今回の対応 |
|---|---|---|
| 6 | ルートスタックのYAML | 変更版6-1を参照し、必要な値を子へ渡す |
| 6-1 | ReplicationGroupの子テンプレート | 旧Importを外し、現在使用中の7系デフォルトグループ名を受け取る |
| 6-2 | ParameterGroupの子テンプレート | 旧グループ・Family・Exportを維持する |

6／6-1／6-2は本書内の識別名であり、実際のファイル名・スタック名・論理IDではない。

今回の到達点：

- Redisの実環境は、更新済みの7.1系と現在のデフォルトグループを維持する。
- 6-1の管理定義を実環境に合わせ、6-2の旧Exportへの依存を外す。
- 旧グループとExportは今回削除せず、整理は別作業にする。

前提：デフォルトグループの継続利用が現場の方針として許容されること。独自グループが必須なら、本書の手順をそのまま適用しない。

## 2. 原因と、この方法を選ぶ根拠

共有された構成は、6-2のOutputsがグループ名をExportし、6-1のCacheParameterGroupNameがImportValueで参照する構成。

共有されたエラーは「Cannot update export ... as it is ... use[d] by ...」というもの。

AWS公式では、Import中のExport値は変更できず、先にすべてのImportを解除する必要がある。[S1]

また、AWS Knowledge Centerは、Import側テンプレートの参照を置き換えて更新した後、Export側を変更する解決手順を説明している。[S2]

Family変更はパラメータグループの置換を伴う。[S3] そのため、旧グループの置換と本体の整合化を分け、旧Export値を維持したままImport側を先に更新する。

公式資料が直接裏付けるのは「Import解除を先行する順序」。現在の7系デフォルトグループへ定義を合わせ、旧資産の整理を後日に分ける部分は、今回の稼働状況に基づく提案である。

## 3. 作業前の記入・確認

実名や値は現場内で記録する。社外へ共有する必要はない。

| 項目 | 現場で記入する内容 |
|---|---|
| 作業対象アカウント・リージョン | ［記入］ |
| 6の実スタック名 | ［記入］ |
| 6-1／6-2の実子スタック名 | ［記入］ |
| 対象ReplicationGroup ID | ［記入］ |
| 現在の実エンジンバージョン | ［7.1系であることを確認して記入］ |
| 現在の適用パラメータグループ名 | ［実環境の正確な名前を記入］ |
| 旧グループ名・旧Family | ［記入。Familyは旧定義を確認］ |
| 旧Export名・現在のExport値 | ［記入］ |
| 旧ExportのImport先一覧 | ［今回の6-1と、その他の参照元を記録］ |
| 6から6-1／6-2へ渡す既存パラメータ | ［実際のキーと値を記録］ |
| 6が参照する6-1／6-2のTemplateURL | ［現在のURLを記録］ |

AWS公式上、Redis 7のクラスターモード有効用デフォルト名はdefault.redis7.cluster.on。ただし本書の値を推測で流用せず、実環境の名前を確認する。[S4]

### 3-1. スタックと実環境の状態を確認する

1. 6と関連子スタックの現在ステータスを記録する。
2. 6がUPDATE_ROLLBACK_COMPLETE等の更新可能な安定状態で、子も処理中・失敗状態ではないことを確認する。
3. UPDATE_ROLLBACK_FAILEDや処理中のスタックがあれば、本手順の更新操作は開始しない。先に原因に応じた復旧が必要。[S10]
4. ElastiCacheで、対象Redisが7.1系であり、現在のグループが適用済みであることを確認する。関連キャッシュクラスター／ノードの状態、適用待ちの変更も確認する。
5. 最初の失敗イベント、6／6-1／6-2の保存済みテンプレート、入力パラメータ、Outputsを保存する。

失敗時に使ったローカルファイルと、現在スタックに保存されている定義を混同しない。6-2は、ロールバック後に保存されている旧定義を基準にする。

## 4. テンプレートを修正する

以下のYAMLは差分の説明用。既存のParametersやResourcesを丸ごと置き換えず、該当箇所へ追加・変更する。既存キーを重複させない。

### 4-1. 6-2：旧グループとExportを維持する

- 旧グループのFamilyをredis7へ変更しない。
- 旧グループの論理ID、Outputs、Export名、Export値を維持する。
- 6から6-2へ渡すFamilyも、旧定義の値（通常redis6.x）を維持する。
- 旧グループや6-2の子スタックを削除しない。

6-2を残すのはExportを維持するため。Redis 7.1へ旧6系グループを適用するという意味ではない。

### 4-2. 6-1：グループ名を受け取る入力を用意する

以下はAppliedCacheParameterGroupNameという新しい入力名を使う例。既存の入力を適切に流用できる場合は、新設せず既存キーを使う。

6-1のParameters配下へ追加する例：

```yaml
  AppliedCacheParameterGroupName:
    Type: String
    Description: Existing Redis 7 parameter group name
```

ReplicationGroupのPropertiesにあるCacheParameterGroupNameを、旧Fn::ImportValueから次の参照へ変更する。

```yaml
      CacheParameterGroupName: !Ref AppliedCacheParameterGroupName
```

- 対象の旧Import式を削除し、同じプロパティを二重に定義しない。
- EngineVersionは7.1となるようにする。既存の親からのパラメータ参照は維持できる。
- 対象外のプロパティや論理IDは変更しない。
- 6-1内で同じ旧Exportを参照する箇所が他にないか確認する。

### 4-3. 6：6-1へ現在のグループ名を渡す

6のParameters配下にも、同じ入力を追加する例：

```yaml
  AppliedCacheParameterGroupName:
    Type: String
    Description: Existing Redis 7 parameter group name
```

6内の、6-1に対応するAWS::CloudFormation::StackのProperties.Parameters配下へ追加する。

```yaml
        AppliedCacheParameterGroupName: !Ref AppliedCacheParameterGroupName
```

既存のParametersマップを残し、その中へ追加する。6-2側へこの値を渡す必要はない。

値の整理：

| 6から渡す対象 | 設定する値 |
|---|---|
| 6-1のエンジンバージョン | 7.1 |
| 6-1の新しいグループ名入力 | 現在適用中の7系デフォルトグループ名 |
