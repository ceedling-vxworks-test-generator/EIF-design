# externalInterfaceLayer（通常版：Frame／Connector／製品CFG）

設計は [詳細設計書](../../設計/詳細設計書.md)、公開APIは [仕様設計書](../../設計/仕様設計書.md) を正とする。

## 区分と依存規則

| フォルダ | 区分 | 機種依存 | 参照してよいヘッダー |
|---|---|---|---|
| `include/` | 公開API | なし | - |
| `frame/` | Frame（共通処理） | なし | `include/`、PCL公開ヘッダー、REF製品非依存ヘッダー |
| `product/<MODEL>/` | 製品CFG（表＋変換関数） | **あり** | `include/`、`frame/include/`、PCL公開ヘッダー、REF製品別ヘッダー |
| `connectors/<接続先>/` | Connector | なし（接続先依存） | `include/`、外部コンポーネントのヘッダー |
| `test/` | 単体試験 | - | - |

`frame/` に REF製品別ヘッダー（`rim/include/<MODEL>`）やAPIサーバーのヘッダーをincludeすると、ビルドエラーになる（include pathを渡していないため）。

## ファイル

```text
include/
  eif_api.h            公開API・結果コード・Connector登録・EIF_Event
  eif_public_id.h      公開ID
  eif_model.h          公開モデル
  eif_profile.h        製品CFGの表の型（製品CFGの作成者向け）
frame/
  include/             eif_frame_api.h（EIF_Initialize）、eif_port.h（Port）
  eif_internal.h       Frame内部の部品間IF
  core/                初期化・表の検証・開始
  connector/           Connector登録
  request/             要求受付（Gateway）・要求台帳と完了の振り分け（Tracker）
  info/                情報取得・設定変更・購読・StateCache
  notify/              イベント配信
  port/                PCL・REFの実APIを指す既定Port
  util/                結果コード変換・ロック・ログ
product/
  Skeleton/            雛形（何も公開しない）
  MiniPrinter/         要求の表・ネットワーク設定・異常通知
connectors/
  api_server/          APIサーバー用（ネットワーク設定GET/PATCH、ジョブ作成、完了・異常のWebSocket送信）
test/
  frame/               スタブPort＋試験用製品CFGで Frame を試験
  product/             MiniPrinterの変換関数の試験
```

## ビルドと試験

### gcc だけで確認する（CMakeが無い環境）

```sh
sh build_and_test.sh
```

次を順に行う。外部ヘッダーの場所は環境変数（`PCL_INC`、`RIM_COMMON_INC`、`RIM_MODEL_INC`、`APISV_INC`）で変更できる。既定は本リポジトリの `参考資料/` 配下。

1. Frameを、REF製品別ヘッダーとAPIサーバーのヘッダー無しでコンパイル（依存規則の確認）
2. Frameの単体試験（18件）
3. MiniPrinter製品CFGの単体試験（5件）
4. 製品CFGのSetup、Skeleton、APIサーバー用Connectorのコンパイル確認（リンクはしない）

### CMake

```sh
cmake -S . -B build -DMODEL=MiniPrinter \
  -DPCL_INCLUDE_DIR=... -DRIM_COMMON_INCLUDE_DIR=... -DRIM_MODEL_INCLUDE_DIR=... -DAPI_SERVER_INCLUDE_DIR=... \
  -DEIF_BUILD_TESTS=ON
```

※ CMakeLists.txt は作成のみで、この環境では動作を確認していない（CMakeが無いため）。

## 初版の範囲と未実装

| 項目 | 状態 |
|---|---|
| Frame（初期化、Connector登録、要求受付・台帳・振り分け・先着完了、取消、結果照会、情報取得、設定変更（REF直接）、購読・StateCache・配信） | 実装・試験済み |
| 設定変更の書込み先 PCL経由（`EIF_SINK_PCL_REQUEST`） | 表の検証で拒否（QA-01、QA-02の決定後に実装） |
| 製品CFG MiniPrinter | 要求の表（暫定の対応）、ネットワーク設定、異常通知（公開アラームIDは暫定） |
| APIサーバー用Connector | ネットワーク設定、ジョブ作成、完了・異常のWebSocket送信。他のRESTカテゴリは現行実装から移設する |
| 印字データ | 対象外（QA-06） |
| ロック | POSIXスレッド（`frame/util/eif_lock.h`）。製品のOS抽象へ寄せる場合はこのファイルだけを差し替える |
| ログ | 既定は標準エラー出力。`EIF_USE_UT_LOG` を定義すると foundation/log へ出す |

設定変更の排他は、設計では「設定IDごとのmutex」としているが、初版は設定変更の頻度が低いため1つのmutexで実装している。
