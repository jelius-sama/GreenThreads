#include "libgtruntime.h"
#include <stdio.h>

// Test: Basic goroutine spawning and joining

static int task_ran = 0;

void simple_task(void *arg) {
    int id = *(int *)arg;
    task_ran = id;
    gt_sleep(50);
}

int main(void) {
    printf("TEST: Basic Goroutine Spawn and Join\n");
    printf("=====================================\n");

    gt_init();

    int id = 42;
    uint64_t task = gt_spawn_void(simple_task, &id);

    if (task == 0) {
        printf("FAIL: Task handle is 0 (invalid)\n");
        return 1;
    }

    printf("Spawned task with handle: %llu\n", (unsigned long long)task);

    int ret = gt_join(task);

    if (ret != 0) {
        printf("FAIL: gt_join returned error code %d\n", ret);
        return 1;
    }

    if (task_ran != 42) {
        printf("FAIL: Task did not run (task_ran = %d, expected 42)\n",
               task_ran);
        return 1;
    }

    printf("PASS: Task spawned, executed, and joined successfully\n");

    gt_shutdown();
    return 0;
}
