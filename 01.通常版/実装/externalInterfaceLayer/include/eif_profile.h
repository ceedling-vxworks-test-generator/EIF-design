/**
 * @file eif_profile.h
 * @brief 製品CFG（機種依存部）がFrameへ渡す表と変換関数の型。製品CFGの作成者だけが使う。
 *
 * 表は static const で定義し、1行で1つの公開IDを表す。表に無いIDは公開されない。
 * 変換関数は入力から出力を作るだけの純粋関数とし、登録・送信・排他を行わない。
 */
#ifndef EIF_PROFILE_H
#define EIF_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_api.h"
#include "product_request_id.h" /* PCL公開ヘッダー：product_request_id_t */
#include "rim_types_common.h"   /* REF製品非依存ヘッダー：RI_TARGET_TYPE 等 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 1つの公開情報・購読エントリが参照できるREF Dataの最大数。 */
#define EIF_MAX_SOURCES (4U)

/** @brief REFから取得した値（コピーせずにポインタとサイズで渡す）。 */
typedef struct {
    const void *data;
    size_t size;
} EIF_SourceView;

/* ---------------------------------------------------------------- 要求の表 */

/** @brief 公開要求ID → PCL製品要求ID。 */
typedef struct {
    eif_request_id_t public_id;            /**< 公開する要求（機能定義） */
    product_request_id_t product_request;  /**< PCLの製品要求ID（マッピング） */
} EIF_RequestMapEntry;

/* ---------------------------------------------------------------- 情報の表 */

/** @brief REF製品型 → 公開モデル。成功でtrue。失敗時は out を使わない。 */
typedef bool (*EIF_ConvertInfoFn)(const EIF_SourceView *src, size_t src_count, void *out_public, size_t out_size);

/** @brief 公開情報ID → REFのID群＋変換関数。 */
typedef struct {
    eif_info_id_t public_id;
    uint32_t source_ids[EIF_MAX_SOURCES];   /**< REFのData ID（機種別） */
    size_t source_sizes[EIF_MAX_SOURCES];   /**< 機種別の型のsizeof（型不一致の検出） */
    size_t source_count;
    size_t public_size;                     /**< 公開モデルのsizeof */
    EIF_ConvertInfoFn convert;
} EIF_InfoMapEntry;

/* ---------------------------------------------------------------- 設定の表 */

/** @brief 設定の書込み先（QA-01）。 */
typedef enum {
    EIF_SINK_REF_DIRECT = 0, /**< REFへ直接書き込む（現在の既定） */
    EIF_SINK_PCL_REQUEST     /**< PCLへ設定変更要求として送る（QA-02の対応後に有効） */
} EIF_SettingSinkKind;

/**
 * @brief パッチを内部形式へ変換して現在値へ反映する。
 * @param current 現在値（REF製品型）
 * @param patch   公開パッチモデル
 * @param out     反映結果（REF製品型）。current と同じサイズ
 * @return パッチの値が不正ならfalse
 */
typedef bool (*EIF_MergeSettingFn)(const void *current, size_t current_size, const void *patch, void *out, size_t out_size);

/** @brief 公開設定ID → 書込み先＋マージ関数。 */
typedef struct {
    eif_setting_id_t public_id;
    uint32_t ref_data_id;                 /**< 現在値を読み、書き込むREF Data ID（機種別） */
    size_t ref_size;                      /**< 機種別の型のsizeof */
    size_t patch_size;                    /**< 公開パッチモデルのsizeof */
    EIF_MergeSettingFn merge;
    EIF_SettingSinkKind sink;             /**< 書込み先。この1列で切り替える */
    product_request_id_t product_request; /**< sink=PCL_REQUEST のときの設定変更要求ID */
} EIF_SettingMapEntry;

/* ---------------------------------------------------------------- 購読の表 */

/**
 * @brief 値（＋前回値）から公開イベントを作る。
 * @param now      今回の値（source順）
 * @param prev     前回の値（source[0]の型）。has_prev=false のときは無効
 * @param out      生成したイベントの格納先
 * @param max      out の要素数
 * @return 生成したイベント数
 */
typedef size_t (*EIF_BuildEventsFn)(const EIF_SourceView *now, size_t now_count,
                                    const void *prev, size_t prev_size, bool has_prev,
                                    EIF_Event *out, size_t max);

/** @brief REFの購読 → 公開イベント。 */
typedef struct {
    RI_TARGET_TYPE target_type;           /**< Data / Capability / Facade */
    uint32_t target_id;                   /**< 購読するREFのID（機種別） */
    RI_NOTIFICATION_TRIGGER trigger;      /**< 変化通知／定期通知（EIFはタイマーを持たない） */
    uint32_t interval_ms;                 /**< 定期通知の周期 */
    uint32_t source_ids[EIF_MAX_SOURCES]; /**< 通知時に取得するREF Data ID */
    size_t source_sizes[EIF_MAX_SOURCES];
    size_t source_count;
    bool keep_previous;                   /**< source[0]の前回値を保持して差分を出すか（StateCache） */
    EIF_BuildEventsFn build_events;
} EIF_SubscriptionEntry;

/* ---------------------------------------------------------------- 表一式 */

/** @brief 製品CFGがFrameへ渡す表一式。 */
typedef struct {
    const char *product_name;
    const EIF_RequestMapEntry *requests;
    size_t request_count;
    const EIF_InfoMapEntry *infos;
    size_t info_count;
    const EIF_SettingMapEntry *settings;
    size_t setting_count;
    const EIF_SubscriptionEntry *subscriptions;
    size_t subscription_count;
} EIF_ProductProfile;

#ifdef __cplusplus
}
#endif

#endif /* EIF_PROFILE_H */
