/**
 * @file test_integration_miniprinter.c
 * @brief Frame＋MiniPrinterのセットアップ（PCL Accessor・REF Accessor）を、PCL・RIMの関数のスタブで結合試験する。
 *
 * PCL・RIMの公開関数（ExternalRequestIngressSubmit、RIM_GetBinary 等）をこのファイルで定義し、
 * 本物の代わりにリンクする。Accessorの共通処理と対応表、Frameの組合せを確認する。
 */
#include <string.h>

#include "eif_api.h"
#include "eif_frame_api.h"
#include "eif_test.h"
#include "pcl_accessor.h"
#include "ref_accessor.h"
#include "rim_api.h"

int g_test_failures = 0;
int g_test_runs = 0;

/* ================================================================ PCL・RIMのスタブ */

static uint64_t s_gen;                              /**< 最後に払い出したPCLの受付ID */
static product_request_id_t s_last_product;         /**< 最後に送信された製品要求ID */
static external_request_completion_cb_t s_pcl_cb;   /**< 最後に渡されたPCLの完了callback */

/**
 * @brief PCLのスタブ：受付だけして完了callbackを記録する（完了は試験が s_pcl_cb を呼んで起こす）。
 * @param[in] id 製品要求ID
 * @param[in] cb 完了callback
 * @return UC_OK＋新しい受付ID
 */
external_request_result_t ExternalRequestIngressSubmit(product_request_id_t id, external_request_completion_cb_t cb)
{
    external_request_result_t r;
    memset(&r, 0, sizeof(r));
    r.api_result = UC_OK;
    r.generation_id = ++s_gen;
    r.execution_result = EXTERNAL_REQUEST_EXECUTION_RESPONSE_PENDING;
    s_last_product = id;
    s_pcl_cb = cb;
    return r;
}

/** @brief PCLのスタブ：取消は常に受け付ける。 @param[in] generation_id 受付ID @return UC_OK */
uc_result_t ExternalRequestIngressCancelRequestedByOwnerGeneration(uint64_t generation_id)
{
    (void)generation_id;
    return UC_OK;
}

/**
 * @brief PCLのスタブ：照会は常に未完了。
 * @param[in]  generation_id 受付ID
 * @param[out] out           結果（PENDING）
 * @return UC_OK
 */
uc_result_t ExternalRequestIngressGetResult(uint64_t generation_id, external_request_result_t *out)
{
    memset(out, 0, sizeof(*out));
    out->generation_id = generation_id;
    out->execution_result = EXTERNAL_REQUEST_EXECUTION_RESPONSE_PENDING;
    return UC_OK;
}

static RIM_network_info_t s_net;               /**< REFのネットワーク情報 */
static uint8_t s_host[RIM_HOSTNAME_LEN];       /**< REFのホスト名 */
static uint8_t s_mac[RIM_MACADDRESS_LEN];      /**< REFのMACアドレス */
static RIM_FaultInfoList s_faults;             /**< REFの異常情報リスト */
static RINotificationCallback s_rim_cb[4];     /**< 購読で渡された通知callback */
static int s_rim_subs;                         /**< 購読の数 */

/**
 * @brief RIMのスタブ：ファイル内の変数を返す（コピーしない）。
 * @param[in]  data_id REFのData ID
 * @param[out] out     値の場所とサイズ
 * @return 既知のIDなら RI_SUCCESS、それ以外は RI_NO_DATA
 */
RIStatus RIM_GetBinary(uint32_t data_id, RI_BINARY *out)
{
    switch (data_id) {
    case RI_DATA_NETWORK_INFO: out->data = (const uint8_t *)&s_net; out->size = sizeof(s_net); return RI_SUCCESS;
    case RI_DATA_HOSTNAME: out->data = s_host; out->size = sizeof(s_host); return RI_SUCCESS;
    case RI_DATA_MACADDRESS: out->data = s_mac; out->size = sizeof(s_mac); return RI_SUCCESS;
    case RI_DATA_FAULT_INFO_LIST: out->data = (const uint8_t *)&s_faults; out->size = sizeof(s_faults); return RI_SUCCESS;
    default: return RI_NO_DATA;
    }
}

/**
 * @brief RIMのスタブ：ネットワーク情報だけ書き込める。
 * @param[in] data_id REFのData ID
 * @param[in] data    書き込む値
 * @param[in] size    data のサイズ
 * @return 書き込めたら RI_SUCCESS
 */
RIStatus RIM_SetBinary(uint32_t data_id, const void *data, size_t size)
{
    if ((data_id != RI_DATA_NETWORK_INFO) || (size != sizeof(s_net))) {
        return RI_INVALID_PARAMETER;
    }
    memcpy(&s_net, data, size);
    return RI_SUCCESS;
}

/**
 * @brief RIMのスタブ：通知callbackを記録し、購読ID（500から）を払い出す。
 * @param[in]  req    購読要求
 * @param[out] out_id 購読ID
 * @return RI_SUCCESS
 */
RIStatus RIM_Subscribe(const RI_SUBSCRIPTION_REQUEST *req, uint64_t *out_id)
{
    s_rim_cb[s_rim_subs] = req->callback;
    *out_id = 500U + (uint64_t)s_rim_subs;
    s_rim_subs++;
    return RI_SUCCESS;
}

/* ================================================================ 試験用のConnector */

static int s_completed;              /**< 完了callbackの回数 */
static EIF_RequestOutcome s_outcome; /**< 最後の終端結果 */
static int s_raised;                 /**< 発生イベントの回数 */
static int s_resolved;               /**< 解消イベントの回数 */
static EIF_ConnectorId s_id;         /**< 試験用ConnectorのID */

/**
 * @brief 試験用Connectorの完了callback。
 * @param[in] h 要求ハンドル（使わない）
 * @param[in] r 公開要求ID（使わない）
 * @param[in] t 要求元トークン（使わない）
 * @param[in] o 終端結果
 * @param[in] c 使わない
 */
static void on_completed(EIF_RequestHandle h, eif_request_id_t r, uint64_t t, EIF_RequestOutcome o, void *c)
{
    (void)h; (void)r; (void)t; (void)c;
    s_completed++;
    s_outcome = o;
}

/**
 * @brief 試験用Connectorのイベントcallback。発生／解消の回数を数える。
 * @param[in] e 公開イベント
 * @param[in] c 使わない
 */
static void on_event(const EIF_Event *e, void *c)
{
    (void)c;
    if (e->event_id == EIF_EVT_ERROR_RAISED) {
        s_raised++;
    } else if (e->event_id == EIF_EVT_ERROR_RESOLVED) {
        s_resolved++;
    }
}

/** @brief 前処理：スタブの値を戻し、MiniPrinterでセットアップ → 登録 → 開始 まで済ませる。 */
static void setup(void)
{
    static const eif_event_id_t filter[] = { EIF_EVT_ERROR_RAISED, EIF_EVT_ERROR_RESOLVED };
    const EIF_ConnectorDesc d = { "test", on_completed, on_event, filter, 2U, NULL };
    const RIM_network_info_t net = { 1U, { 192U, 168U, 1U, 5U }, { 255U, 255U, 255U, 0U }, { 192U, 168U, 1U, 1U } };

    EIF_Frame_ResetForTest();
    ref_accessor_reset_for_test();
    s_gen = 0U;
    s_rim_subs = 0;
    s_completed = 0;
    s_raised = 0;
    s_resolved = 0;
    s_net = net;
    memset(s_host, 0, sizeof(s_host));
    memcpy(s_host, "mini", 4U);
    memset(s_mac, 0xABU, sizeof(s_mac));
    memset(&s_faults, 0, sizeof(s_faults));
    (void)EIF_Setup(); /* setup/MiniPrinter */
    (void)EIF_RegisterConnector(&d, &s_id);
    (void)EIF_Start();
}

/* ---------------------------------------------------------------- 試験 */

/** @brief 要求：PCL Accessorの対応表で製品要求IDへ変換され、PCLの完了が要求元へ届く。対応表に無い要求は拒否される。 */
static void test_request_goes_through_pcl_accessor_table(void)
{
    EIF_RequestHandle h = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id, EIF_REQ_HEAD_CLEANING, NULL, 0U, 1U, &h), EIF_ACCEPTED);
    TEST_EQ(s_last_product, PRODUCT_REQUEST_ID_NORMAL_PURGE_ALL); /* 対応表で変換された */
    s_pcl_cb(s_last_product, s_gen, EXTERNAL_REQUEST_EXECUTION_RESPONSE_ACCEPTED);
    TEST_EQ(s_completed, 1);
    TEST_EQ(s_outcome, EIF_OUTCOME_COMPLETED);
    TEST_EQ(EIF_SubmitRequest(s_id, EIF_REQ_CAP_PUMP_CHECK, NULL, 0U, 1U, &h), EIF_ERR_NOT_EXPOSED);
}

/** @brief 情報取得：REFの3つの値（ネットワーク情報・ホスト名・MAC）が公開モデルの文字列へ変換される。 */
static void test_get_network_through_ref_accessor(void)
{
    eif_network_settings_t m;
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m)), EIF_OK);
    TEST_ASSERT(strcmp(m.ip, "192.168.1.5") == 0);
    TEST_ASSERT(strcmp(m.hostname, "mini") == 0);
    TEST_ASSERT(strcmp(m.macaddress, "AB:AB:AB:AB:AB:AB") == 0);
}

/** @brief 設定変更：指定した項目だけREFに書かれ、このREF Dataで変更できない項目は拒否される。 */
static void test_patch_network_writes_ref(void)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle h = 0U;
    memset(&p, 0, sizeof(p));
    p.has_gateway = true;
    (void)strcpy(p.gateway, "192.168.1.254");
    TEST_EQ(EIF_PatchSetting(s_id, EIF_SETTING_NETWORK, &p, sizeof(p), 1U, &h), EIF_OK);
    TEST_EQ(s_net.gateway[3], 254U);
    TEST_EQ(s_net.ip[3], 5U); /* 未指定は現在値 */

    memset(&p, 0, sizeof(p));
    p.has_hostname = true;    /* このREF Dataでは変更できない */
    (void)strcpy(p.hostname, "other");
    TEST_EQ(EIF_PatchSetting(s_id, EIF_SETTING_NETWORK, &p, sizeof(p), 1U, &h), EIF_ERR_INVALID_PARAM);
}

/** @brief 通知：REFの異常情報リストの変化が、発生／解消のイベントとして届く。 */
static void test_fault_list_to_events(void)
{
    TEST_EQ(s_rim_subs, 1); /* アラーム一覧の監視だけが登録される */
    s_faults.count = 1U;
    s_faults.items[0].code = (RIM_FaultCode)4;
    s_faults.items[0].state = RIM_FAULT_STATE_ACTIVE;
    s_rim_cb[0](500U, NULL);
    TEST_EQ(s_raised, 1);
    s_faults.count = 0U;
    s_rim_cb[0](500U, NULL);
    TEST_EQ(s_resolved, 1);
}

/**
 * @brief 試験の入口。
 * @return 失敗が無ければ0
 */
int main(void)
{
    printf("== Integration tests: frame + MiniPrinter accessors\n");
    RUN_TEST(setup, test_request_goes_through_pcl_accessor_table);
    RUN_TEST(setup, test_get_network_through_ref_accessor);
    RUN_TEST(setup, test_patch_network_writes_ref);
    RUN_TEST(setup, test_fault_list_to_events);
    return TEST_SUMMARY();
}
