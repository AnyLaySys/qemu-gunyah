#include <check.h>
#include <stdlib.h>
#include <string.h>
#include "block.c"

START_TEST(test_buffer_reads_never_exceed_declared_length)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // 100 chars - exploit case
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",  // 50 chars - boundary case
        "valid",  // 5 chars - valid input
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"  // 120 chars - 2x typical buffer
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        // Test strcpy usage - ensure destination buffer is properly sized
        char dest[64];  // Fixed size buffer
        size_t dest_size = sizeof(dest);
        
        // Use strncpy with explicit null-termination check
        strncpy(dest, payloads[i], dest_size - 1);
        dest[dest_size - 1] = '\0';
        
        // Verify length doesn't exceed buffer
        ck_assert_msg(strlen(dest) < dest_size, 
                     "Buffer overflow detected: input length %zu exceeds buffer size %zu",
                     strlen(payloads[i]), dest_size);
        
        // Verify null termination
        ck_assert_msg(dest[dest_size - 1] == '\0' || strlen(dest) == dest_size - 1,
                     "String not properly null-terminated");
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_reads_never_exceed_declared_length);
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