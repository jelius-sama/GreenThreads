# Go Green Threads FFI Library

A production-ready C library that exports Go's green threads (goroutines) and concurrency primitives via FFI using C ABI.

## Features

This library provides full access to Go's concurrency model from C:

- **Goroutines**: Spawn C functions as lightweight green threads
- **Channels**: Type-safe message passing with blocking/non-blocking operations
- **WaitGroups**: Coordinate multiple goroutines
- **Mutexes**: Protect shared state with Go's efficient mutex implementation
- **Context**: Cancellation and timeout propagation
- **Timers & Tickers**: Time-based operations
- **Select Operations**: Multiplexing on multiple channels

## Architecture

### How It Works

1. **C Function Trampolines**: Uses inline C helpers to call C function pointers from Go
2. **Handle-Based API**: All Go objects are exposed as opaque `uint64_t` handles
3. **C-Compatible Types**: All communication uses C types (`void*`, `int`, byte arrays)
4. **Zero-Copy Where Possible**: Minimizes data copying between C and Go

### Key Design Decisions

- C functions run **inside goroutines** via the trampoline pattern
- All Go concurrency features are exposed as opaque handles
- Thread-safe: Multiple C threads can safely interact with the library
- No Go pointers leak to C (satisfies cgo safety requirements)

## Building

### Prerequisites

- Go 1.20+ (with CGO enabled)
- GCC or compatible C compiler
- Make (optional, for convenience)

### Build Steps

```bash
# Build the shared library
make lib

# Build and run the example
make example run

# Or manually:
go build -buildmode=c-shared -o lib/libgtruntime.so gtruntime.go
gcc -o build/example example.c -L./lib -lgtruntime -lpthread
LD_LIBRARY_PATH=./lib ./build/example
```

### System-Wide Installation

```bash
sudo make install
```

This installs:
- `libgtruntime.so` → `/usr/local/lib/`
- `gtruntime.h` → `/usr/local/include/`

## API Reference

### Initialization

```c
void gt_init(void);        // Optional, auto-initializes on first use
void gt_shutdown(void);    // Cleanup resources
```

### Spawning Goroutines

```c
// Function pointer types
typedef void (*gt_void_func_t)(void* arg);
typedef void* (*gt_ptr_func_t)(void* arg);
typedef int (*gt_int_func_t)(void* arg);

// Spawn functions
gt_task_t gt_spawn_void(gt_void_func_t fn, void* arg);
gt_task_t gt_spawn_ptr(gt_ptr_func_t fn, void* arg);
gt_task_t gt_spawn_int(gt_int_func_t fn, void* arg);
```

**Example:**
```c
void my_task(void* arg) {
    int id = *(int*)arg;
    printf("Task %d running\n", id);
}

int main() {
    int id = 42;
    gt_task_t task = gt_spawn_void(my_task, &id);
    gt_join(task);
}
```

### Task Management

```c
int gt_join(gt_task_t task);                          // Wait for completion
int gt_join_ptr(gt_task_t task, void** result);       // Get pointer result
int gt_join_int(gt_task_t task, int* result);         // Get int result
bool gt_task_done(gt_task_t task);                    // Check if done (non-blocking)
int gt_join_all(void);                                // Wait for all tasks
```

### Channels

```c
// Create/destroy
gt_chan_t gt_chan_create(uint32_t capacity);          // Buffered channel
void gt_chan_close(gt_chan_t ch);

// Send/receive (blocking)
int gt_chan_send(gt_chan_t ch, const void* data, uint32_t len);
int gt_chan_recv(gt_chan_t ch, void* buf, uint32_t maxlen, uint32_t* out_len);

// Send/receive (non-blocking)
int gt_chan_try_send(gt_chan_t ch, const void* data, uint32_t len);
int gt_chan_try_recv(gt_chan_t ch, void* buf, uint32_t maxlen, uint32_t* out_len);

// Inspection
uint32_t gt_chan_len(gt_chan_t ch);
uint32_t gt_chan_cap(gt_chan_t ch);
```

**Example:**
```c
gt_chan_t ch = gt_chan_create(10);  // Buffer size 10

// Producer
char msg[] = "Hello";
gt_chan_send(ch, msg, strlen(msg) + 1);

// Consumer
char buf[64];
uint32_t len;
int ret = gt_chan_recv(ch, buf, sizeof(buf), &len);
if (ret == GT_OK) {
    printf("Received: %s\n", buf);
}

gt_chan_close(ch);
```

### WaitGroup

```c
gt_wg_t gt_wg_create(void);
void gt_wg_add(gt_wg_t wg, int delta);
void gt_wg_done(gt_wg_t wg);
void gt_wg_wait(gt_wg_t wg);
void gt_wg_destroy(gt_wg_t wg);
```

**Example:**
```c
gt_wg_t wg = gt_wg_create();
gt_wg_add(wg, 3);

for (int i = 0; i < 3; i++) {
    // Each task calls gt_wg_done(wg) when finished
}

gt_wg_wait(wg);  // Block until all done
gt_wg_destroy(wg);
```

### Mutex

```c
gt_mutex_t gt_mutex_create(void);
void gt_mutex_lock(gt_mutex_t mtx);
void gt_mutex_unlock(gt_mutex_t mtx);
bool gt_mutex_try_lock(gt_mutex_t mtx);
void gt_mutex_destroy(gt_mutex_t mtx);
```

**Example:**
```c
gt_mutex_t mtx = gt_mutex_create();
int shared_counter = 0;

gt_mutex_lock(mtx);
shared_counter++;
gt_mutex_unlock(mtx);

gt_mutex_destroy(mtx);
```

### Context (Cancellation)

```c
gt_ctx_t gt_ctx_background(void);
gt_ctx_t gt_ctx_with_cancel(gt_ctx_t parent);
gt_ctx_t gt_ctx_with_timeout(gt_ctx_t parent, uint64_t timeout_ms);
gt_ctx_t gt_ctx_with_deadline(gt_ctx_t parent, int64_t deadline_unix_ms);

void gt_ctx_cancel(gt_ctx_t ctx);
bool gt_ctx_is_done(gt_ctx_t ctx);
gt_chan_t gt_ctx_done_chan(gt_ctx_t ctx);
void gt_ctx_destroy(gt_ctx_t ctx);
```

**Example:**
```c
gt_ctx_t ctx = gt_ctx_with_timeout(gt_ctx_background(), 5000);

// In your task:
if (gt_ctx_is_done(ctx)) {
    printf("Context cancelled!\n");
    return;
}

gt_ctx_destroy(ctx);
```

### Timers

```c
gt_timer_t gt_timer_create(uint64_t duration_ms);
bool gt_timer_stop(gt_timer_t timer);
bool gt_timer_reset(gt_timer_t timer, uint64_t duration_ms);
gt_chan_t gt_timer_chan(gt_timer_t timer);
void gt_timer_destroy(gt_timer_t timer);
```

**Example:**
```c
gt_timer_t timer = gt_timer_create(1000);  // 1 second
gt_chan_t ch = gt_timer_chan(timer);

char buf[1];
uint32_t len;
gt_chan_recv(ch, buf, sizeof(buf), &len);  // Blocks until timer fires

gt_timer_destroy(timer);
```

### Tickers

```c
gt_ticker_t gt_ticker_create(uint64_t period_ms);
gt_chan_t gt_ticker_chan(gt_ticker_t ticker);
void gt_ticker_stop(gt_ticker_t ticker);
void gt_ticker_destroy(gt_ticker_t ticker);
```

**Example:**
```c
gt_ticker_t ticker = gt_ticker_create(500);  // Tick every 500ms
gt_chan_t ch = gt_ticker_chan(ticker);

for (int i = 0; i < 5; i++) {
    char buf[1];
    uint32_t len;
    gt_chan_recv(ch, buf, sizeof(buf), &len);
    printf("Tick!\n");
}

gt_ticker_stop(ticker);
gt_ticker_destroy(ticker);
```

### Time Operations

```c
void gt_sleep(uint64_t duration_ms);
int64_t gt_now_unix_ms(void);
```

### Runtime Utilities

```c
uint32_t gt_num_cpu(void);           // Number of CPUs
uint32_t gt_num_goroutine(void);     // Current goroutine count
void gt_gosched(void);               // Yield to scheduler
```

## Error Codes

```c
#define GT_OK 0                      // Success
#define GT_ERR_INVALID_HANDLE 1      // Invalid handle
#define GT_ERR_CLOSED 2              // Channel closed
#define GT_ERR_TIMEOUT 3             // Operation timed out
#define GT_ERR_BUFFER_TOO_SMALL 4    // Buffer too small
#define GT_ERR_WOULD_BLOCK 5         // Non-blocking op would block
```

## Common Patterns

### Producer-Consumer

```c
void producer(void* arg) {
    gt_chan_t ch = *(gt_chan_t*)arg;
    
    for (int i = 0; i < 10; i++) {
        gt_chan_send(ch, &i, sizeof(i));
    }
    gt_chan_close(ch);
}

void consumer(void* arg) {
    gt_chan_t ch = *(gt_chan_t*)arg;
    int buf;
    uint32_t len;
    
    while (gt_chan_recv(ch, &buf, sizeof(buf), &len) == GT_OK) {
        printf("Got: %d\n", buf);
    }
}
```

### Worker Pool

```c
gt_wg_t wg = gt_wg_create();
gt_chan_t jobs = gt_chan_create(100);
gt_chan_t results = gt_chan_create(100);

// Start workers
for (int i = 0; i < 5; i++) {
    gt_wg_add(wg, 1);
    // Spawn worker that reads from jobs, writes to results
}

// Send jobs
for (int i = 0; i < 100; i++) {
    gt_chan_send(jobs, &i, sizeof(i));
}
gt_chan_close(jobs);

// Wait and collect results
gt_wg_wait(wg);
gt_chan_close(results);
```

### Pipeline

```c
gt_chan_t stage1_out = gt_chan_create(10);
gt_chan_t stage2_out = gt_chan_create(10);

gt_spawn_void(stage1_func, &stage1_out);
gt_spawn_void(stage2_func, &stage2_out);
gt_spawn_void(stage3_func, &stage2_out);

gt_join_all();
```

## Performance Considerations

1. **Channel Buffer Size**: Buffered channels reduce blocking
2. **Goroutine Count**: Millions of goroutines are lightweight
3. **Data Copying**: Channel sends copy data—minimize large transfers
4. **Mutex Contention**: Keep critical sections short
5. **Memory**: Each goroutine ~2KB stack (grows as needed)

## Limitations

1. **C Code in Goroutines**: C functions run in goroutines but:
   - Cannot use Go-style preemption
   - Blocking C calls will block the goroutine
   - Not subject to Go's deadlock detection

2. **Type Safety**: Channels are untyped byte arrays—application must manage types

3. **Error Handling**: No exceptions—check return codes

4. **Pointer Lifetime**: Be careful with pointers passed to goroutines

## Thread Safety

- All API functions are thread-safe
- Multiple C threads can safely spawn goroutines
- Go's runtime handles M:N threading internally

## Debugging

```c
// Print runtime info
printf("CPUs: %u\n", gt_num_cpu());
printf("Goroutines: %u\n", gt_num_goroutine());

// Enable Go runtime debugging
export GODEBUG=gctrace=1,schedtrace=1000
```

## Extending the Library

To add custom Go tasks accessible by ID:

```go
// In gtruntime.go
func myCustomTask(arg unsafe.Pointer) {
    // Your Go code here
}

var goTaskRegistry = map[uint32]taskFunc{
    1: myCustomTask,
    2: anotherTask,
}
```

Then from C:
```c
gt_task_t task = gt_spawn_go_task(1, my_arg);
```

## License

MIT License - See LICENSE file

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Submit a pull request

## Resources

- [Go Concurrency Patterns](https://go.dev/blog/pipelines)
- [CGO Documentation](https://pkg.go.dev/cmd/cgo)
- [Go Memory Model](https://go.dev/ref/mem)

## Support

For issues or questions:
- Open an issue on GitHub
- Check the examples in `example.c`
- Read the comprehensive API documentation above
