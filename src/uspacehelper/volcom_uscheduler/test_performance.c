#include "volcom_uscheduler.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#define PERFORMANCE_ITERATIONS 10000
#define WARMUP_ITERATIONS 1000

// Timing utility functions
double get_time_diff(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
}

void create_test_task(struct task_info_s *task, int id) {
    task->task_id = id;
    snprintf(task->task_name, sizeof(task->task_name), "PerfTask_%d", id);
    task->priority = (id % 3) + 1;
    task->memory_usage = 1024 + (id % 2048);
    task->cpu_usage = 10.0 + (id % 80);
    strncpy(task->status, "pending", sizeof(task->status) - 1);
    
    snprintf(task->client.ip_address, sizeof(task->client.ip_address), 
             "192.168.1.%d", 100 + (id % 50));
    task->client.port = 8000 + (id % 100);
    snprintf(task->client.client_name, sizeof(task->client.client_name), 
             "perf_client_%d", id);
}

void benchmark_enqueue_dequeue(struct task_buffer *buffer, int iterations) {
    printf("Running enqueue/dequeue benchmark (%d iterations)...\n", iterations);
    
    struct task_info_s tasks[MAX_TASKS];
    struct task_info_s dequeued_task;
    struct timeval start, end;
    
    // Prepare tasks
    for (int i = 0; i < MAX_TASKS; i++) {
        create_test_task(&tasks[i], i);
    }
    
    // Benchmark enqueue operations
    gettimeofday(&start, NULL);
    for (int i = 0; i < iterations; i++) {
        // Fill buffer
        for (int j = 0; j < MAX_TASKS; j++) {
            task_buffer_enqueue(buffer, &tasks[j]);
        }
        
        // Empty buffer
        for (int j = 0; j < MAX_TASKS; j++) {
            task_buffer_dequeue(buffer, &dequeued_task);
        }
    }
    gettimeofday(&end, NULL);
    
    double total_time = get_time_diff(start, end);
    double ops_per_second = (iterations * MAX_TASKS * 2) / total_time; // *2 for enqueue+dequeue
    
    printf("  Total time: %.3f seconds\n", total_time);
    printf("  Operations per second: %.0f\n", ops_per_second);
    printf("  Average time per operation: %.3f microseconds\n", 
           (total_time * 1000000) / (iterations * MAX_TASKS * 2));
}

void benchmark_buffer_size(struct task_buffer *buffer, int iterations) {
    printf("Running buffer size benchmark (%d iterations)...\n", iterations);
    
    struct task_info_s task;
    create_test_task(&task, 1);
    
    // Add one task to buffer
    task_buffer_enqueue(buffer, &task);
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < iterations; i++) {
        task_buffer_size(buffer);
    }
    
    gettimeofday(&end, NULL);
    
    double total_time = get_time_diff(start, end);
    double ops_per_second = iterations / total_time;
    
    printf("  Total time: %.3f seconds\n", total_time);
    printf("  Size checks per second: %.0f\n", ops_per_second);
    printf("  Average time per size check: %.3f microseconds\n", 
           (total_time * 1000000) / iterations);
    
    // Clean up
    struct task_info_s dequeued_task;
    task_buffer_dequeue(buffer, &dequeued_task);
}

void benchmark_concurrent_access(struct task_buffer *buffer, int iterations) {
    printf("Running concurrent access simulation (%d iterations)...\n", iterations);
    
    struct task_info_s tasks[MAX_TASKS];
    struct task_info_s dequeued_task;
    struct timeval start, end;
    
    // Prepare tasks
    for (int i = 0; i < MAX_TASKS; i++) {
        create_test_task(&tasks[i], i);
    }
    
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < iterations; i++) {
        // Simulate mixed operations
        int ops_per_iteration = MAX_TASKS / 2;
        
        // Fill half the buffer
        for (int j = 0; j < ops_per_iteration; j++) {
            task_buffer_enqueue(buffer, &tasks[j]);
        }
        
        // Check size multiple times (simulate monitoring)
        for (int j = 0; j < 5; j++) {
            task_buffer_size(buffer);
        }
        
        // Remove some tasks
        for (int j = 0; j < ops_per_iteration / 2; j++) {
            task_buffer_dequeue(buffer, &dequeued_task);
        }
        
        // Add more tasks
        for (int j = ops_per_iteration; j < ops_per_iteration + 2; j++) {
            task_buffer_enqueue(buffer, &tasks[j % MAX_TASKS]);
        }
        
        // Clean up remaining tasks
        int remaining = task_buffer_size(buffer);
        for (int j = 0; j < remaining; j++) {
            task_buffer_dequeue(buffer, &dequeued_task);
        }
    }
    
    gettimeofday(&end, NULL);
    
    double total_time = get_time_diff(start, end);
    double ops_per_second = iterations / total_time;
    
    printf("  Total time: %.3f seconds\n", total_time);
    printf("  Mixed operations per second: %.0f\n", ops_per_second);
    printf("  Average time per mixed operation: %.3f microseconds\n", 
           (total_time * 1000000) / iterations);
}

void memory_usage_analysis(struct task_buffer *buffer) {
    printf("Analyzing memory usage...\n");
    
    size_t buffer_size = sizeof(struct task_buffer);
    size_t task_size = sizeof(struct task_info_s);
    size_t total_task_storage = task_size * MAX_TASKS;
    
    printf("  Buffer structure size: %zu bytes\n", buffer_size);
    printf("  Single task size: %zu bytes\n", task_size);
    printf("  Total task storage: %zu bytes\n", total_task_storage);
    printf("  Total memory usage: %zu bytes (%.2f KB)\n", 
           buffer_size, buffer_size / 1024.0);
    
    // Memory layout analysis
    printf("  Task array offset: %zu\n", offsetof(struct task_buffer, tasks));
    printf("  Head offset: %zu\n", offsetof(struct task_buffer, head));
    printf("  Tail offset: %zu\n", offsetof(struct task_buffer, tail));
    printf("  Count offset: %zu\n", offsetof(struct task_buffer, count));
    printf("  Mutex offset: %zu\n", offsetof(struct task_buffer, mutex));
}

int main() {
    printf("=== Volcom User Scheduler Performance Test ===\n\n");
    
    // Initialize buffer
    struct task_buffer buffer;
    task_buffer_init(&buffer);
    
    printf("Test Configuration:\n");
    printf("  Buffer capacity: %d tasks\n", MAX_TASKS);
    printf("  Performance iterations: %d\n", PERFORMANCE_ITERATIONS);
    printf("  Warmup iterations: %d\n", WARMUP_ITERATIONS);
    printf("\n");
    
    // Memory usage analysis
    memory_usage_analysis(&buffer);
    printf("\n");
    
    // Warmup phase
    printf("Warming up...\n");
    benchmark_enqueue_dequeue(&buffer, WARMUP_ITERATIONS);
    printf("Warmup completed.\n\n");
    
    // Performance benchmarks
    printf("=== Performance Benchmarks ===\n\n");
    
    // Test 1: Pure enqueue/dequeue performance
    printf("1. Enqueue/Dequeue Performance:\n");
    benchmark_enqueue_dequeue(&buffer, PERFORMANCE_ITERATIONS);
    printf("\n");
    
    // Test 2: Buffer size check performance
    printf("2. Buffer Size Check Performance:\n");
    benchmark_buffer_size(&buffer, PERFORMANCE_ITERATIONS * 10);
    printf("\n");
    
    // Test 3: Mixed operations performance
    printf("3. Mixed Operations Performance:\n");
    benchmark_concurrent_access(&buffer, PERFORMANCE_ITERATIONS / 10);
    printf("\n");
    
    // Test 4: Buffer state verification
    printf("4. Buffer State Verification:\n");
    assert(task_buffer_size(&buffer) == 0);
    printf("  ✓ Buffer is empty after tests\n");
    
    // Test 5: Capacity stress test
    printf("5. Capacity Stress Test:\n");
    struct task_info_s stress_task;
    create_test_task(&stress_task, 999);
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Fill to capacity
    for (int i = 0; i < MAX_TASKS; i++) {
        task_buffer_enqueue(&buffer, &stress_task);
    }
    
    gettimeofday(&end, NULL);
    double fill_time = get_time_diff(start, end);
    
    printf("  Time to fill buffer: %.3f microseconds\n", fill_time * 1000000);
    printf("  Buffer size at capacity: %d\n", task_buffer_size(&buffer));
    
    // Empty the buffer
    struct task_info_s dequeued_task;
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < MAX_TASKS; i++) {
        task_buffer_dequeue(&buffer, &dequeued_task);
    }
    
    gettimeofday(&end, NULL);
    double empty_time = get_time_diff(start, end);
    
    printf("  Time to empty buffer: %.3f microseconds\n", empty_time * 1000000);
    printf("  Buffer size after emptying: %d\n", task_buffer_size(&buffer));
    
    // Clean up
    task_buffer_destroy(&buffer);
    
    printf("\n=== Performance test completed ===\n");
    return 0;
}
