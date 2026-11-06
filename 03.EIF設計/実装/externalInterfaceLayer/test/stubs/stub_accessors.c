/**
 * @file stub_accessors.c
 * @brief 試験用のスタブAccessor。
 */
#include <string.h>

#include "stub_accessors.h"

stub_t g_stub;

/* ---------------------------------------------------------------- 製品制御IF */

/** @brief 公開範囲：g_stub.supported_request だけ。 @param[in] id 公開要求ID @return 一致すれば true */
static bool c_is_supported(eif_request_id_t id) { return id == g_stub.supported_request; }

/**
 * @brief 送信：g_stub.submit_result を返す。受付なら次のチケットを払い出す。
 * @param[in]  id  公開要求ID（使わない）
 * @param[out] out チケット
 * @return g_stub.submit_result
 * @note complete_inline なら、戻る前に完了を呼ぶ（先着完了の再現）。
 */
static EIF_Result c_submit(eif_request_id_t id, EIF_Ticket *out)
{
    (void)id;
    if (g_stub.submit_result != EIF_ACCEPTED) {
        return g_stub.submit_result;
    }
    *out = ++g_stub.next_ticket;
    if (g_stub.complete_inline) {
        g_stub.done(*out, EIF_OUTCOME_COMPLETED);
    }
    return EIF_ACCEPTED;
}

/** @brief 取消：チケットを記録する。 @param[in] t チケット @return EIF_OK */
static EIF_Result c_cancel(EIF_Ticket t) { g_stub.last_cancel = t; return EIF_OK; }
/** @brief 照会：常に未完了。 @param[in] t チケット @param[out] o EIF_OUTCOME_PENDING @return EIF_OK */
static EIF_Result c_query(EIF_Ticket t, EIF_RequestOutcome *o) { (void)t; *o = EIF_OUTCOME_PENDING; return EIF_OK; }
/** @brief 完了トランポリンを記録する。 @param[in] fn 完了トランポリン */
static void c_set_done(EIF_ControlDoneFn fn) { g_stub.done = fn; }

/* ---------------------------------------------------------------- システム情報IF */

/** @brief 公開範囲：ネットワーク設定とアラーム一覧。 @param[in] id 公開情報ID @return 公開していれば true */
static bool i_is_supported(eif_info_id_t id) { return (id == EIF_INFO_NETWORK_SETTINGS) || (id == EIF_INFO_ALARM_LIST); }

/**
 * @brief 公開モデルのサイズ。
 * @param[in] id 公開情報ID
 * @return ネットワーク設定ならそのsizeof、それ以外はアラーム一覧のsizeof
 */
static size_t i_size(eif_info_id_t id)
{
    return (id == EIF_INFO_NETWORK_SETTINGS) ? sizeof(eif_network_settings_t) : sizeof(eif_alarm_list_t);
}

/**
 * @brief 取得：g_stub.network を返す（ネットワーク設定のみ）。
 * @param[in]  id   公開情報ID
 * @param[out] out  書込み先
 * @param[in]  size out のサイズ
 * @return g_stub.read_result。ネットワーク設定以外は EIF_ERR_INTERNAL
 */
static EIF_Result i_read(eif_info_id_t id, void *out, size_t size)
{
    if (g_stub.read_result != EIF_OK) {
        return g_stub.read_result;
    }
    if ((id != EIF_INFO_NETWORK_SETTINGS) || (size != sizeof(g_stub.network))) {
        return EIF_ERR_INTERNAL;
    }
    memcpy(out, &g_stub.network, size);
    return EIF_OK;
}

/**
 * @brief 通知できるか：g_stub.unsupported_events に無い公開イベントは通知する。
 * @param[in] req 購読要求
 * @return 通知できれば true
 */
static bool i_event_supported(const EIF_SubscriptionRequest *req)
{
    g_stub.event_supported_calls++;
    return (g_stub.unsupported_events & (1UL << (unsigned)req->id)) == 0U;
}

/**
 * @brief 購読：受け口を記録する（通知は試験が g_stub.watches[i].fn を呼んで起こす）。
 * @param[in] req 購読要求
 * @param[in] fn  受け口
 * @param[in] ctx fn へ渡す値
 * @return g_stub.watch_result
 */
static EIF_Result i_watch_event(const EIF_SubscriptionRequest *req, EIF_InfoEventFn fn, void *ctx)
{
    g_stub.watch_calls++;
    if (g_stub.watch_result != EIF_OK) {
        return g_stub.watch_result;
    }
    if ((size_t)g_stub.watch_count < STUB_MAX_WATCH) {
        stub_watch_t *w = &g_stub.watches[g_stub.watch_count++];
        w->req = *req;
        w->fn = fn;
        w->ctx = ctx;
    }
    return EIF_OK;
}

/* ---------------------------------------------------------------- 設定保存IF */

/** @brief 公開範囲：ネットワーク設定だけ。 @param[in] id 公開設定ID @return 公開していれば true */
static bool s_is_supported(eif_setting_id_t id) { return id == EIF_SETTING_NETWORK; }

/**
 * @brief 保存：マージ結果を記録し、g_stub.store_result を返す。
 * @param[in]  id         公開設定ID（使わない）
 * @param[in]  merged     マージ結果
 * @param[in]  size       merged のサイズ
 * @param[out] out_ticket EIF_ACCEPTED の場合に次のチケット
 * @return g_stub.store_result
 */
static EIF_Result s_store(eif_setting_id_t id, const void *merged, size_t size, EIF_Ticket *out_ticket)
{
    (void)id;
    g_stub.store_calls++;
    memcpy(&g_stub.stored, merged, size);
    if (g_stub.store_result == EIF_ACCEPTED) {
        *out_ticket = ++g_stub.next_ticket;
    }
    return g_stub.store_result;
}

static const EIF_ProductControlIf s_control = { c_is_supported, c_submit, c_cancel, c_query, c_set_done };
static const EIF_SystemInfoIf s_info = { i_is_supported, i_size, i_read, i_event_supported, i_watch_event };
static const EIF_SettingStoreIf s_setting = { s_is_supported, s_store };
const EIF_AccessorSet g_stub_accessors = { &s_control, &s_info, &s_setting };

/** @brief スタブを既定値に戻す。 */
void stub_reset(void)
{
    memset(&g_stub, 0, sizeof(g_stub));
    g_stub.supported_request = EIF_REQ_PRINT;
    g_stub.submit_result = EIF_ACCEPTED;
    g_stub.read_result = EIF_OK;
    g_stub.store_result = EIF_OK;
    g_stub.watch_result = EIF_OK;
    g_stub.network.dhcp = true;
    (void)strcpy(g_stub.network.ip, "10.0.0.2");
    (void)strcpy(g_stub.network.hostname, "mp");
}
