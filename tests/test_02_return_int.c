#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Goroutine returning an integer value

int compute_sum(void *arg) {
    int n = *(int *)arg;
    int sum = 0;
    for (int i = 1; i <= n; i++) {
        sum += i;
    }
    gt_sleep(50);
    return sum;
}

int main(void) {
    printf("TEST: Return Integer from Goroutine\n");
    printf("====================================\n");

    gt_init();

    int n = 10;
    int expected = 55; // Sum of 1..10

    uint64_t task = gt_spawn_int(compute_sum, &n);

    int result;
    int ret = gt_join_int(task, &result);

    if (ret != 0) {
        printf("FAIL: gt_join_int returned error code %d\n", ret);
        return 1;
    }

    printf("Expected: %d, Got: %d\n", expected, result);

    if (result != expected) {
        printf("FAIL: Result mismatch\n");
        return 1;
    }

    printf("PASS: Integer return value works correctly\n");

    gt_shutdown();
    return 0;
}
