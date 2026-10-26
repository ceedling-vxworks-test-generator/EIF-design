/**
 * @file eif_lock.h
 * @brief Frameが使う排他の薄いラッパー。
 *
 * 既定はPOSIXスレッド。製品のOS抽象（platform/Os）へ寄せる場合は本ファイルだけを差し替える。
 */
#ifndef EIF_LOCK_H
#define EIF_LOCK_H

#include <pthread.h>

/** @brief mutexの型。 */
typedef pthread_mutex_t eif_mutex_t;

/** @brief 静的に初期化するときの初期値。 */
#define EIF_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

/**
 * @brief mutexを取る（取れるまで待つ）。
 * @param[in,out] m 対象のmutex
 */
static inline void eif_mutex_lock(eif_mutex_t *m)
{
    (void)pthread_mutex_lock(m);
}

/**
 * @brief mutexを放す。
 * @param[in,out] m 対象のmutex（eif_mutex_lock() で取ったもの）
 */
static inline void eif_mutex_unlock(eif_mutex_t *m)
{
    (void)pthread_mutex_unlock(m);
}

#endif /* EIF_LOCK_H */
