/**
 * @file eif_test.h
 * @brief 依存の無い最小の試験マクロ。
 *
 * 使い方：試験関数の中で TEST_ASSERT／TEST_EQ を使い、main から RUN_TEST(前処理, 試験関数) で実行し、
 * 最後に TEST_SUMMARY() の値を main の戻り値にする。
 */
#ifndef EIF_TEST_H
#define EIF_TEST_H

#include <stdio.h>

/** @brief 失敗した確認の数（試験ファイルが定義する）。 */
extern int g_test_failures;
/** @brief 実行した試験関数の数（試験ファイルが定義する）。 */
extern int g_test_runs;

/**
 * @brief 条件を確認する。偽なら失敗を記録し、その試験関数から抜ける。
 * @param cond 確認する条件
 */
#define TEST_ASSERT(cond)                                                          \
    do {                                                                           \
        if (!(cond)) {                                                             \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            g_test_failures++;                                                     \
            return;                                                                \
        }                                                                          \
    } while (0)

/**
 * @brief 2つの値が等しいことを確認する。
 * @param a 実際の値
 * @param b 期待する値
 */
#define TEST_EQ(a, b) TEST_ASSERT((a) == (b))

/**
 * @brief 前処理と試験関数を実行し、[PASS]／[FAIL] を表示する。
 * @param setup 前処理（状態の初期化）
 * @param fn    試験関数
 */
#define RUN_TEST(setup, fn)                                                        \
    do {                                                                           \
        const int before_ = g_test_failures;                                       \
        setup();                                                                   \
        fn();                                                                      \
        printf("%s %s\n", (before_ == g_test_failures) ? "[PASS]" : "[FAIL]", #fn); \
        g_test_runs++;                                                             \
    } while (0)

/**
 * @brief 結果を表示し、終了コードを返す。
 * @return 失敗が無ければ0、あれば1
 */
#define TEST_SUMMARY()                                                             \
    (printf("\n%d tests, %d failures\n", g_test_runs, g_test_failures), (g_test_failures == 0) ? 0 : 1)

#endif /* EIF_TEST_H */
