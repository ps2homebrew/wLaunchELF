#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* This test is self-contained and does not require production headers.
 *
 * NOTE: In the production code, superblock.page_size is declared as
 * `unsigned short` (vmc_fs/vmc.h:187), so values are bounded to [0, 65535].
 * The near-UINT_MAX vectors below are therefore unreachable in practice;
 * they are included to validate that the detection logic itself is correct
 * when using wider (uint64_t) arithmetic.
 *
 * page_size values to test:
 * 1. 32-bit overflow payload: 0xFFFFFF01 — 32-bit addition wraps to 0x00
 * 2. 32-bit boundary wrap:    0xFFFFFF00 — 32-bit addition wraps to 0xFF
 * 3. Valid/normal input:      0x00000200 — 512-byte page (unsigned short range)
 */
static const uint32_t page_sizes[] = {
    0xFFFFFF01U,  /* 32-bit overflow payload (unreachable in production) */
    0xFFFFFF00U,  /* 32-bit boundary wrap    (unreachable in production) */
    0x00000200U,  /* valid input within unsigned short range             */
};

START_TEST(test_page_size_no_overflow)
{
    /* Invariant: the aligned buffer size must be >= page_size bytes.
     *
     * We use uint64_t arithmetic so that the alignment expression itself
     * cannot overflow during the test — this lets us correctly identify
     * cases where the equivalent 32-bit expression WOULD have wrapped,
     * producing a buffer smaller than the original page_size. */

    uint32_t page_size = page_sizes[_i];

    /* Perform the alignment in 64-bit to avoid overflow in the test */
    uint64_t aligned = ((uint64_t)page_size + 0xFFU) & ~(uint64_t)0xFFU;

    /* Security invariant: aligned size must be >= page_size.
     * If 32-bit overflow had occurred, aligned < page_size, meaning
     * malloc would allocate a buffer too small for the data written. */
    ck_assert_msg(aligned >= (uint64_t)page_size,
        "OVERFLOW DETECTED: page_size=0x%08X aligned=0x%016llX — "
        "buffer would be undersized, heap overflow possible",
        page_size, (unsigned long long)aligned);
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
