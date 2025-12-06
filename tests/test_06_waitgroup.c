#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: WaitGroup coordination

typedef struct {
    int id;
    uint64_t wg;
    int *counter;
} worker_args_t;

static void worker(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;

    gt_sleep(50);
    (*args->counter)++;

    gt_wg_done(args->wg);
    free(args);
}

int main(void) {
    printf("TEST: WaitGroup\n");
    printf("===============\n");

    gt_init();

    uint64_t wg = gt_wg_create();
    int counter = 0;
    const int num_workers = 5;

    gt_wg_add(wg, num_workers);

    printf("Starting %d workers...\n", num_workers);

    for (int i = 0; i < num_workers; i++) {
        worker_args_t *args = malloc(sizeof(worker_args_t));
        args->id = i;
        args->wg = wg;
        args->counter = &counter;

        gt_spawn_void(worker, args);
    }

    printf("Waiting for workers...\n");
    gt_wg_wait(wg);

    printf("All workers completed. Counter = %d\n", counter);

    if (counter != num_workers) {
        printf("FAIL: Expected counter=%d, got %d\n", num_workers, counter);
        return 1;
    }

    printf("PASS: WaitGroup works correctly\n");

    gt_wg_destroy(wg);
    gt_shutdown();
    return 0;
}
