#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test: Basic channel send and receive

int main(void) {
    printf("TEST: Basic Channel Send/Recv\n");
    printf("==============================\n");

    gt_init();

    uint64_t ch = gt_chan_create(5);

    if (ch == 0) {
        printf("FAIL: Channel creation failed\n");
        return 1;
    }

    // Send data
    const char *msg = "test message";
    int ret = gt_chan_send(ch, msg, strlen(msg) + 1);

    if (ret != 0) {
        printf("FAIL: Channel send failed with error %d\n", ret);
        return 1;
    }

    printf("Sent: %s\n", msg);

    // Receive data
    char buf[64];
    uint32_t len;
    ret = gt_chan_recv(ch, buf, sizeof(buf), &len);

    if (ret != 0) {
        printf("FAIL: Channel recv failed with error %d\n", ret);
        return 1;
    }

    printf("Received: %s (len=%u)\n", buf, len);

    if (strcmp(buf, msg) != 0) {
        printf("FAIL: Message mismatch\n");
        return 1;
    }

    printf("PASS: Channel send/recv works correctly\n");

    gt_chan_close(ch);
    gt_shutdown();
    return 0;
}
