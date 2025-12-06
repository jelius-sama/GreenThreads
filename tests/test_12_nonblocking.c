#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Non-blocking channel operations

int main(void) {
    printf("TEST: Non-blocking Channel Operations\n");
    printf("======================================\n");

    gt_init();

    uint64_t ch = gt_chan_create(2); // Small buffer

    // Fill the channel
    int val1 = 100, val2 = 200;
    gt_chan_send(ch, &val1, sizeof(val1));
    gt_chan_send(ch, &val2, sizeof(val2));

    printf("Channel filled (2/2)\n");
    printf("Channel len: %u, cap: %u\n", gt_chan_len(ch), gt_chan_cap(ch));

    // Try to send when full - should fail
    int val3 = 300;
    int ret = gt_chan_try_send(ch, &val3, sizeof(val3));

    if (ret != 5) { // GT_ERR_WOULD_BLOCK
        printf("FAIL: Expected WOULD_BLOCK (5), got %d\n", ret);
        return 1;
    }

    printf("Try-send on full channel correctly returned WOULD_BLOCK\n");

    // Non-blocking receive - should succeed
    char buf[64];
    uint32_t len;
    ret = gt_chan_try_recv(ch, buf, sizeof(buf), &len);

    if (ret != 0) {
        printf("FAIL: Try-recv failed with error %d\n", ret);
        return 1;
    }

    int received = *(int *)buf;
    printf("Try-recv succeeded, got: %d\n", received);

    if (received != 100) {
        printf("FAIL: Expected 100, got %d\n", received);
        return 1;
    }

    // Now send should succeed
    ret = gt_chan_try_send(ch, &val3, sizeof(val3));

    if (ret != 0) {
        printf("FAIL: Try-send failed after space available, error %d\n", ret);
        return 1;
    }

    printf("Try-send succeeded after space was freed\n");

    printf("PASS: Non-blocking operations work correctly\n");

    gt_chan_close(ch);
    gt_shutdown();
    return 0;
}
