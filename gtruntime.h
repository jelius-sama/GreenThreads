#ifndef GTRUNTIME_H
#define GTRUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== OPAQUE HANDLES ==========
typedef uint64_t gt_task_t;   // Handle to async task/goroutine
typedef uint64_t gt_chan_t;   // Handle to channel
typedef uint64_t gt_wg_t;     // Handle to WaitGroup
typedef uint64_t gt_mutex_t;  // Handle to Mutex
typedef uint64_t gt_ctx_t;    // Handle to Context
typedef uint64_t gt_timer_t;  // Handle to Timer
typedef uint64_t gt_ticker_t; // Handle to Ticker

// ========== FUNCTION POINTER TYPES ==========
typedef void (*gt_void_func_t)(void *arg);
typedef void *(*gt_ptr_func_t)(void *arg);
typedef int (*gt_int_func_t)(void *arg);

// ========== RUNTIME INITIALIZATION ==========
void gt_init(void);
void gt_shutdown(void);

// ========== GOROUTINE SPAWNING ==========
// Spawn a C function as a goroutine (void return)
gt_task_t gt_spawn_void(gt_void_func_t fn, void *arg);

// Spawn a C function as a goroutine (pointer return)
gt_task_t gt_spawn_ptr(gt_ptr_func_t fn, void *arg);

// Spawn a C function as a goroutine (int return)
gt_task_t gt_spawn_int(gt_int_func_t fn, void *arg);

// Spawn a predefined Go task by ID
gt_task_t gt_spawn_go_task(uint32_t task_id, void *arg);

// ========== TASK MANAGEMENT ==========
// Wait for a task to complete
int gt_join(gt_task_t task);

// Wait for a task and retrieve pointer result
int gt_join_ptr(gt_task_t task, void **result);

// Wait for a task and retrieve int result
int gt_join_int(gt_task_t task, int *result);

// Check if task is done (non-blocking)
bool gt_task_done(gt_task_t task);

// Wait for all spawned tasks
int gt_join_all(void);

// ========== CHANNELS ==========
// Create buffered channel
gt_chan_t gt_chan_create(uint32_t capacity);

// Close channel
void gt_chan_close(gt_chan_t ch);

// Send data to channel (blocking)
int gt_chan_send(gt_chan_t ch, const void *data, uint32_t len);

// Send data to channel (non-blocking)
int gt_chan_try_send(gt_chan_t ch, const void *data, uint32_t len);

// Receive data from channel (blocking)
int gt_chan_recv(gt_chan_t ch, void *buf, uint32_t maxlen, uint32_t *out_len);

// Receive data from channel (non-blocking)
int gt_chan_try_recv(gt_chan_t ch, void *buf, uint32_t maxlen,
                     uint32_t *out_len);

// Get channel length
uint32_t gt_chan_len(gt_chan_t ch);

// Get channel capacity
uint32_t gt_chan_cap(gt_chan_t ch);

// ========== SELECT OPERATIONS ==========
typedef struct {
    gt_chan_t channel;
    void *buf;
    uint32_t maxlen;
    uint32_t *out_len;
    int case_id;
} gt_select_case_t;

// Perform select operation on multiple channels
// Returns the case_id of the selected case, or -1 on error
int gt_select(gt_select_case_t *cases, uint32_t num_cases);

// ========== WAITGROUP ==========
gt_wg_t gt_wg_create(void);
void gt_wg_add(gt_wg_t wg, int delta);
void gt_wg_done(gt_wg_t wg);
void gt_wg_wait(gt_wg_t wg);
void gt_wg_destroy(gt_wg_t wg);

// ========== MUTEX ==========
gt_mutex_t gt_mutex_create(void);
void gt_mutex_lock(gt_mutex_t mtx);
void gt_mutex_unlock(gt_mutex_t mtx);
bool gt_mutex_try_lock(gt_mutex_t mtx);
void gt_mutex_destroy(gt_mutex_t mtx);

// ========== CONTEXT ==========
gt_ctx_t gt_ctx_background(void);
gt_ctx_t gt_ctx_with_cancel(gt_ctx_t parent);
gt_ctx_t gt_ctx_with_timeout(gt_ctx_t parent, uint64_t timeout_ms);
gt_ctx_t gt_ctx_with_deadline(gt_ctx_t parent, int64_t deadline_unix_ms);
void gt_ctx_cancel(gt_ctx_t ctx);
bool gt_ctx_is_done(gt_ctx_t ctx);
gt_chan_t gt_ctx_done_chan(gt_ctx_t ctx);
void gt_ctx_destroy(gt_ctx_t ctx);

// ========== TIMERS ==========
gt_timer_t gt_timer_after(uint64_t duration_ms);
gt_timer_t gt_timer_create(uint64_t duration_ms);
bool gt_timer_stop(gt_timer_t timer);
bool gt_timer_reset(gt_timer_t timer, uint64_t duration_ms);
gt_chan_t gt_timer_chan(gt_timer_t timer);
void gt_timer_destroy(gt_timer_t timer);

// ========== TICKERS ==========
gt_ticker_t gt_ticker_create(uint64_t period_ms);
gt_chan_t gt_ticker_chan(gt_ticker_t ticker);
void gt_ticker_stop(gt_ticker_t ticker);
void gt_ticker_destroy(gt_ticker_t ticker);

// ========== TIME OPERATIONS ==========
void gt_sleep(uint64_t duration_ms);
int64_t gt_now_unix_ms(void);

// ========== RUNTIME UTILITIES ==========
uint32_t gt_num_cpu(void);
uint32_t gt_num_goroutine(void);
void gt_gosched(void); // Yield to scheduler

// ========== ERROR CODES ==========
#define GT_OK 0
#define GT_ERR_INVALID_HANDLE 1
#define GT_ERR_CLOSED 2
#define GT_ERR_TIMEOUT 3
#define GT_ERR_BUFFER_TOO_SMALL 4
#define GT_ERR_WOULD_BLOCK 5

#ifdef __cplusplus
}
#endif

#endif // GTRUNTIME_H
