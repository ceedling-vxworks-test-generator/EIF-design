/**
 * @file eif_log.h
 * @brief Frameのログ出力。
 *
 * EIF_USE_UT_LOG を定義すると foundation/log（UT_LOG_*）へ出す。未定義なら標準エラー出力へ出す。
 */
#ifndef EIF_LOG_H
#define EIF_LOG_H

/** @brief ログのモジュール名。 */
#define EIF_LOG_MODULE "eif"

/*
 * EIF_LOG_ERROR(fmt, ...) … 異常（処理を続けられない、データを捨てた）
 * EIF_LOG_WARN(fmt, ...)  … 想定外だが処理は続ける
 * EIF_LOG_INFO(fmt, ...)  … 動作の記録（標準エラー出力の場合は出さない）
 * EIF_LOG_SILENT を定義すると何も出さない（試験用）。
 */

#if defined(EIF_USE_UT_LOG)
#include "utility_log.h"
#define EIF_LOG_ERROR(...) UT_LOG_ERROR(EIF_LOG_MODULE, __VA_ARGS__)
#define EIF_LOG_WARN(...)  UT_LOG_WARN(EIF_LOG_MODULE, __VA_ARGS__)
#define EIF_LOG_INFO(...)  UT_LOG_INFO(EIF_LOG_MODULE, __VA_ARGS__)
#elif defined(EIF_LOG_SILENT)
#define EIF_LOG_ERROR(...) ((void)0)
#define EIF_LOG_WARN(...)  ((void)0)
#define EIF_LOG_INFO(...)  ((void)0)
#else
#include <stdio.h>
#define EIF_LOG_ERROR(...) ((void)fprintf(stderr, "[EIF][E] " __VA_ARGS__), (void)fputc('\n', stderr))
#define EIF_LOG_WARN(...)  ((void)fprintf(stderr, "[EIF][W] " __VA_ARGS__), (void)fputc('\n', stderr))
#define EIF_LOG_INFO(...)  ((void)0)
#endif

#endif /* EIF_LOG_H */
