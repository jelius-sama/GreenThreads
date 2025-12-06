#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Task status checking

static void slow_task(void *arg) {
    int duration = *(int *)arg;
    gt_sleep(duration);
}

int main(void) {
    printf("TEST: Task Status Checking\n");
    printf("==========================\n");

    gt_init();

    int duration = 300;
    uint64_t task = gt_spawn_void(slow_task, &duration);

    printf("Started task with 300ms duration\n");

    // Check status immediately - should not be done
    if (gt_task_done(task)) {
        printf("FAIL: Task reported done immediately\n");
        return 1;
    }

    printf("Task correctly reported as running\n");

    // Wait a bit
    gt_sleep(100);

    // Should still not be done
    if (gt_task_done(task)) {
        printf("FAIL: Task reported done too early (at 100ms)\n");
        return 1;
    }

    printf("Task still running at 100ms\n");

    // Wait for completion
    gt_sleep(250);

    // Should be done now
    if (!gt_task_done(task)) {
        printf("FAIL: Task not done after 350ms total\n");
        return 1;
    }

    printf("Task correctly reported as done after completion\n");

    gt_join(task);

    printf("PASS: Task status checking works correctly\n");

    gt_shutdown();
    return 0;
}
