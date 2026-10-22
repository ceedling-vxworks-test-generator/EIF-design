/**
 * @file eif_lock.h
 * @brief Frameが使う排他の薄いラッパー。
 *
 * 既定はPOSIXスレッド。製品のOS抽象（platform/Os）へ寄せる場合は本ファイルだけを差し替える。
 */
#ifndef EIF_LOCK_H
#define EIF_LOCK_H

#include <pthread.h>

typedef pthread_mutex_t eif_mutex_t;

#define EIF_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline void eif_mutex_lock(eif_mutex_t *m)
{
    (void)pthread_mutex_lock(m);
}

static inline void eif_mutex_unlock(eif_mutex_t *m)
{
    (void)pthread_mutex_unlock(m);
}

#endif /* EIF_LOCK_H */
