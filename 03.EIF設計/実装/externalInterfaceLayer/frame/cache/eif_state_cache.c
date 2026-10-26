/**
 * @file eif_state_cache.c
 * @brief 差分導出のための前回値（公開モデル）。EIFでデータを保持するのはここだけ（QA-03）。
 *
 * 固定長のプール（EIF_STATE_CACHE_BYTES）を、監視する公開情報ごとのスロットに切り分けて使う。
 * スロットはEIF_Start()時に割り当て、以降は解放しない。
 */
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"

/** @brief スロットの最大数。 */
#define CACHE_SLOTS (8U)

/** @brief スロット（公開情報1つ分の前回値）。 */
typedef struct {
    bool allocated;     /**< 割当て済み */
    bool has_value;     /**< 前回値を保存済み（起動直後は false） */
    eif_info_id_t id;   /**< 公開情報ID */
    size_t offset;      /**< プール内の位置 */
    size_t size;        /**< 前回値のサイズ（公開モデルのsizeof） */
    eif_mutex_t lock;   /**< 前回値の読み書きの保護（REFの通知が直列なら不要。QA-11） */
} slot_t;

static _Alignas(max_align_t) unsigned char s_pool[EIF_STATE_CACHE_BYTES]; /**< 前回値のプール */
static size_t s_used = 0U;                                               /**< プールの使用済みバイト数 */
static slot_t s_slots[CACHE_SLOTS];                                      /**< スロット */

/**
 * @brief 公開情報IDのスロットを探す。
 * @param[in] id 公開情報ID
 * @return スロット。未割当てならNULL
 */
static slot_t *slot_of(eif_info_id_t id)
{
    for (size_t i = 0U; i < CACHE_SLOTS; i++) {
        if (s_slots[i].allocated && (s_slots[i].id == id)) {
            return &s_slots[i];
        }
    }
    return NULL;
}

/**
 * @brief 公開情報1つ分の前回値の領域を割り当てる（16バイト境界に切り上げる）。
 *
 * @param[in] id   公開情報ID
 * @param[in] size 公開モデルのサイズ
 * @return 割り当てた（割当て済みを含む）なら true。サイズ0、容量・スロット不足なら false
 *
 * @note EIF_Start() から、起動処理のスレッド [C1] で呼ばれる。
 */
bool eif_cache_allocate(eif_info_id_t id, size_t size)
{
    const size_t aligned = (size + 15U) & ~(size_t)15U;

    if (slot_of(id) != NULL) {
        return true;
    }
    if ((size == 0U) || (aligned > (EIF_STATE_CACHE_BYTES - s_used))) {
        return false;
    }
    for (size_t i = 0U; i < CACHE_SLOTS; i++) {
        if (!s_slots[i].allocated) {
            s_slots[i].allocated = true;
            s_slots[i].has_value = false;
            s_slots[i].id = id;
            s_slots[i].offset = s_used;
            s_slots[i].size = size;
            (void)pthread_mutex_init(&s_slots[i].lock, NULL);
            s_used += aligned;
            return true;
        }
    }
    return false;
}

/**
 * @brief 前回値のスロットをロックし、前回値を返す。必ず eif_cache_store_and_unlock() と対で呼ぶ。
 *
 * @param[in]  id           公開情報ID
 * @param[out] out_prev     前回値（ロック中だけ有効）。未割当てならNULL
 * @param[out] out_has_prev 前回値があれば true（起動直後・未割当ては false）
 *
 * @note REFの通知ワーカー [C4] から呼ばれる。未割当てならロックしない。
 */
void eif_cache_lock(eif_info_id_t id, const void **out_prev, bool *out_has_prev)
{
    slot_t *s = slot_of(id);

    *out_prev = NULL;
    *out_has_prev = false;
    if (s == NULL) {
        return;
    }
    eif_mutex_lock(&s->lock);
    *out_prev = &s_pool[s->offset];
    *out_has_prev = s->has_value;
}

/**
 * @brief 今回値を前回値として保存し、スロットのロックを放す。
 *
 * @param[in] id   公開情報ID
 * @param[in] data 今回値（NULLなら保存せずにロックだけ放す）
 * @param[in] size data のサイズ（割当て時と異なれば保存しない）
 */
void eif_cache_store_and_unlock(eif_info_id_t id, const void *data, size_t size)
{
    slot_t *s = slot_of(id);

    if (s == NULL) {
        return;
    }
    if ((data != NULL) && (size == s->size)) {
        memcpy(&s_pool[s->offset], data, size);
        s->has_value = true;
    }
    eif_mutex_unlock(&s->lock);
}

/** @brief 試験専用。全スロットを解放し、プールを空にする。 */
void eif_cache_reset(void)
{
    for (size_t i = 0U; i < CACHE_SLOTS; i++) {
        if (s_slots[i].allocated) {
            (void)pthread_mutex_destroy(&s_slots[i].lock);
        }
    }
    memset(s_slots, 0, sizeof(s_slots));
    memset(s_pool, 0, sizeof(s_pool));
    s_used = 0U;
}
