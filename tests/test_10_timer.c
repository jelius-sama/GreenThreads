#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Timer

int main(void) {
    printf("TEST: Timer\n");
    printf("===========\n");

    gt_init();

    printf("Creating timer for 200ms...\n");
    int64_t start = gt_now_unix_ms();

    uint64_t timer = gt_timer_create(200);
    uint64_t timer_ch = gt_timer_chan(timer);

    printf("Waiting for timer...\n");

    char buf[1];
    uint32_t len;
    int ret = gt_chan_recv(timer_ch, buf, sizeof(buf), &len);

    int64_t end = gt_now_unix_ms();
    int64_t elapsed = end - start;

    if (ret != 0) {
        printf("FAIL: Timer channel recv failed with error %d\n", ret);
        return 1;
    }

    printf("Timer fired after %lld ms\n", (long long)elapsed);

    if (elapsed < 180 || elapsed > 250) {
        printf("FAIL: Timer timing is off (expected ~200ms, got %lld ms)\n",
               (long long)elapsed);
        return 1;
    }

    printf("PASS: Timer works correctly\n");

    gt_timer_destroy(timer);
    gt_shutdown();
    return 0;
}
