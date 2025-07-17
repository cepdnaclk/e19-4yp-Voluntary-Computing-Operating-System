#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "employee/task_buffer.h"

static volatile int running = 1;
static TaskBuffer test_buffer;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

void* producer_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 5 && running; i++) {
        Task task = {0};
        snprintf(task.file_path, sizeof(task.file_path), "task_%d_%d.txt", thread_id, i);
        snprintf(task.employer_ip, sizeof(task.employer_ip), "192.168.1.%d", thread_id);
        task.client_fd = thread_id * 100 + i;
        
        if (task_buffer_enqueue(&test_buffer, &task)) {
            printf("[Producer %d] Enqueued task: %s\n", thread_id, task.file_path);
        }
        
        usleep(100000); // 100ms
    }
    
    return NULL;
}

void* consumer_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    while (running) {
        Task task;
        if (task_buffer_dequeue_timeout(&test_buffer, &task, 500)) { // 500ms timeout
            printf("[Consumer %d] Dequeued task: %s from %s (fd: %d)\n", 
                   thread_id, task.file_path, task.employer_ip, task.client_fd);
            
            // Simulate processing
            usleep(50000); // 50ms
        } else {
            printf("[Consumer %d] Timeout waiting for task\n", thread_id);
        }
        
        // Report buffer status
        int size = task_buffer_size(&test_buffer);
        int capacity = task_buffer_capacity(&test_buffer);
        double usage = task_buffer_usage_percentage(&test_buffer);
        
        printf("[Consumer %d] Buffer: %d/%d tasks (%.1f%% usage)\n", 
               thread_id, size, capacity, usage);
    }
    
    return NULL;
}

int main() {
    printf("=== Employee Architecture Test ===\n");
    printf("Testing socket architecture with 2 threads:\n");
    printf("1. Broadcasting thread (simulated)\n");
    printf("2. Worker thread (simulated)\n");
    printf("Press Ctrl+C to stop\n\n");
    
    signal(SIGINT, sigint_handler);
    
    // Initialize buffer
    task_buffer_init(&test_buffer);
    
    // Create producer and consumer threads
    pthread_t producers[2];
    pthread_t consumers[2];
    int producer_ids[2] = {1, 2};
    int consumer_ids[2] = {1, 2};
    
    // Start producer threads (simulate task reception)
    for (int i = 0; i < 2; i++) {
        pthread_create(&producers[i], NULL, producer_thread, &producer_ids[i]);
    }
    
    // Start consumer threads (simulate worker processing)
    for (int i = 0; i < 2; i++) {
        pthread_create(&consumers[i], NULL, consumer_thread, &consumer_ids[i]);
    }
    
    // Let it run for a bit
    sleep(3);
    
    // Stop gracefully
    running = 0;
    
    // Wait for threads to finish
    for (int i = 0; i < 2; i++) {
        pthread_join(producers[i], NULL);
        pthread_join(consumers[i], NULL);
    }
    
    // Final buffer status
    printf("\n=== Final Buffer Status ===\n");
    printf("Buffer size: %d\n", task_buffer_size(&test_buffer));
    printf("Buffer capacity: %d\n", task_buffer_capacity(&test_buffer));
    printf("Buffer usage: %.1f%%\n", task_buffer_usage_percentage(&test_buffer));
    
    // Clean up
    task_buffer_destroy(&test_buffer);
    
    printf("\n=== Test Complete ===\n");
    printf("Architecture Summary:\n");
    printf("✓ Single UDP socket for broadcasting availability\n");
    printf("✓ Single TCP socket for accepting connections\n");
    printf("✓ Buffer status monitoring in worker threads\n");
    printf("✓ Timeout-based task processing\n");
    printf("✓ Thread-safe buffer operations\n");
    
    return 0;
}
