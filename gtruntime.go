package main

/*
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef void (*c_void_func_t)(void*);
typedef void* (*c_ptr_func_t)(void*);
typedef int (*c_int_func_t)(void*);

typedef uint64_t gt_chan_t;   // Handle to channel
typedef struct {
    gt_chan_t channel;
    void *buf;
    uint32_t maxlen;
    uint32_t *out_len;
    int case_id;
} gt_select_case_t;

// Trampolines to invoke C function pointers from Go
static inline void invoke_void_func(c_void_func_t fn, void* arg) {
    fn(arg);
}

static inline void* invoke_ptr_func(c_ptr_func_t fn, void* arg) {
    return fn(arg);
}

static inline int invoke_int_func(c_int_func_t fn, void* arg) {
    return fn(arg);
}

typedef const void *gt_chan_arg_data;
*/
import "C"

import (
    "context"
    "runtime"
    "sync"
    "sync/atomic"
    "time"
    "unsafe"
)

// ========== HANDLE TYPES ==========
type (
    taskHandle   uint64
    chanHandle   uint64
    wgHandle     uint64
    mutexHandle  uint64
    ctxHandle    uint64
    timerHandle  uint64
    tickerHandle uint64
)

// ========== TASK STORAGE ==========
type taskState struct {
    done   chan struct{}
    result interface{}
    err    error
}

var (
    nextTaskID uint64
    tasksMu    sync.RWMutex
    tasks      = make(map[taskHandle]*taskState)

    nextChanID uint64
    chansMu    sync.RWMutex
    chans      = make(map[chanHandle]chan []byte)

    nextWgID uint64
    wgsMu    sync.RWMutex
    wgs      = make(map[wgHandle]*sync.WaitGroup)

    nextMutexID uint64
    mutexesMu   sync.RWMutex
    mutexes     = make(map[mutexHandle]*sync.Mutex)

    nextCtxID  uint64
    ctxsMu     sync.RWMutex
    ctxs       = make(map[ctxHandle]context.Context)
    ctxCancels = make(map[ctxHandle]context.CancelFunc)

    nextTimerID uint64
    timersMu    sync.RWMutex
    timers      = make(map[timerHandle]*time.Timer)

    nextTickerID uint64
    tickersMu    sync.RWMutex
    tickers      = make(map[tickerHandle]*time.Ticker)
)

// ========== GO TASK REGISTRY ==========
type taskFunc func(arg unsafe.Pointer)

var goTaskRegistry = map[uint32]taskFunc{
    // Add predefined Go tasks here
    // Example: 1: exampleGoTask,
}

// ========== RUNTIME INITIALIZATION ==========

//export gt_init
func gt_init() {
    // Go runtime auto-initializes
}

//export gt_shutdown
func gt_shutdown() {
    // Clean up resources
    tasksMu.Lock()
    tasks = make(map[taskHandle]*taskState)
    tasksMu.Unlock()

    chansMu.Lock()
    for _, ch := range chans {
        close(ch)
    }
    chans = make(map[chanHandle]chan []byte)
    chansMu.Unlock()
}

// ========== TASK HELPERS ==========

func allocateTask() (taskHandle, *taskState) {
    h := taskHandle(atomic.AddUint64(&nextTaskID, 1))
    state := &taskState{
        done: make(chan struct{}),
    }

    tasksMu.Lock()
    tasks[h] = state
    tasksMu.Unlock()

    return h, state
}

func completeTask(h taskHandle, result interface{}, err error) {
    tasksMu.Lock()
    state, ok := tasks[h]
    tasksMu.Unlock()

    if ok {
        state.result = result
        state.err = err
        close(state.done)
    }
}

func getTask(h taskHandle) (*taskState, bool) {
    tasksMu.RLock()
    defer tasksMu.RUnlock()
    state, ok := tasks[h]
    return state, ok
}

// ========== GOROUTINE SPAWNING ==========

//export gt_spawn_void
func gt_spawn_void(fn C.c_void_func_t, arg unsafe.Pointer) C.uint64_t {
    handle, _ := allocateTask()

    go func() {
        defer func() {
            if r := recover(); r != nil {
                completeTask(handle, nil, nil)
            }
        }()

        C.invoke_void_func(fn, arg)
        completeTask(handle, nil, nil)
    }()

    return C.uint64_t(handle)
}

//export gt_spawn_ptr
func gt_spawn_ptr(fn C.c_ptr_func_t, arg unsafe.Pointer) C.uint64_t {
    handle, _ := allocateTask()

    go func() {
        defer func() {
            if r := recover(); r != nil {
                completeTask(handle, nil, nil)
            }
        }()

        result := C.invoke_ptr_func(fn, arg)
        completeTask(handle, result, nil)
    }()

    return C.uint64_t(handle)
}

//export gt_spawn_int
func gt_spawn_int(fn C.c_int_func_t, arg unsafe.Pointer) C.uint64_t {
    handle, _ := allocateTask()

    go func() {
        defer func() {
            if r := recover(); r != nil {
                completeTask(handle, 0, nil)
            }
        }()

        result := C.invoke_int_func(fn, arg)
        completeTask(handle, int(result), nil)
    }()

    return C.uint64_t(handle)
}

//export gt_spawn_go_task
func gt_spawn_go_task(task_id C.uint32_t, arg unsafe.Pointer) C.uint64_t {
    id := uint32(task_id)
    entry, ok := goTaskRegistry[id]
    if !ok {
        return 0 // Invalid task ID
    }

    handle, _ := allocateTask()

    go func() {
        defer func() {
            if r := recover(); r != nil {
                completeTask(handle, nil, nil)
            }
        }()

        entry(arg)
        completeTask(handle, nil, nil)
    }()

    return C.uint64_t(handle)
}

// ========== TASK MANAGEMENT ==========

//export gt_join
func gt_join(h C.uint64_t) C.int {
    handle := taskHandle(h)
    state, ok := getTask(handle)
    if !ok {
        return C.int(1) // Invalid handle
    }

    <-state.done

    tasksMu.Lock()
    delete(tasks, handle)
    tasksMu.Unlock()

    return C.int(0)
}

//export gt_join_ptr
func gt_join_ptr(h C.uint64_t, result *unsafe.Pointer) C.int {
    handle := taskHandle(h)
    state, ok := getTask(handle)
    if !ok {
        return C.int(1)
    }

    <-state.done

    if result != nil {
        *result = state.result.(unsafe.Pointer)
    }

    tasksMu.Lock()
    delete(tasks, handle)
    tasksMu.Unlock()

    return C.int(0)
}

//export gt_join_int
func gt_join_int(h C.uint64_t, result *C.int) C.int {
    handle := taskHandle(h)
    state, ok := getTask(handle)
    if !ok {
        return C.int(1)
    }

    <-state.done

    if result != nil {
        *result = C.int(state.result.(int))
    }

    tasksMu.Lock()
    delete(tasks, handle)
    tasksMu.Unlock()

    return C.int(0)
}

//export gt_task_done
func gt_task_done(h C.uint64_t) C.bool {
    handle := taskHandle(h)
    state, ok := getTask(handle)
    if !ok {
        return C.bool(true)
    }

    select {
    case <-state.done:
        return C.bool(true)
    default:
        return C.bool(false)
    }
}

//export gt_join_all
func gt_join_all() C.int {
    for {
        tasksMu.RLock()
        if len(tasks) == 0 {
            tasksMu.RUnlock()
            return C.int(0)
        }

        doneList := make([]chan struct{}, 0, len(tasks))
        for _, state := range tasks {
            doneList = append(doneList, state.done)
        }
        tasksMu.RUnlock()

        for _, done := range doneList {
            <-done
        }
    }
}

// ========== CHANNELS ==========

//export gt_chan_create
func gt_chan_create(capacity C.uint32_t) C.uint64_t {
    h := chanHandle(atomic.AddUint64(&nextChanID, 1))
    ch := make(chan []byte, int(capacity))

    chansMu.Lock()
    chans[h] = ch
    chansMu.Unlock()

    return C.uint64_t(h)
}

//export gt_chan_close
func gt_chan_close(h C.uint64_t) {
    handle := chanHandle(h)

    chansMu.Lock()
    ch, ok := chans[handle]
    if ok {
        close(ch)
        delete(chans, handle)
    }
    chansMu.Unlock()
}

//export gt_chan_send
func gt_chan_send(h C.uint64_t, data C.gt_chan_arg_data, length C.uint32_t) C.int {
    handle := chanHandle(h)

    chansMu.RLock()
    ch, ok := chans[handle]
    chansMu.RUnlock()

    if !ok {
        return C.int(1) // Invalid handle
    }

    var buf []byte
    if length > 0 {
        buf = C.GoBytes(unsafe.Pointer(data), C.int(length))
    }

    defer func() {
        if r := recover(); r != nil {
            // Channel closed during send
        }
    }()

    ch <- buf
    return C.int(0)
}

//export gt_chan_try_send
func gt_chan_try_send(h C.uint64_t, data C.gt_chan_arg_data, length C.uint32_t) C.int {
    handle := chanHandle(h)

    chansMu.RLock()
    ch, ok := chans[handle]
    chansMu.RUnlock()

    if !ok {
        return C.int(1)
    }

    var buf []byte
    if length > 0 {
        buf = C.GoBytes(unsafe.Pointer(data), C.int(length))
    }

    defer func() {
        if r := recover(); r != nil {
            // Channel closed
        }
    }()

    select {
    case ch <- buf:
        return C.int(0)
    default:
        return C.int(5) // Would block
    }
}

//export gt_chan_recv
func gt_chan_recv(h C.uint64_t, buf unsafe.Pointer, maxlen C.uint32_t, out_len *C.uint32_t) C.int {
    handle := chanHandle(h)

    chansMu.RLock()
    ch, ok := chans[handle]
    chansMu.RUnlock()

    if !ok {
        return C.int(1)
    }

    msg, ok := <-ch
    if !ok {
        *out_len = 0
        return C.int(2) // Closed
    }

    n := len(msg)
    if n > int(maxlen) {
        n = int(maxlen)
    }

    if n > 0 && buf != nil {
        dst := unsafe.Slice((*byte)(buf), int(maxlen))
        copy(dst[:n], msg)
    }

    *out_len = C.uint32_t(n)
    return C.int(0)
}

//export gt_chan_try_recv
func gt_chan_try_recv(h C.uint64_t, buf unsafe.Pointer, maxlen C.uint32_t, out_len *C.uint32_t) C.int {
    handle := chanHandle(h)

    chansMu.RLock()
    ch, ok := chans[handle]
    chansMu.RUnlock()

    if !ok {
        return C.int(1)
    }

    select {
    case msg, ok := <-ch:
        if !ok {
            *out_len = 0
            return C.int(2)
        }

        n := len(msg)
        if n > int(maxlen) {
            n = int(maxlen)
        }

        if n > 0 && buf != nil {
            dst := unsafe.Slice((*byte)(buf), int(maxlen))
            copy(dst[:n], msg)
        }

        *out_len = C.uint32_t(n)
        return C.int(0)
    default:
        return C.int(5) // Would block
    }
}

//export gt_chan_len
func gt_chan_len(h C.uint64_t) C.uint32_t {
    handle := chanHandle(h)

    chansMu.RLock()
    ch, ok := chans[handle]
    chansMu.RUnlock()

    if !ok {
        return 0
    }

    return C.uint32_t(len(ch))
}

//export gt_chan_cap
func gt_chan_cap(h C.uint64_t) C.uint32_t {
    handle := chanHandle(h)

    chansMu.RLock()
    ch, ok := chans[handle]
    chansMu.RUnlock()

    if !ok {
        return 0
    }

    return C.uint32_t(cap(ch))
}

// ========== SELECT OPERATIONS ==========

//export gt_select
func gt_select(cases *C.gt_select_case_t, num_cases C.uint32_t) C.int {
    // This is a simplified select implementation
    // In production, you'd want a more sophisticated approach
    n := int(num_cases)
    if n == 0 {
        return C.int(-1)
    }

    // Convert C array to Go slice
    cCases := unsafe.Slice((*C.gt_select_case_t)(cases), n)

    // Build select cases
    selectCases := make([]chan []byte, n)
    for i := 0; i < n; i++ {
        handle := chanHandle(cCases[i].channel)
        chansMu.RLock()
        ch, ok := chans[handle]
        chansMu.RUnlock()

        if !ok {
            return C.int(-1)
        }
        selectCases[i] = ch
    }

    // Simple implementation: try each in order
    for i := 0; i < n; i++ {
        select {
        case msg, ok := <-selectCases[i]:
            if !ok {
                return C.int(-1)
            }

            // Copy data to output buffer
            if cCases[i].buf != nil && cCases[i].out_len != nil {
                length := len(msg)
                if length > int(cCases[i].maxlen) {
                    length = int(cCases[i].maxlen)
                }

                if length > 0 {
                    dst := unsafe.Slice((*byte)(cCases[i].buf), int(cCases[i].maxlen))
                    copy(dst[:length], msg)
                }

                *cCases[i].out_len = C.uint32_t(length)
            }

            return C.int(cCases[i].case_id)
        default:
            continue
        }
    }

    // None ready, block on first
    msg, ok := <-selectCases[0]
    if !ok {
        return C.int(-1)
    }

    if cCases[0].buf != nil && cCases[0].out_len != nil {
        length := len(msg)
        if length > int(cCases[0].maxlen) {
            length = int(cCases[0].maxlen)
        }

        if length > 0 {
            dst := unsafe.Slice((*byte)(cCases[0].buf), int(cCases[0].maxlen))
            copy(dst[:length], msg)
        }

        *cCases[0].out_len = C.uint32_t(length)
    }

    return C.int(cCases[0].case_id)
}

// ========== WAITGROUP ==========

//export gt_wg_create
func gt_wg_create() C.uint64_t {
    h := wgHandle(atomic.AddUint64(&nextWgID, 1))
    wg := &sync.WaitGroup{}

    wgsMu.Lock()
    wgs[h] = wg
    wgsMu.Unlock()

    return C.uint64_t(h)
}

//export gt_wg_add
func gt_wg_add(h C.uint64_t, delta C.int) {
    handle := wgHandle(h)

    wgsMu.RLock()
    wg, ok := wgs[handle]
    wgsMu.RUnlock()

    if ok {
        wg.Add(int(delta))
    }
}

//export gt_wg_done
func gt_wg_done(h C.uint64_t) {
    handle := wgHandle(h)

    wgsMu.RLock()
    wg, ok := wgs[handle]
    wgsMu.RUnlock()

    if ok {
        wg.Done()
    }
}

//export gt_wg_wait
func gt_wg_wait(h C.uint64_t) {
    handle := wgHandle(h)

    wgsMu.RLock()
    wg, ok := wgs[handle]
    wgsMu.RUnlock()

    if ok {
        wg.Wait()
    }
}

//export gt_wg_destroy
func gt_wg_destroy(h C.uint64_t) {
    handle := wgHandle(h)

    wgsMu.Lock()
    delete(wgs, handle)
    wgsMu.Unlock()
}

// ========== MUTEX ==========

//export gt_mutex_create
func gt_mutex_create() C.uint64_t {
    h := mutexHandle(atomic.AddUint64(&nextMutexID, 1))
    mtx := &sync.Mutex{}

    mutexesMu.Lock()
    mutexes[h] = mtx
    mutexesMu.Unlock()

    return C.uint64_t(h)
}

//export gt_mutex_lock
func gt_mutex_lock(h C.uint64_t) {
    handle := mutexHandle(h)

    mutexesMu.RLock()
    mtx, ok := mutexes[handle]
    mutexesMu.RUnlock()

    if ok {
        mtx.Lock()
    }
}

//export gt_mutex_unlock
func gt_mutex_unlock(h C.uint64_t) {
    handle := mutexHandle(h)

    mutexesMu.RLock()
    mtx, ok := mutexes[handle]
    mutexesMu.RUnlock()

    if ok {
        mtx.Unlock()
    }
}

//export gt_mutex_try_lock
func gt_mutex_try_lock(h C.uint64_t) C.bool {
    handle := mutexHandle(h)

    mutexesMu.RLock()
    mtx, ok := mutexes[handle]
    mutexesMu.RUnlock()

    if !ok {
        return C.bool(false)
    }

    return C.bool(mtx.TryLock())
}

//export gt_mutex_destroy
func gt_mutex_destroy(h C.uint64_t) {
    handle := mutexHandle(h)

    mutexesMu.Lock()
    delete(mutexes, handle)
    mutexesMu.Unlock()
}

// ========== CONTEXT ==========

//export gt_ctx_background
func gt_ctx_background() C.uint64_t {
    h := ctxHandle(atomic.AddUint64(&nextCtxID, 1))
    ctx := context.Background()

    ctxsMu.Lock()
    ctxs[h] = ctx
    ctxsMu.Unlock()

    return C.uint64_t(h)
}

//export gt_ctx_with_cancel
func gt_ctx_with_cancel(parent C.uint64_t) C.uint64_t {
    parentHandle := ctxHandle(parent)

    ctxsMu.RLock()
    parentCtx, ok := ctxs[parentHandle]
    ctxsMu.RUnlock()

    if !ok {
        return 0
    }

    h := ctxHandle(atomic.AddUint64(&nextCtxID, 1))
    ctx, cancel := context.WithCancel(parentCtx)

    ctxsMu.Lock()
    ctxs[h] = ctx
    ctxCancels[h] = cancel
    ctxsMu.Unlock()

    return C.uint64_t(h)
}

//export gt_ctx_with_timeout
func gt_ctx_with_timeout(parent C.uint64_t, timeout_ms C.uint64_t) C.uint64_t {
    parentHandle := ctxHandle(parent)

    ctxsMu.RLock()
    parentCtx, ok := ctxs[parentHandle]
    ctxsMu.RUnlock()

    if !ok {
        return 0
    }

    h := ctxHandle(atomic.AddUint64(&nextCtxID, 1))
    duration := time.Duration(timeout_ms) * time.Millisecond
    ctx, cancel := context.WithTimeout(parentCtx, duration)

    ctxsMu.Lock()
    ctxs[h] = ctx
    ctxCancels[h] = cancel
    ctxsMu.Unlock()

    return C.uint64_t(h)
}

//export gt_ctx_with_deadline
func gt_ctx_with_deadline(parent C.uint64_t, deadline_unix_ms C.int64_t) C.uint64_t {
    parentHandle := ctxHandle(parent)

    ctxsMu.RLock()
    parentCtx, ok := ctxs[parentHandle]
    ctxsMu.RUnlock()

    if !ok {
        return 0
    }

    h := ctxHandle(atomic.AddUint64(&nextCtxID, 1))
    deadline := time.Unix(0, int64(deadline_unix_ms)*int64(time.Millisecond))
    ctx, cancel := context.WithDeadline(parentCtx, deadline)

    ctxsMu.Lock()
    ctxs[h] = ctx
    ctxCancels[h] = cancel
    ctxsMu.Unlock()

    return C.uint64_t(h)
}

//export gt_ctx_cancel
func gt_ctx_cancel(h C.uint64_t) {
    handle := ctxHandle(h)

    ctxsMu.RLock()
    cancel, ok := ctxCancels[handle]
    ctxsMu.RUnlock()

    if ok {
        cancel()
    }
}

//export gt_ctx_is_done
func gt_ctx_is_done(h C.uint64_t) C.bool {
    handle := ctxHandle(h)

    ctxsMu.RLock()
    ctx, ok := ctxs[handle]
    ctxsMu.RUnlock()

    if !ok {
        return C.bool(true)
    }

    select {
    case <-ctx.Done():
        return C.bool(true)
    default:
        return C.bool(false)
    }
}

//export gt_ctx_done_chan
func gt_ctx_done_chan(h C.uint64_t) C.uint64_t {
    handle := ctxHandle(h)

    ctxsMu.RLock()
    ctx, ok := ctxs[handle]
    ctxsMu.RUnlock()

    if !ok {
        return 0
    }

    // Convert context.Done() to our channel handle
    doneChan := ctx.Done()

    // Create a bridge channel
    ch := chanHandle(atomic.AddUint64(&nextChanID, 1))
    bridgeChan := make(chan []byte, 1)

    chansMu.Lock()
    chans[ch] = bridgeChan
    chansMu.Unlock()

    // Bridge the context done to our channel
    go func() {
        <-doneChan
        close(bridgeChan)
    }()

    return C.uint64_t(ch)
}

//export gt_ctx_destroy
func gt_ctx_destroy(h C.uint64_t) {
    handle := ctxHandle(h)

    ctxsMu.Lock()
    if cancel, ok := ctxCancels[handle]; ok {
        cancel()
        delete(ctxCancels, handle)
    }
    delete(ctxs, handle)
    ctxsMu.Unlock()
}

// ========== TIMERS ==========

//export gt_timer_after
func gt_timer_after(duration_ms C.uint64_t) C.uint64_t {
    return gt_timer_create(duration_ms)
}

//export gt_timer_create
func gt_timer_create(duration_ms C.uint64_t) C.uint64_t {
    h := timerHandle(atomic.AddUint64(&nextTimerID, 1))
    duration := time.Duration(duration_ms) * time.Millisecond
    timer := time.NewTimer(duration)

    timersMu.Lock()
    timers[h] = timer
    timersMu.Unlock()

    return C.uint64_t(h)
}

//export gt_timer_stop
func gt_timer_stop(h C.uint64_t) C.bool {
    handle := timerHandle(h)

    timersMu.RLock()
    timer, ok := timers[handle]
    timersMu.RUnlock()

    if !ok {
        return C.bool(false)
    }

    return C.bool(timer.Stop())
}

//export gt_timer_reset
func gt_timer_reset(h C.uint64_t, duration_ms C.uint64_t) C.bool {
    handle := timerHandle(h)

    timersMu.RLock()
    timer, ok := timers[handle]
    timersMu.RUnlock()

    if !ok {
        return C.bool(false)
    }

    duration := time.Duration(duration_ms) * time.Millisecond
    return C.bool(timer.Reset(duration))
}

//export gt_timer_chan
func gt_timer_chan(h C.uint64_t) C.uint64_t {
    handle := timerHandle(h)

    timersMu.RLock()
    timer, ok := timers[handle]
    timersMu.RUnlock()

    if !ok {
        return 0
    }

    // Create bridge channel
    ch := chanHandle(atomic.AddUint64(&nextChanID, 1))
    bridgeChan := make(chan []byte, 1)

    chansMu.Lock()
    chans[ch] = bridgeChan
    chansMu.Unlock()

    // Bridge timer channel
    go func() {
        <-timer.C
        bridgeChan <- []byte{1}
    }()

    return C.uint64_t(ch)
}

//export gt_timer_destroy
func gt_timer_destroy(h C.uint64_t) {
    handle := timerHandle(h)

    timersMu.Lock()
    if timer, ok := timers[handle]; ok {
        timer.Stop()
        delete(timers, handle)
    }
    timersMu.Unlock()
}

// ========== TICKERS ==========

//export gt_ticker_create
func gt_ticker_create(period_ms C.uint64_t) C.uint64_t {
    h := tickerHandle(atomic.AddUint64(&nextTickerID, 1))
    period := time.Duration(period_ms) * time.Millisecond
    ticker := time.NewTicker(period)

    tickersMu.Lock()
    tickers[h] = ticker
    tickersMu.Unlock()

    return C.uint64_t(h)
}

//export gt_ticker_chan
func gt_ticker_chan(h C.uint64_t) C.uint64_t {
    handle := tickerHandle(h)

    tickersMu.RLock()
    ticker, ok := tickers[handle]
    tickersMu.RUnlock()

    if !ok {
        return 0
    }

    // Create bridge channel
    ch := chanHandle(atomic.AddUint64(&nextChanID, 1))
    bridgeChan := make(chan []byte, 10)

    chansMu.Lock()
    chans[ch] = bridgeChan
    chansMu.Unlock()

    // Bridge ticker channel
    go func() {
        for range ticker.C {
            select {
            case bridgeChan <- []byte{1}:
            default:
                // Drop if buffer full
            }
        }
    }()

    return C.uint64_t(ch)
}

//export gt_ticker_stop
func gt_ticker_stop(h C.uint64_t) {
    handle := tickerHandle(h)

    tickersMu.RLock()
    ticker, ok := tickers[handle]
    tickersMu.RUnlock()

    if ok {
        ticker.Stop()
    }
}

//export gt_ticker_destroy
func gt_ticker_destroy(h C.uint64_t) {
    handle := tickerHandle(h)

    tickersMu.Lock()
    if ticker, ok := tickers[handle]; ok {
        ticker.Stop()
        delete(tickers, handle)
    }
    tickersMu.Unlock()
}

// ========== TIME OPERATIONS ==========

//export gt_sleep
func gt_sleep(duration_ms C.uint64_t) {
    time.Sleep(time.Duration(duration_ms) * time.Millisecond)
}

//export gt_now_unix_ms
func gt_now_unix_ms() C.int64_t {
    return C.int64_t(time.Now().UnixMilli())
}

// ========== RUNTIME UTILITIES ==========

//export gt_num_cpu
func gt_num_cpu() C.uint32_t {
    return C.uint32_t(runtime.NumCPU())
}

//export gt_num_goroutine
func gt_num_goroutine() C.uint32_t {
    return C.uint32_t(runtime.NumGoroutine())
}

//export gt_gosched
func gt_gosched() {
    runtime.Gosched()
}

func main() {}
