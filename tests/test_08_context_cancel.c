#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Context cancellation

static int cancelled_at = -1;

static void cancellable_work(void *arg) {
    uint64_t ctx = *(uint64_t *)arg;

    for (int i = 0; i < 100; i++) {
        if (gt_ctx_is_done(ctx)) {
            cancelled_at = i;
            return;
        }
        gt_sleep(10);
    }

    cancelled_at = 100; // Completed without cancellation
}

int main(void) {
    printf("TEST: Context Cancellation\n");
    printf("==========================\n");

    gt_init();

    uint64_t bg = gt_ctx_background();
    uint64_t ctx = gt_ctx_with_cancel(bg);

    printf("Starting cancellable work...\n");
    uint64_t task = gt_spawn_void(cancellable_work, &ctx);

    // Let it run for a bit
    gt_sleep(50);

    printf("Cancelling context...\n");
    gt_ctx_cancel(ctx);

    gt_join(task);

    printf("Work cancelled at iteration: %d\n", cancelled_at);

    if (cancelled_at < 0) {
        printf("FAIL: Task did not run\n");
        return 1;
    }

    if (cancelled_at >= 100) {
        printf("FAIL: Task completed without cancellation\n");
        return 1;
    }

    if (cancelled_at < 3 || cancelled_at > 7) {
        printf("FAIL: Cancellation timing seems off (expected 3-7, got %d)\n",
               cancelled_at);
        return 1;
    }

    printf("PASS: Context cancellation works correctly\n");

    gt_ctx_destroy(ctx);
    gt_ctx_destroy(bg);
    gt_shutdown();
    return 0;
}
