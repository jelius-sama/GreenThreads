#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Ticker

int main(void) {
    printf("TEST: Ticker\n");
    printf("============\n");

    gt_init();

    printf("Creating ticker with 100ms interval...\n");
    int64_t start = gt_now_unix_ms();

    uint64_t ticker = gt_ticker_create(100);
    uint64_t tick_ch = gt_ticker_chan(ticker);

    char buf[1];
    uint32_t len;
    int64_t times[5];

    printf("Receiving 5 ticks...\n");
    for (int i = 0; i < 5; i++) {
        int ret = gt_chan_recv(tick_ch, buf, sizeof(buf), &len);
        times[i] = gt_now_unix_ms();

        if (ret != 0) {
            printf("FAIL: Ticker recv failed at tick %d with error %d\n", i,
                   ret);
            return 1;
        }

        printf("  Tick %d at %lld ms\n", i, (long long)(times[i] - start));
    }

    gt_ticker_stop(ticker);

    // Check intervals
    for (int i = 1; i < 5; i++) {
        int64_t interval = times[i] - times[i - 1];
        printf("  Interval %d: %lld ms\n", i, (long long)interval);

        if (interval < 80 || interval > 120) {
            printf("FAIL: Interval %d is out of range (expected ~100ms, got "
                   "%lld ms)\n",
                   i, (long long)interval);
            return 1;
        }
    }

    printf("PASS: Ticker works correctly\n");

    gt_ticker_destroy(ticker);
    gt_shutdown();
    return 0;
}
