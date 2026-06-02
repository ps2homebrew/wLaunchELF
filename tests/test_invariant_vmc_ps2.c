#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Include the production header/source under test */
#include "vmc_fs/vmc_ps2.h"

/* page_size values to test:
 * 1. Exact exploit: 0xFFFFFF01 — addition wraps to 0x00 (tiny alloc, huge write)
 * 2. Boundary:      0xFFFFFF00 — addition wraps to 0xFF (near-wrap)
 * 3. Valid input:   0x00000200 — normal 512-byte page size
 */
static const uint32_t page_sizes[] = {
    0xFFFFFF01U,  /* exact exploit payload */
    0xFFFFFF00U,  /* boundary wrap case    */
    0x00000200U,  /* valid/normal input     */
};

START_TEST(test_page_size_no_overflow)
{
    /* Invariant: the allocated buffer must be >= page_size bytes;
     * if the addition overflows, the allocation is too small and
     * any subsequent write would be an out-of-bounds access.
     * We verify the alignment calculation never produces a result
     * smaller than the original page_size (i.e., no wrap-around). */

    uint32_t page_size = page_sizes[_i];

    /* Replicate the vulnerable expression to detect overflow */
    uint32_t aligned = (page_size + 0xFFU) & ~(uint32_t)0xFFU;

    /* Security invariant: aligned size must be >= page_size.
     * If overflow occurred, aligned < page_size, which means
     * malloc would allocate a buffer too small for the data. */
    ck_assert_msg(aligned >= page_size,
        "OVERFLOW DETECTED: page_size=0x%08X aligned=0x%08X — "
        "buffer would be undersized, heap overflow possible",
        page_size, aligned);
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_loop_test(tc_core, test_page_size_no_overflow,
                        0, (int)(sizeof(page_sizes) / sizeof(page_sizes[0])));
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}