#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>

static jmp_buf jump_buffer;
static void segfault_handler(int sig) {
    longjmp(jump_buffer, 1);
}

typedef struct {
    char prefix[155];
    char filename[100];
} tar_header_t;

extern void extract_tar_entry(tar_header_t *header);

START_TEST(test_tar_buffer_overflow_boundary)
{
    // Invariant: tar extraction must not corrupt stack or crash on adversarial header inputs
    
    tar_header_t headers[] = {
        // Exploit: combined length 255+ bytes (overflow)
        {.prefix = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         .filename = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        
        // Boundary: exactly 254 bytes (should be safe if buffer is 255)
        {.prefix = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
         .filename = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        
        // Valid: short paths
        {.prefix = "usr/local", .filename = "bin/tool"}
    };
    
    int num_headers = sizeof(headers) / sizeof(headers[0]);
    
    signal(SIGSEGV, segfault_handler);
    signal(SIGABRT, segfault_handler);
    
    for (int i = 0; i < num_headers; i++) {
        if (setjmp(jump_buffer) == 0) {
            extract_tar_entry(&headers[i]);
            ck_assert_msg(1, "No crash on input %d", i);
        } else {
            ck_abort_msg("Stack corruption or crash detected on input %d", i);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tar_buffer_overflow_boundary);
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