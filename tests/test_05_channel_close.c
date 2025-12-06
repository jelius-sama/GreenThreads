#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>

// Test: Channel close and drain buffered items

int main(void) {
    printf("TEST: Channel Close and Drain\n");
    printf("==============================\n");

    gt_init();

    uint64_t ch = gt_chan_create(10);

    // Send multiple items
    printf("Sending 5 integers...\n");
    for (int i = 1; i <= 5; i++) {
        gt_chan_send(ch, &i, sizeof(i));
    }

    // Close the channel
    printf("Closing channel...\n");
    gt_chan_close(ch);

    // Should still be able to drain all items
    printf("Draining channel...\n");
    int received = 0;
    char buf[64];
    uint32_t len;

    while (1) {
        int ret = gt_chan_recv(ch, buf, sizeof(buf), &len);

        if (ret == 2) { // GT_ERR_CLOSED
            printf("Channel drained (received %d items)\n", received);
            break;
        }

        if (ret == 0) {
            int val = *(int *)buf;
            printf("  Received: %d\n", val);
            received++;
        } else {
            printf("FAIL: Unexpected error code %d\n", ret);
            return 1;
        }
    }

    if (received != 5) {
        printf("FAIL: Expected 5 items, got %d\n", received);
        return 1;
    }

    printf("PASS: Channel close and drain works correctly\n");

    gt_shutdown();
    return 0;
}
