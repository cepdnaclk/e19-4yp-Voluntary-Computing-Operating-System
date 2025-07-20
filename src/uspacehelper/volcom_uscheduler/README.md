# Volcom User Scheduler Module

A thread-safe circular buffer implementation for task scheduling in the Volcom voluntary computing system.

## Features

-   **Thread-Safe Operations**: Uses pthread mutexes and condition variables
-   **Circular Buffer**: Efficient memory usage with fixed-size buffer
-   **Blocking Operations**: Producers block when buffer is full, consumers block when empty
-   **Task Management**: Complete task information including client details and resource usage
-   **Performance Optimized**: Minimal locking overhead for high-throughput scenarios

## Files

-   `volcom_uscheduler.h` - Header file with structure definitions and function declarations
-   `volcom_uscheduler.c` - Implementation of the task buffer operations
-   `Makefile` - Build configuration for library and tests
-   `test_uscheduler.c` - Basic functionality tests
-   `test_multithread.c` - Multithreaded producer-consumer tests
-   `test_performance.c` - Performance benchmarks and stress tests
-   `run_tests.sh` - Automated test runner script

## Building

### Build Library Only

```bash
make libvolcom_uscheduler.a
```

### Build All Tests

```bash
make tests
```

### Build Everything

```bash
make all
```

## Testing

### Run Individual Tests

```bash
# Basic functionality test
./test_uscheduler

# Multithreaded test
./test_multithread

# Performance test
./test_performance
```

### Memory Leak Testing (requires valgrind)

```bash
valgrind --leak-check=full ./test_uscheduler
```

### Thread Safety Testing (requires valgrind)

```bash
valgrind --tool=helgrind ./test_multithread
```

## Usage

### Basic Usage

```c
#include "volcom_uscheduler.h"

int main() {
    // Initialize buffer
    struct task_buffer buffer;
    task_buffer_init(&buffer);

    // Create a task
    struct task_info_s task;
    task.task_id = 1;
    strcpy(task.task_name, "MyTask");
    task.priority = 1;
    task.memory_usage = 1024;
    task.cpu_usage = 50.0;
    strcpy(task.status, "pending");

    // Add task to buffer
    task_buffer_enqueue(&buffer, &task);

    // Get task from buffer
    struct task_info_s dequeued_task;
    task_buffer_dequeue(&buffer, &dequeued_task);

    // Clean up
    task_buffer_destroy(&buffer);
    return 0;
}
```

### Multithreaded Usage

```c
#include "volcom_uscheduler.h"
#include <pthread.h>

struct task_buffer shared_buffer;

void* producer_thread(void* arg) {
    struct task_info_s task;
    // Initialize task...
    task_buffer_enqueue(&shared_buffer, &task);
    return NULL;
}

void* consumer_thread(void* arg) {
    struct task_info_s task;
    task_buffer_dequeue(&shared_buffer, &task);
    // Process task...
    return NULL;
}

int main() {
    task_buffer_init(&shared_buffer);

    pthread_t producer, consumer;
    pthread_create(&producer, NULL, producer_thread, NULL);
    pthread_create(&consumer, NULL, consumer_thread, NULL);

    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    task_buffer_destroy(&shared_buffer);
    return 0;
}
```

## API Reference

### Data Structures

#### `struct task_info_s`

Contains task information including:

-   `task_id` - Unique task identifier
-   `task_name` - Human-readable task name
-   `priority` - Task priority (1-3)
-   `memory_usage` - Expected memory usage in KB
-   `cpu_usage` - Expected CPU usage percentage
-   `status` - Current task status
-   `client` - Client information structure

#### `struct task_buffer`

Thread-safe circular buffer for tasks:

-   `tasks[MAX_TASKS]` - Array of task structures
-   `head`, `tail`, `count` - Buffer state variables
-   `mutex` - Mutex for thread safety
-   `not_empty`, `not_full` - Condition variables for blocking

### Functions

#### `void task_buffer_init(struct task_buffer *buf)`

Initialize a task buffer. Must be called before using the buffer.

#### `void task_buffer_destroy(struct task_buffer *buf)`

Clean up buffer resources. Call when buffer is no longer needed.

#### `bool task_buffer_enqueue(struct task_buffer *buf, struct task_info_s *task)`

Add a task to the buffer. Blocks if buffer is full.

#### `bool task_buffer_dequeue(struct task_buffer *buf, struct task_info_s *task_out)`

Remove and return a task from the buffer. Blocks if buffer is empty.

#### `int task_buffer_size(struct task_buffer *buf)`

Get the current number of tasks in the buffer.

#### `void task_buffer_print(struct task_buffer *buf)`

Print the current state of the buffer for debugging.

## Configuration

### Buffer Size

The buffer size is defined by `MAX_TASKS` in the header file (default: 8).

### Thread Safety

All operations are thread-safe using pthread mutexes and condition variables.

### Blocking Behavior

-   `enqueue` blocks when buffer is full
-   `dequeue` blocks when buffer is empty
-   Use condition variables for efficient blocking/waking

## Performance

### Benchmarks

The performance test provides benchmarks for:

-   Pure enqueue/dequeue operations
-   Buffer size checks
-   Mixed operations simulation
-   Memory usage analysis

### Typical Performance

-   ~100,000+ operations per second on modern hardware
-   Minimal memory overhead
-   O(1) enqueue/dequeue operations
-   Thread-safe with minimal contention

## Integration

### With Volcom Main

```c
#include "volcom_uscheduler.h"
#include "volcom_sysinfo/volcom_sysinfo.h"
#include "volcom_rcsmngr/volcom_rcsmngr.h"

// Use task buffer in your main scheduler
struct task_buffer task_queue;
task_buffer_init(&task_queue);

// Producer thread adds tasks from network
// Consumer thread processes tasks with cgroups
```

### Compilation

```bash
gcc -pthread -Wall -Wextra -std=c99 your_program.c -L. -lvolcom_uscheduler -o your_program
```

## Testing Results

The test suite includes:

1. **Basic Functionality**: Buffer operations, edge cases, capacity limits
2. **Multithreaded**: Producer-consumer patterns with multiple threads
3. **Performance**: Benchmarks and stress tests
4. **Memory**: Leak detection and memory usage analysis
5. **Thread Safety**: Race condition detection with helgrind

## License

Part of the Volcom Voluntary Computing Operating System project.
