# externalInterfaceLayer（Frame／Connector／Accessor／機種セットアップ）

設計は [詳細設計書](../../設計/詳細設計書.md)、公開APIは [仕様設計書](../../設計/仕様設計書.md) を参照。

## 区分と依存規則

| フォルダ | 区分 | 参照してよいもの |
|---|---|---|
| `include/` | 公開API | EIFの型だけ |
| `frame/` | Frame（機種非依存部） | EIFの型だけ（PCL・REF・APIサーバー・機種を知らない） |
| `frame/include/eif_accessor_if.h` | 下位IF（FrameがPCL・REFを使うときの呼出し口） | EIFの型だけ |
| `connectors/<接続先>/` | Connector（外部コンポーネント接続部） | 公開API、外部コンポーネントのヘッダー |
| `accessors/pcl/` | PCL Accessor（PCL接続部） | 下位IF、PCL公開ヘッダー |
| `accessors/ref/` | REF Accessor（REF接続部） | 下位IF、REFヘッダー（`models/` だけが製品別ヘッダー） |
| `accessors/*/models/<MODEL>/` | 機種ごとの対応表 | 機種別のID・型 |
| `setup/<MODEL>/` | 機種セットアップ | どの下位IFにどのAccessorをつなぐか |
| `test/stubs/` | スタブAccessor | EIFの型だけ |

依存の向きは、すべて「Connector／Accessor／機種セットアップ → Frame」になる。FrameからPCL・REF・外部コンポーネントへの依存は無い。

## ファイル

```text
include/                    eif_api.h、eif_public_id.h、eif_model.h
frame/
  include/                  eif_accessor_if.h（下位IF）、eif_frame_api.h（機種セットアップ向け）
  eif_frame_internal.h      Frame内部の部品間IF
  core/                     初期化・開始
  connector/                Connectorの登録
  request/                  要求受付・要求台帳と完了の振り分け（キー＝チケット）
  info/                     情報取得
  setting/                  設定定義表・公開モデルのマージ
  notify/                   購読表（購読要求の集約）・公開イベントの配信（前回値・差分は持たない）
connectors/
  api_server/               APIサーバー用Connector
accessors/
  pcl/                      PCL Accessor（共通処理）＋ models/MiniPrinter（対応表）
  ref/                      REF Accessor（取得・購読・書込みの共通処理）＋ models/MiniPrinter（対応表・変換）
setup/
  MiniPrinter/              機種セットアップ（EIF_Setup）
  Skeleton/                 雛形（何も公開しないAccessor）
test/
  stubs/                    スタブAccessor
  frame/                    Frameの単体試験
  integration/              Frame＋MiniPrinterのセットアップの結合試験（PCL・RIMの関数はスタブ）
```

## ビルドと試験

```sh
sh build_and_test.sh
```

1. Frameを、PCL・REF・APIサーバーのヘッダーを1つも渡さずにコンパイル（依存規則の確認）
2. Frameの単体試験（16件、スタブAccessor）
3. Frame＋MiniPrinterのセットアップの結合試験（5件、PCL・RIMの関数はスタブ）
4. Skeletonのセットアップ、APIサーバー用Connectorのコンパイル確認

※ CMakeLists.txt は作成のみで、この環境では動作を確認していない（CMakeが無いため）。

## 初版の範囲と未実装

- PCL経由の設定書込みAccessor（設定保存IFの別実装）は未実装（QA-01、QA-02）。Frame側の受け入れ（`EIF_ACCEPTED`の追跡）は実装・試験済み。
- 印字データは対象外（QA-06）。
- エラーの発生／解消の通知：RIMの異常1件ごとの通知は未提供のため、暫定で通知内容を `RIM_FaultInfo`（異常1件）とみなし、異常情報リスト（`RI_DATA_FAULT_INFO_LIST`）の購読で受ける（`accessors/ref/models/MiniPrinter/ref_maps.c`）。通知の名前と内容はRIMと調整して確定する。
- 公開イベントごとの「指定できる通知方法」（`frame/connector/eif_connector_registry.c` の `k_event_defs`）は、エラーの発生／解消は変化時のみ（仕様設計書 9.3.2）。仕様に未記載の公開イベントは暫定値（インク残量の変化だけ定期も可）。
