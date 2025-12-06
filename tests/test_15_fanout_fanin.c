#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Fan-out/Fan-in pattern (the critical test that was failing)

typedef struct {
    uint64_t in;
    uint64_t out;
    int worker_id;
    uint64_t wg;
} worker_args_t;

static void worker(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    char buf[64];
    uint32_t len;

    while (1) {
        int ret = gt_chan_recv(args->in, buf, sizeof(buf), &len);

        if (ret == 2) { // Closed
            break;
        }

        if (ret == 0) {
            int val = *(int *)buf;
            int result = val * args->worker_id;
            gt_chan_send(args->out, &result, sizeof(result));
        }
    }

    gt_wg_done(args->wg);
    free(args);
}

static void producer(void *arg) {
    uint64_t ch = *(uint64_t *)arg;

    for (int i = 1; i <= 9; i++) {
        gt_chan_send(ch, &i, sizeof(i));
        gt_sleep(10);
    }

    gt_chan_close(ch);
}

typedef struct {
    uint64_t wg;
    uint64_t output;
} closer_args_t;

static void closer(void *arg) {
    closer_args_t *args = (closer_args_t *)arg;
    gt_wg_wait(args->wg);
    gt_chan_close(args->output);
    free(args);
}

int main(void) {
    printf("TEST: Fan-out/Fan-in Pattern\n");
    printf("============================\n");

    gt_init();

    uint64_t input = gt_chan_create(10);
    uint64_t output = gt_chan_create(10);
    uint64_t wg = gt_wg_create();

    const int num_workers = 3;
    gt_wg_add(wg, num_workers);

    printf("Starting %d workers...\n", num_workers);

    for (int i = 0; i < num_workers; i++) {
        worker_args_t *args = malloc(sizeof(worker_args_t));
        args->in = input;
        args->out = output;
        args->worker_id = i + 1;
        args->wg = wg;

        gt_spawn_void(worker, args);
    }

    printf("Starting producer...\n");
    uint64_t prod = gt_spawn_void(producer, &input);

    printf("Starting closer...\n");
    closer_args_t *closer_args = malloc(sizeof(closer_args_t));
    closer_args->wg = wg;
    closer_args->output = output;
    uint64_t cls = gt_spawn_void(closer, closer_args);

    printf("Collecting results...\n");

    char buf[64];
    uint32_t len;
    int results_received = 0;
    int total = 0;

    while (1) {
        int ret = gt_chan_recv(output, buf, sizeof(buf), &len);

        if (ret == 2) { // Closed
            break;
        }

        if (ret == 0) {
            int val = *(int *)buf;
            total += val;
            results_received++;
            printf("  Result %d: %d\n", results_received, val);
        }
    }

    gt_join(prod);
    gt_join(cls);

    printf("Received %d results, total: %d\n", results_received, total);

    if (results_received != 9) {
        printf("FAIL: Expected 9 results, got %d\n", results_received);
        return 1;
    }

    // Total should be: sum of (i * worker_id) for i=1..9, distributed among 3
    // workers Exact total depends on which worker processes which item But it
    // should be reasonable
    if (total < 45 || total > 135) {
        printf("FAIL: Total sum %d is out of expected range (45-135)\n", total);
        return 1;
    }

    printf("PASS: Fan-out/Fan-in works correctly\n");

    gt_wg_destroy(wg);
    gt_shutdown();
    return 0;
}
