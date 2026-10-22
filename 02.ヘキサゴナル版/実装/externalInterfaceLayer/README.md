# externalInterfaceLayer（ヘキサゴナル版：コア／ポート／アダプタ／組立て）

設計は [設計書_ヘキサゴナル版](../../設計/設計書_ヘキサゴナル版.md)、考え方は [ヘキサゴナルアーキテクチャ入門](../../設計/ヘキサゴナルアーキテクチャ入門.md) を参照。公開APIは通常版と同じ（[仕様設計書](../../../01.通常版/設計/仕様設計書.md)）。

## 六角形との対応

| フォルダ | 六角形のどこか | 知っているもの |
|---|---|---|
| `include/` | 公開ポート（入口） | EIFの型だけ |
| `core/` | コア | EIFの型だけ（PCL・REF・APIサーバー・機種を知らない） |
| `core/include/eif_driven_ports.h` | 出口ポート | EIFの型だけ |
| `adapters/driving/<接続先>/` | 使う側のアダプタ | 公開ポート、外部コンポーネントのヘッダー |
| `adapters/driven/pcl/` | 使われる側のアダプタ（PCL） | 出口ポート、PCL公開ヘッダー |
| `adapters/driven/ref/` | 使われる側のアダプタ（REF） | 出口ポート、REFヘッダー（`models/` は製品別ヘッダー） |
| `adapters/driven/*/models/<MODEL>/` | アダプタの対応表（機種ごと） | 機種別のID・型 |
| `composition/<MODEL>/` | 組立て | どのポートにどのアダプタをつなぐか |
| `test/fakes/` | 偽物アダプタ | EIFの型だけ |

## ファイル

```text
include/                    eif_api.h、eif_public_id.h、eif_model.h（通常版＋アラーム一覧）
core/
  include/                  eif_driven_ports.h（出口ポート）、eif_core_api.h（組立て用の入口）
  eif_core_internal.h       コア内部の部品間IF
  core/                     初期化・開始
  connector/                使う側アダプタ（Connector）の登録
  request/                  要求受付・要求台帳と完了の振り分け（キー＝チケット）
  info/                     情報取得
  setting/                  設定定義表・公開モデルのマージ
  notify/                   通知定義表・差分（アラーム一覧）・配信
  cache/                    StateCache（前回値＝公開モデル）
adapters/
  driving/api_server/       APIサーバー用（通常版のConnectorと同じコード）
  driven/pcl/               PCLアダプタ（共通処理）＋ models/MiniPrinter（対応表）
  driven/ref/               REFアダプタ（取得・監視・書込みの共通処理）＋ models/MiniPrinter（対応表・変換）
composition/
  MiniPrinter/              組立て（EIF_Setup）
  Skeleton/                 雛形（何も公開しないポート）
test/
  fakes/                    偽物アダプタ
  core/                     コアの単体試験
  adapters/                 コア＋MiniPrinterの組立ての通し試験（PCL・RIMの関数は偽物）
```

## ビルドと試験

```sh
sh build_and_test.sh
```

1. コアを、PCL・REF・APIサーバーのヘッダーを1つも渡さずにコンパイル（依存規則の確認）
2. コアの単体試験（10件、偽物アダプタ）
3. コア＋MiniPrinterの組立ての通し試験（4件、PCL・RIMの関数は偽物）
4. Skeletonの組立て、APIサーバー用アダプタのコンパイル確認

※ CMakeLists.txt は作成のみで、この環境では動作を確認していない（CMakeが無いため）。

## 通常版との主な違い（実装で確認できる点）

| 観点 | 通常版 | ヘキサゴナル版 |
|---|---|---|
| 中心のコンパイルに必要な外部ヘッダー | PCL公開ヘッダー、REF製品非依存ヘッダー | **なし** |
| 差分（発生／解消） | 製品CFGのイベント生成関数（機種ごと） | コアの通知定義表（公開モデル＝アラーム一覧に対して1回だけ） |
| 設定のマージ | 製品CFGのマージ関数（REF製品型に対して） | コアの設定定義表（公開モデルに対して）＋REFアダプタの変換 |
| 設定の書込み先の切替 | 製品CFGの`sink`列 | 組立てでつなぐアダプタを替える（PCL経由は`store()`が`EIF_ACCEPTED`を返すだけで、コアは対応済み。試験済み） |
| 中心の試験 | スタブPort（PCL・REFの型） | 偽物アダプタ（EIFの型だけ） |

## 初版の範囲と未実装

- PCL経由の設定アダプタ（設定保存ポートの別実装）は未実装（QA-01、QA-02）。コア側の受け入れ（`EIF_ACCEPTED`の追跡）は実装・試験済み。
- REFアダプタの通知用の作業領域は、REFの通知が直列に来る前提（QA-11）。
- 印字データは対象外（QA-06）。
