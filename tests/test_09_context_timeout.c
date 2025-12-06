#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Context with timeout

static int work_iterations = 0;

static void timeout_work(void *arg) {
    uint64_t ctx = *(uint64_t *)arg;

    while (!gt_ctx_is_done(ctx)) {
        work_iterations++;
        gt_sleep(50);
    }
}

int main(void) {
    printf("TEST: Context Timeout\n");
    printf("=====================\n");

    gt_init();

    uint64_t bg = gt_ctx_background();
    uint64_t ctx = gt_ctx_with_timeout(bg, 200); // 200ms timeout

    printf("Starting work with 200ms timeout...\n");
    uint64_t task = gt_spawn_void(timeout_work, &ctx);

    gt_join(task);

    printf("Work completed %d iterations\n", work_iterations);

    // Should complete around 4 iterations (200ms / 50ms per iteration)
    if (work_iterations < 3 || work_iterations > 5) {
        printf("FAIL: Expected 3-5 iterations, got %d\n", work_iterations);
        return 1;
    }

    printf("PASS: Context timeout works correctly\n");

    gt_ctx_destroy(ctx);
    gt_ctx_destroy(bg);
    gt_shutdown();
    return 0;
}
