/**
 * @file eif_connector_registry.c
 * @brief Connectorの登録（詳細設計書 7.2）。登録は起動時だけで、EIF_Start()後は不変。
 *
 * 登録中はmutexで保護する。EIF_Start() で締め切った後は不変になるため、
 * 読出し（完了通知・イベント配信のたびに呼ばれる）はロックせずに行う。
 */
#include <stdatomic.h>
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"

static eif_connector_t s_connectors[EIF_MAX_CONNECTORS]; /**< 登録表（登録順。index＋1 ＝ Connector ID） */
static size_t s_count = 0U;                               /**< 登録済みの件数 */
static atomic_bool s_closed = false;                      /**< 締切り済み（EIF_Start() 後）なら true */
static eif_mutex_t s_lock = EIF_MUTEX_INITIALIZER;        /**< 登録中の保護 */

/**
 * @brief Connectorを登録する。
 *
 * desc をコピーし、event_filter はビット列（event_mask）に変換して保持する（呼出し元の配列は保持しない）。
 * 公開イベントIDの範囲外の値は無視する。
 *
 * @param[in]  desc   登録内容
 * @param[out] out_id 払い出したConnector ID（1から）
 *
 * @retval EIF_OK                登録した
 * @retval EIF_ERR_INVALID_PARAM desc・out_id がNULL、on_request_completed がNULL、
 *                               event_filter_count>0 なのに event_filter がNULL
 * @retval EIF_ERR_BUSY          締切り後、または登録数の上限
 *
 * @note 起動処理のスレッド [C1] から呼ぶ。
 */
EIF_Result EIF_RegisterConnector(const EIF_ConnectorDesc *desc, EIF_ConnectorId *out_id)
{
    if ((desc == NULL) || (out_id == NULL) || (desc->on_request_completed == NULL) ||
        ((desc->event_filter_count > 0U) && (desc->event_filter == NULL))) {
        return EIF_ERR_INVALID_PARAM;
    }

    eif_mutex_lock(&s_lock);
    if (atomic_load(&s_closed) || (s_count >= EIF_MAX_CONNECTORS)) {
        eif_mutex_unlock(&s_lock);
        return EIF_ERR_BUSY;
    }

    eif_connector_t *c = &s_connectors[s_count];
    memset(c, 0, sizeof(*c));
    c->id = (EIF_ConnectorId)(s_count + 1U);
    c->desc = *desc;
    c->desc.event_filter = NULL; /* 呼出元の配列は保持しない。ビット列へ写す */
    c->desc.event_filter_count = 0U;
    for (size_t i = 0U; i < desc->event_filter_count; i++) {
        const eif_event_id_t e = desc->event_filter[i];
        if ((e > EIF_EVT_INVALID) && (e < EIF_EVT_COUNT)) {
            c->event_mask |= (1UL << (unsigned)e); /* bit n ＝ 公開イベントID n を受け取る */
        }
    }
    s_count++;
    *out_id = c->id;
    eif_mutex_unlock(&s_lock);
    return EIF_OK;
}

/**
 * @brief 登録を締め切る。以降の登録は EIF_ERR_BUSY になり、登録表は不変になる。
 * @note EIF_Start() から呼ばれる。
 */
void eif_registry_close(void)
{
    eif_mutex_lock(&s_lock);
    atomic_store(&s_closed, true); /* 以降の読出しはロック不要（不変） */
    eif_mutex_unlock(&s_lock);
}

/**
 * @brief Connector IDから登録内容を引く。
 * @param[in] id Connector ID
 * @return 登録内容。未登録・締切り前ならNULL
 */
const eif_connector_t *eif_registry_get(EIF_ConnectorId id)
{
    if ((id == 0U) || (id > EIF_MAX_CONNECTORS)) {
        return NULL;
    }
    return eif_registry_at((size_t)id - 1U);
}

/**
 * @brief 登録順の index 番目の登録内容を返す。
 * @param[in] index 0から
 * @return 登録内容。範囲外・締切り前ならNULL（締切り前は登録中の可能性があるため返さない）
 */
const eif_connector_t *eif_registry_at(size_t index)
{
    if (!atomic_load(&s_closed) || (index >= s_count)) {
        return NULL;
    }
    return &s_connectors[index];
}

/** @brief 試験専用。登録表を空にし、締切りを解除する。 */
void eif_registry_reset(void)
{
    eif_mutex_lock(&s_lock);
    memset(s_connectors, 0, sizeof(s_connectors));
    s_count = 0U;
    atomic_store(&s_closed, false);
    eif_mutex_unlock(&s_lock);
}
