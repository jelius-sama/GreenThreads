#include "libgtruntime.h"
#include <stdio.h>

// Test: Mutex for protecting shared state

typedef struct {
    uint64_t mutex;
    int *counter;
} mutex_args_t;

static void increment_many(void *arg) {
    mutex_args_t *args = (mutex_args_t *)arg;

    for (int i = 0; i < 1000; i++) {
        gt_mutex_lock(args->mutex);
        (*args->counter)++;
        gt_mutex_unlock(args->mutex);
    }
}

int main(void) {
    printf("TEST: Mutex\n");
    printf("===========\n");

    gt_init();

    uint64_t mutex = gt_mutex_create();
    int counter = 0;
    const int num_threads = 10;
    const int increments = 1000;
    const int expected = num_threads * increments;

    mutex_args_t args = {.mutex = mutex, .counter = &counter};

    printf("Starting %d goroutines, each incrementing %d times...\n",
           num_threads, increments);

    uint64_t tasks[10];
    for (int i = 0; i < num_threads; i++) {
        tasks[i] = gt_spawn_void(increment_many, &args);
    }

    for (int i = 0; i < num_threads; i++) {
        gt_join(tasks[i]);
    }

    printf("Final counter: %d (expected: %d)\n", counter, expected);

    if (counter != expected) {
        printf("FAIL: Counter mismatch (possible race condition)\n");
        return 1;
    }

    printf("PASS: Mutex correctly protects shared state\n");

    gt_mutex_destroy(mutex);
    gt_shutdown();
    return 0;
}
