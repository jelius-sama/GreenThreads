#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ========== EXAMPLE 1: Basic Goroutine Spawning ==========

void simple_task(void *arg) {
    int id = *(int *)arg;
    printf("[Task %d] Running in goroutine\n", id);
    gt_sleep(100);
    printf("[Task %d] Completed\n", id);
}

void demo_basic_spawning() {
    printf("\n=== Demo 1: Basic Goroutine Spawning ===\n");

    int id1 = 1, id2 = 2, id3 = 3;

    uint64_t t1 = gt_spawn_void(simple_task, &id1);
    uint64_t t2 = gt_spawn_void(simple_task, &id2);
    uint64_t t3 = gt_spawn_void(simple_task, &id3);

    printf("Spawned 3 tasks\n");

    gt_join(t1);
    gt_join(t2);
    gt_join(t3);

    printf("All tasks completed\n");
}

// ========== EXAMPLE 2: Returning Values from Goroutines ==========

int compute_factorial(void *arg) {
    int n = *(int *)arg;
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    gt_sleep(50); // Simulate work
    return result;
}

void *allocate_string(void *arg) {
    const char *input = (const char *)arg;
    char *result = malloc(strlen(input) + 20);
    sprintf(result, "Processed: %s", input);
    gt_sleep(100);
    return result;
}

void demo_return_values() {
    printf("\n=== Demo 2: Return Values ===\n");

    int n = 10;
    uint64_t task = gt_spawn_int(compute_factorial, &n);

    int factorial;
    gt_join_int(task, &factorial);
    printf("Factorial of %d = %d\n", n, factorial);

    const char *input = "Hello World";
    uint64_t str_task = gt_spawn_ptr(allocate_string, (void *)input);

    void *result_ptr;
    gt_join_ptr(str_task, &result_ptr);
    printf("Result: %s\n", (char *)result_ptr);
    free(result_ptr);
}

// ========== EXAMPLE 3: Channels for Communication ==========

typedef struct {
    uint64_t channel;
} channel_wrapper_t;

void producer(void *arg) {
    uint64_t ch = *(uint64_t *)arg;

    for (int i = 0; i < 5; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);

        printf("[Producer] Sending: %s\n", msg);
        gt_chan_send(ch, msg, strlen(msg) + 1);
        gt_sleep(100);
    }

    gt_chan_close(ch);
    printf("[Producer] Channel closed\n");
}

void consumer(void *arg) {
    uint64_t ch = *(uint64_t *)arg;
    char buf[64];
    uint32_t len;

    while (1) {
        int ret = gt_chan_recv(ch, buf, sizeof(buf), &len);

        if (ret == 2) { // GT_ERR_CLOSED
            printf("[Consumer] Channel closed, exiting\n");
            break;
        }

        if (ret == 0) { // GT_OK
            printf("[Consumer] Received: %s\n", buf);
        }
    }
}

void demo_channels() {
    printf("\n=== Demo 3: Channels ===\n");

    uint64_t ch = gt_chan_create(3); // Buffered channel

    uint64_t prod = gt_spawn_void(producer, &ch);
    uint64_t cons = gt_spawn_void(consumer, &ch);

    gt_join(prod);
    gt_join(cons);
}

// ========== EXAMPLE 4: WaitGroup Pattern ==========

typedef struct {
    int worker_id;
    uint64_t wg;
} worker_args_t;

void worker(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;

    printf("[Worker %d] Starting\n", args->worker_id);
    gt_sleep(50 + (args->worker_id * 20));
    printf("[Worker %d] Done\n", args->worker_id);

    gt_wg_done(args->wg);
    free(args);
}

void demo_waitgroup() {
    printf("\n=== Demo 4: WaitGroup ===\n");

    uint64_t wg = gt_wg_create();
    const int num_workers = 5;

    gt_wg_add(wg, num_workers);

    for (int i = 0; i < num_workers; i++) {
        worker_args_t *args = malloc(sizeof(worker_args_t));
        args->worker_id = i;
        args->wg = wg;

        gt_spawn_void(worker, args);
    }

    printf("Waiting for all workers...\n");
    gt_wg_wait(wg);
    printf("All workers completed\n");

    gt_wg_destroy(wg);
}

// ========== EXAMPLE 5: Mutex for Shared State ==========

typedef struct {
    uint64_t mutex;
    int *counter;
    int iterations;
} counter_args_t;

void increment_counter(void *arg) {
    counter_args_t *args = (counter_args_t *)arg;

    for (int i = 0; i < args->iterations; i++) {
        gt_mutex_lock(args->mutex);
        (*args->counter)++;
        gt_mutex_unlock(args->mutex);
    }
}

void demo_mutex() {
    printf("\n=== Demo 5: Mutex ===\n");

    uint64_t mutex = gt_mutex_create();
    int counter = 0;

    const int num_goroutines = 10;
    const int iterations = 1000;

    counter_args_t args = {
        .mutex = mutex, .counter = &counter, .iterations = iterations};

    uint64_t tasks[10];
    for (int i = 0; i < num_goroutines; i++) {
        tasks[i] = gt_spawn_void(increment_counter, &args);
    }

    for (int i = 0; i < num_goroutines; i++) {
        gt_join(tasks[i]);
    }

    printf("Final counter value: %d (expected: %d)\n", counter,
           num_goroutines * iterations);

    gt_mutex_destroy(mutex);
}

// ========== EXAMPLE 6: Context and Cancellation ==========

typedef struct {
    uint64_t ctx;
    int worker_id;
} ctx_worker_args_t;

void cancellable_worker(void *arg) {
    ctx_worker_args_t *args = (ctx_worker_args_t *)arg;

    printf("[Worker %d] Starting (cancellable)\n", args->worker_id);

    for (int i = 0; i < 10; i++) {
        if (gt_ctx_is_done(args->ctx)) {
            printf("[Worker %d] Cancelled at iteration %d\n", args->worker_id,
                   i);
            free(args);
            return;
        }

        printf("[Worker %d] Iteration %d\n", args->worker_id, i);
        gt_sleep(100);
    }

    printf("[Worker %d] Completed normally\n", args->worker_id);
    free(args);
}

void demo_context() {
    printf("\n=== Demo 6: Context and Cancellation ===\n");

    uint64_t bg = gt_ctx_background();
    uint64_t ctx = gt_ctx_with_cancel(bg);

    // Start 3 workers
    for (int i = 0; i < 3; i++) {
        ctx_worker_args_t *args = malloc(sizeof(ctx_worker_args_t));
        args->ctx = ctx;
        args->worker_id = i;
        gt_spawn_void(cancellable_worker, args);
    }

    // Let them run for a bit
    gt_sleep(350);

    // Cancel all workers
    printf("\n[Main] Cancelling context\n");
    gt_ctx_cancel(ctx);

    gt_sleep(200); // Give them time to finish

    gt_ctx_destroy(ctx);
    gt_ctx_destroy(bg);
}

// ========== EXAMPLE 7: Timers ==========

void demo_timers() {
    printf("\n=== Demo 7: Timers ===\n");

    printf("Setting timer for 500ms...\n");
    uint64_t timer = gt_timer_create(500);
    uint64_t timer_ch = gt_timer_chan(timer);

    char buf[1];
    uint32_t len;

    printf("Waiting for timer...\n");
    gt_chan_recv(timer_ch, buf, sizeof(buf), &len);
    printf("Timer fired!\n");

    gt_timer_destroy(timer);
}

// ========== EXAMPLE 8: Tickers ==========

void demo_tickers() {
    printf("\n=== Demo 8: Tickers ===\n");

    uint64_t ticker = gt_ticker_create(200);
    uint64_t tick_ch = gt_ticker_chan(ticker);

    char buf[1];
    uint32_t len;

    printf("Ticker started (200ms interval)\n");

    for (int i = 0; i < 5; i++) {
        gt_chan_recv(tick_ch, buf, sizeof(buf), &len);
        printf("Tick %d at %lld ms\n", i, (long long)gt_now_unix_ms());
    }

    gt_ticker_stop(ticker);
    gt_ticker_destroy(ticker);
    printf("Ticker stopped\n");
}

// ========== EXAMPLE 9: Pipeline Pattern ==========

void stage1(void *arg) {
    uint64_t out = *(uint64_t *)arg;

    for (int i = 1; i <= 5; i++) {
        printf("[Stage1] Producing %d\n", i);
        gt_chan_send(out, &i, sizeof(i));
        gt_sleep(100);
    }

    gt_chan_close(out);
}

typedef struct {
    uint64_t in;
    uint64_t out;
} stage2_args_t;

void stage2(void *arg) {
    stage2_args_t *args = (stage2_args_t *)arg;
    uint64_t in = args->in;
    uint64_t out = args->out;

    char buf[64];
    uint32_t len;

    while (1) {
        int ret = gt_chan_recv(in, buf, sizeof(buf), &len);

        if (ret == 2) { // GT_ERR_CLOSED
            gt_chan_close(out);
            break;
        }

        if (ret == 0) { // GT_OK
            int val = *(int *)buf;
            int squared = val * val;
            printf("[Stage2] %d -> %d\n", val, squared);
            gt_chan_send(out, &squared, sizeof(squared));
        }
    }
}

void stage3(void *arg) {
    uint64_t in = *(uint64_t *)arg;

    char buf[64];
    uint32_t len;
    int sum = 0;

    while (1) {
        int ret = gt_chan_recv(in, buf, sizeof(buf), &len);

        if (ret == 2) { // GT_ERR_CLOSED
            break;
        }

        if (ret == 0) { // GT_OK
            int val = *(int *)buf;
            sum += val;
            printf("[Stage3] Accumulated: %d (total: %d)\n", val, sum);
        }
    }

    printf("[Stage3] Final sum: %d\n", sum);
}

void demo_pipeline() {
    printf("\n=== Demo 9: Pipeline Pattern ===\n");

    uint64_t ch1 = gt_chan_create(2);
    uint64_t ch2 = gt_chan_create(2);

    uint64_t s1 = gt_spawn_void(stage1, &ch1);

    stage2_args_t stage2_args = {ch1, ch2};
    uint64_t s2 = gt_spawn_void(stage2, &stage2_args);

    uint64_t s3 = gt_spawn_void(stage3, &ch2);

    gt_join(s1);
    gt_join(s2);
    gt_join(s3);
}

// ========== EXAMPLE 10: Fan-out/Fan-in ==========

typedef struct {
    uint64_t in;
    uint64_t out;
    int worker_id;
    uint64_t wg;
} fanout_args_t;

void fanout_worker(void *arg) {
    fanout_args_t *args = (fanout_args_t *)arg;

    char buf[64];
    uint32_t len;

    while (1) {
        int ret = gt_chan_recv(args->in, buf, sizeof(buf), &len);

        if (ret == 2) { // GT_ERR_CLOSED
            printf("[Worker %d] Input closed, exiting\n", args->worker_id);
            break;
        }

        if (ret == 0) { // GT_OK
            int val = *(int *)buf;
            int result = val * args->worker_id;

            printf("[Worker %d] Processing %d -> %d\n", args->worker_id, val,
                   result);

            gt_sleep(50 + (args->worker_id * 10));

            printf("[Worker %d] Sending result %d\n", args->worker_id, result);
            gt_chan_send(args->out, &result, sizeof(result));
            printf("[Worker %d] Result sent\n", args->worker_id);
        }
    }

    printf("[Worker %d] Calling wg_done\n", args->worker_id);
    gt_wg_done(args->wg);
    free(args);
}

typedef struct {
    uint64_t wg;
    uint64_t output;
} closer_args_t;

void output_closer(void *arg) {
    closer_args_t *args = (closer_args_t *)arg;
    gt_wg_wait(args->wg);
    gt_chan_close(args->output);
    free(args);
}

void producer_function(void *arg) {
    uint64_t ch = *(uint64_t *)arg;
    printf("[Producer] Starting, will send 9 items\n");
    for (int i = 1; i <= 9; i++) {
        printf("[Producer] Sending %d\n", i);
        int value = i; // Copy to ensure stable address
        int ret = gt_chan_send(ch, &value, sizeof(value));
        if (ret != 0) {
            printf("[Producer] Send failed with error %d\n", ret);
            break;
        }
        printf("[Producer] Sent %d successfully\n", i);
    }
    printf("[Producer] Closing input channel\n");
    gt_chan_close(ch);
    printf("[Producer] Producer finished\n");
}

void demo_fanout_fanin() {
    printf("\n=== Demo 10: Fan-out/Fan-in ===\n");

    uint64_t input = gt_chan_create(10);
    uint64_t output = gt_chan_create(10);
    uint64_t wg = gt_wg_create();

    const int num_workers = 3;
    gt_wg_add(wg, num_workers);

    // Start workers (fan-out)
    for (int i = 0; i < num_workers; i++) {
        fanout_args_t *args = malloc(sizeof(fanout_args_t));
        args->in = input;
        args->out = output;
        args->worker_id = i + 1;
        args->wg = wg;

        gt_spawn_void(fanout_worker, args);
    }

    // Start producer
    uint64_t producer_task = gt_spawn_void(producer_function, &input);

    // Start output closer
    closer_args_t *closer = malloc(sizeof(closer_args_t));
    closer->wg = wg;
    closer->output = output;
    uint64_t collector = gt_spawn_void(output_closer, closer);

    // Read all results (fan-in)
    char buf[64];
    uint32_t len;
    int total = 0;
    int results_received = 0;

    printf("[Collector] Waiting for results...\n");

    while (1) {
        int ret = gt_chan_recv(output, buf, sizeof(buf), &len);

        if (ret == 2) { // GT_ERR_CLOSED
            printf("[Collector] Channel closed after %d results\n",
                   results_received);
            break;
        }

        if (ret == 0) { // GT_OK
            int val = *(int *)buf;
            total += val;
            results_received++;
            printf("[Collector] Got result %d: %d\n", results_received, val);
        }
    }

    printf("[Collector] Total: %d\n", total);

    printf("[Main] Waiting for producer to finish...\n");
    gt_join(producer_task);
    printf("[Main] Producer finished\n");

    printf("[Main] Waiting for closer to finish...\n");
    gt_join(collector);
    printf("[Main] Closer finished\n");

    gt_wg_destroy(wg);
    printf("[Main] Fan-out/Fan-in demo completed\n");
}

// ========== EXAMPLE 11: Try Send/Recv (Non-blocking) ==========

void demo_non_blocking() {
    printf("\n=== Demo 11: Non-blocking Operations ===\n");

    uint64_t ch = gt_chan_create(2); // Small buffer

    // Fill the channel
    int val1 = 100, val2 = 200;
    printf("Sending first two values...\n");
    gt_chan_send(ch, &val1, sizeof(val1));
    gt_chan_send(ch, &val2, sizeof(val2));

    // Try to send when full (should fail)
    int val3 = 300;
    int ret = gt_chan_try_send(ch, &val3, sizeof(val3));
    if (ret == 5) { // GT_ERR_WOULD_BLOCK
        printf("Try send failed: channel full (expected)\n");
    }

    // Try to receive
    char buf[64];
    uint32_t len;
    ret = gt_chan_try_recv(ch, buf, sizeof(buf), &len);
    if (ret == 0) { // GT_OK
        printf("Try recv succeeded: got %d\n", *(int *)buf);
    }

    // Now we can send
    ret = gt_chan_try_send(ch, &val3, sizeof(val3));
    if (ret == 0) {
        printf("Try send succeeded: sent %d\n", val3);
    }

    printf("Channel len: %u, cap: %u\n", gt_chan_len(ch), gt_chan_cap(ch));

    gt_chan_close(ch);
}

// ========== EXAMPLE 12: Task Status Check ==========

void slow_task(void *arg) {
    int duration = *(int *)arg;
    printf("[Slow Task] Running for %d ms\n", duration);
    gt_sleep(duration);
    printf("[Slow Task] Completed\n");
}

void demo_task_status() {
    printf("\n=== Demo 12: Task Status Checking ===\n");

    int duration = 500;
    uint64_t task = gt_spawn_void(slow_task, &duration);

    // Poll for completion
    for (int i = 0; i < 10; i++) {
        if (gt_task_done(task)) {
            printf("Task completed at check %d\n", i);
            break;
        }
        printf("Task still running (check %d)\n", i);
        gt_sleep(100);
    }

    gt_join(task);
}

// ========== EXAMPLE 13: Timeout Context ==========

void timeout_sensitive_work(void *arg) {
    uint64_t ctx = *(uint64_t *)arg;

    for (int i = 0; i < 20; i++) {
        if (gt_ctx_is_done(ctx)) {
            printf("[Work] Timed out at iteration %d\n", i);
            return;
        }

        printf("[Work] Processing iteration %d\n", i);
        gt_sleep(100);
    }

    printf("[Work] Completed all work\n");
}

void demo_context_timeout() {
    printf("\n=== Demo 13: Context with Timeout ===\n");

    uint64_t bg = gt_ctx_background();
    uint64_t ctx = gt_ctx_with_timeout(bg, 500); // 500ms timeout

    uint64_t task = gt_spawn_void(timeout_sensitive_work, &ctx);

    gt_join(task);

    gt_ctx_destroy(ctx);
    gt_ctx_destroy(bg);
}

// ========== MAIN ==========

int main(void) {
    printf("=================================================\n");
    printf("Go Green Threads FFI Library - Comprehensive Demo\n");
    printf("=================================================\n");

    gt_init();

    printf("\nCPU Count: %u\n", gt_num_cpu());
    printf("Initial Goroutines: %u\n", gt_num_goroutine());

    demo_basic_spawning();
    demo_return_values();
    demo_channels();
    demo_waitgroup();
    demo_mutex();
    demo_context();
    demo_timers();
    demo_tickers();
    demo_pipeline();
    // demo_fanout_fanin();
    demo_non_blocking();
    demo_task_status();
    demo_context_timeout();

    printf("\n=== All Demos Completed ===\n");
    printf("Final Goroutines: %u\n", gt_num_goroutine());

    gt_shutdown();

    return 0;
}
