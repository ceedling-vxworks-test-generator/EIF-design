/**
 * @file eif_setup.c
 * @brief 機種セットアップの雛形。何も公開しないAccessorをつなぐ。新機種はこのフォルダと各Accessorの models/ をコピーして作る。
 *
 * MODEL 未指定時に使う。全要求・全情報・全設定が EIF_ERR_NOT_EXPOSED になる。
 */
#include "eif_frame_api.h"

/* ---------------------------------------------------------------- 何も公開しない下位IFの実装 */

/** @brief 製品制御IF：公開しない。 @param[in] id 公開要求ID @return 常に false */
static bool none_request(eif_request_id_t id) { (void)id; return false; }
/** @brief 製品制御IF：送信しない。 @param[in] id 公開要求ID @param[out] out 書かない @return EIF_ERR_NOT_EXPOSED */
static EIF_Result none_submit(eif_request_id_t id, EIF_Ticket *out) { (void)id; (void)out; return EIF_ERR_NOT_EXPOSED; }
/** @brief 製品制御IF：取消対象なし。 @param[in] t チケット @return EIF_ERR_NOT_FOUND */
static EIF_Result none_cancel(EIF_Ticket t) { (void)t; return EIF_ERR_NOT_FOUND; }
/** @brief 製品制御IF：照会対象なし。 @param[in] t チケット @param[out] o 書かない @return EIF_ERR_NOT_FOUND */
static EIF_Result none_query(EIF_Ticket t, EIF_RequestOutcome *o) { (void)t; (void)o; return EIF_ERR_NOT_FOUND; }
/** @brief 製品制御IF：完了は来ないので保持しない。 @param[in] fn 完了トランポリン */
static void none_done(EIF_ControlDoneFn fn) { (void)fn; }
/** @brief システム情報IF：公開しない。 @param[in] id 公開情報ID @return 常に false */
static bool none_info(eif_info_id_t id) { (void)id; return false; }
/** @brief システム情報IF：サイズなし。 @param[in] id 公開情報ID @return 0 */
static size_t none_size(eif_info_id_t id) { (void)id; return 0U; }
/** @brief システム情報IF：取得しない。 @param[in] id 公開情報ID @param[out] o 書かない @param[in] s サイズ @return EIF_ERR_NOT_EXPOSED */
static EIF_Result none_read(eif_info_id_t id, void *o, size_t s) { (void)id; (void)o; (void)s; return EIF_ERR_NOT_EXPOSED; }
/** @brief システム情報IF：監視しない。 @param[in] id 公開情報ID @param[in] f 関数 @param[in] c 値 @return EIF_ERR_NOT_EXPOSED */
static EIF_Result none_watch(eif_info_id_t id, EIF_InfoChangedFn f, void *c) { (void)id; (void)f; (void)c; return EIF_ERR_NOT_EXPOSED; }
/** @brief 設定保存IF：公開しない。 @param[in] id 公開設定ID @return 常に false */
static bool none_setting(eif_setting_id_t id) { (void)id; return false; }

/**
 * @brief 設定保存IF：保存しない。
 * @param[in]  id 公開設定ID
 * @param[in]  m  マージ済みの公開モデル
 * @param[in]  s  m のサイズ
 * @param[out] t  書かない
 * @return EIF_ERR_NOT_EXPOSED
 */
static EIF_Result none_store(eif_setting_id_t id, const void *m, size_t s, EIF_Ticket *t)
{
    (void)id; (void)m; (void)s; (void)t;
    return EIF_ERR_NOT_EXPOSED;
}

static const EIF_ProductControlIf s_control = { none_request, none_submit, none_cancel, none_query, none_done };
static const EIF_SystemInfoIf s_info = { none_info, none_size, none_read, none_watch };
static const EIF_SettingStoreIf s_setting = { none_setting, none_store };

/**
 * @brief 何も公開しない構成でEIFを初期化する。
 * @retval EIF_OK 初期化済み
 * @note 起動処理のスレッド [C1] から1回呼ぶ。
 */
EIF_Result EIF_Setup(void)
{
    static const EIF_AccessorSet acc = { &s_control, &s_info, &s_setting };
    return EIF_Frame_Initialize(&acc);
}
