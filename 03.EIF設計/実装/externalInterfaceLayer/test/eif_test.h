/**
 * @file eif_test.h
 * @brief 依存の無い最小の試験マクロ。
 */
#ifndef EIF_TEST_H
#define EIF_TEST_H

#include <stdio.h>

extern int g_test_failures;
extern int g_test_runs;

#define TEST_ASSERT(cond)                                                          \
    do {                                                                           \
        if (!(cond)) {                                                             \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            g_test_failures++;                                                     \
            return;                                                                \
        }                                                                          \
    } while (0)

#define TEST_EQ(a, b) TEST_ASSERT((a) == (b))

#define RUN_TEST(setup, fn)                                                        \
    do {                                                                           \
        const int before_ = g_test_failures;                                       \
        setup();                                                                   \
        fn();                                                                      \
        printf("%s %s\n", (before_ == g_test_failures) ? "[PASS]" : "[FAIL]", #fn); \
        g_test_runs++;                                                             \
    } while (0)

#define TEST_SUMMARY()                                                             \
    (printf("\n%d tests, %d failures\n", g_test_runs, g_test_failures), (g_test_failures == 0) ? 0 : 1)

#endif /* EIF_TEST_H */
