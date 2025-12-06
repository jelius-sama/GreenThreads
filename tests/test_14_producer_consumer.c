#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Producer-Consumer pattern

static void producer(void *arg) {
    uint64_t ch = *(uint64_t *)arg;

    for (int i = 1; i <= 10; i++) {
        gt_chan_send(ch, &i, sizeof(i));
        gt_sleep(10);
    }

    gt_chan_close(ch);
}

static int total_received = 0;
static int last_value = 0;

static void consumer(void *arg) {
    uint64_t ch = *(uint64_t *)arg;
    char buf[64];
    uint32_t len;

    while (1) {
        int ret = gt_chan_recv(ch, buf, sizeof(buf), &len);

        if (ret == 2) { // Channel closed
            break;
        }

        if (ret == 0) {
            int val = *(int *)buf;
            total_received++;
            last_value = val;
        }
    }
}

int main(void) {
    printf("TEST: Producer-Consumer Pattern\n");
    printf("================================\n");

    gt_init();

    uint64_t ch = gt_chan_create(5);

    printf("Starting producer and consumer...\n");

    uint64_t prod = gt_spawn_void(producer, &ch);
    uint64_t cons = gt_spawn_void(consumer, &ch);

    gt_join(prod);
    gt_join(cons);

    printf("Received %d items, last value: %d\n", total_received, last_value);

    if (total_received != 10) {
        printf("FAIL: Expected 10 items, got %d\n", total_received);
        return 1;
    }

    if (last_value != 10) {
        printf("FAIL: Expected last value 10, got %d\n", last_value);
        return 1;
    }

    printf("PASS: Producer-Consumer works correctly\n");

    gt_shutdown();
    return 0;
}
