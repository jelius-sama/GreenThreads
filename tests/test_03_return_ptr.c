#include "libgtruntime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test: Goroutine returning a pointer value

void *create_string(void *arg) {
    const char *input = (const char *)arg;
    char *result = malloc(100);
    snprintf(result, 100, "Hello, %s!", input);
    gt_sleep(50);
    return result;
}

int main(void) {
    printf("TEST: Return Pointer from Goroutine\n");
    printf("====================================\n");

    gt_init();

    const char *name = "World";
    uint64_t task = gt_spawn_ptr(create_string, (void *)name);

    void *result_ptr;
    int ret = gt_join_ptr(task, &result_ptr);

    if (ret != 0) {
        printf("FAIL: gt_join_ptr returned error code %d\n", ret);
        return 1;
    }

    char *result = (char *)result_ptr;
    printf("Got: %s\n", result);

    if (strcmp(result, "Hello, World!") != 0) {
        printf("FAIL: String mismatch\n");
        free(result);
        return 1;
    }

    printf("PASS: Pointer return value works correctly\n");

    free(result);
    gt_shutdown();
    return 0;
}
