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
  notify/                   通知定義表・差分（アラーム一覧）・配信
  cache/                    StateCache（前回値＝公開モデル）
connectors/
  api_server/               APIサーバー用Connector
accessors/
  pcl/                      PCL Accessor（共通処理）＋ models/MiniPrinter（対応表）
  ref/                      REF Accessor（取得・監視・書込みの共通処理）＋ models/MiniPrinter（対応表・変換）
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
2. Frameの単体試験（10件、スタブAccessor）
3. Frame＋MiniPrinterのセットアップの結合試験（4件、PCL・RIMの関数はスタブ）
4. Skeletonのセットアップ、APIサーバー用Connectorのコンパイル確認

※ CMakeLists.txt は作成のみで、この環境では動作を確認していない（CMakeが無いため）。

## 初版の範囲と未実装

- PCL経由の設定書込みAccessor（設定保存IFの別実装）は未実装（QA-01、QA-02）。Frame側の受け入れ（`EIF_ACCEPTED`の追跡）は実装・試験済み。
- REF Accessorの通知用の作業領域は、REFの通知が直列に来る前提（QA-11）。
- 印字データは対象外（QA-06）。
