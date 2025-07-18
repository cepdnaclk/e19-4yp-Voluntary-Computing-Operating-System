#include "volcom_uscheduler.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define TASKS_PER_PRODUCER 10
#define TOTAL_TASKS (NUM_PRODUCERS * TASKS_PER_PRODUCER)

struct thread_data {
    struct task_buffer *buffer;
    int thread_id;
    int tasks_produced;
    int tasks_consumed;
};

// Global statistics
int total_produced = 0;
int total_consumed = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void create_test_task(struct task_info_s *task, int id, int producer_id) {
    task->task_id = id;
    snprintf(task->task_name, sizeof(task->task_name), "Task_P%d_%d", producer_id, id);
    task->priority = (rand() % 3) + 1; // Random priority 1-3
    task->memory_usage = 1024 + (rand() % 2048); // Random memory usage
    task->cpu_usage = 10.0 + (rand() % 80); // Random CPU usage
    strncpy(task->status, "pending", sizeof(task->status) - 1);
    
    // Initialize client info
    snprintf(task->client.ip_address, sizeof(task->client.ip_address), 
             "192.168.1.%d", 100 + (id % 50));
    task->client.port = 8000 + (id % 100);
    snprintf(task->client.client_name, sizeof(task->client.client_name), 
             "client_p%d_%d", producer_id, id);
}

void *producer_thread(void *arg) {
    struct thread_data *data = (struct thread_data *)arg;
    struct task_buffer *buffer = data->buffer;
    int producer_id = data->thread_id;
    
    printf("[Producer %d] Started\n", producer_id);
    
    for (int i = 0; i < TASKS_PER_PRODUCER; i++) {
        struct task_info_s task;
        int task_id = producer_id * TASKS_PER_PRODUCER + i;
        
        create_test_task(&task, task_id, producer_id);
        
        printf("[Producer %d] Creating task %d: %s\n", producer_id, task_id, task.task_name);
        
        // Simulate some work before enqueueing
        usleep(rand() % 100000); // Random delay up to 100ms
        
        bool success = task_buffer_enqueue(buffer, &task);
        if (success) {
            pthread_mutex_lock(&stats_mutex);
            total_produced++;
            data->tasks_produced++;
            pthread_mutex_unlock(&stats_mutex);
            
            printf("[Producer %d] ✓ Enqueued task %d (buffer size: %d)\n", 
                   producer_id, task_id, task_buffer_size(buffer));
        } else {
            printf("[Producer %d] ✗ Failed to enqueue task %d\n", producer_id, task_id);
        }
    }
    
    printf("[Producer %d] Finished - produced %d tasks\n", producer_id, data->tasks_produced);
    return NULL;
}

void *consumer_thread(void *arg) {
    struct thread_data *data = (struct thread_data *)arg;
    struct task_buffer *buffer = data->buffer;
    int consumer_id = data->thread_id;
    
    printf("[Consumer %d] Started\n", consumer_id);
    
    while (1) {
        struct task_info_s task;
        bool success = task_buffer_dequeue(buffer, &task);
        
        if (success) {
            pthread_mutex_lock(&stats_mutex);
            total_consumed++;
            data->tasks_consumed++;
            pthread_mutex_unlock(&stats_mutex);
            
            printf("[Consumer %d] ✓ Dequeued task %d: %s (buffer size: %d)\n", 
                   consumer_id, task.task_id, task.task_name, task_buffer_size(buffer));
            
            // Simulate task processing
            usleep(rand() % 50000); // Random processing time up to 50ms
            
            // Check if we've consumed all tasks
            pthread_mutex_lock(&stats_mutex);
            if (total_consumed >= TOTAL_TASKS) {
                pthread_mutex_unlock(&stats_mutex);
                break;
            }
            pthread_mutex_unlock(&stats_mutex);
        }
    }
    
    printf("[Consumer %d] Finished - consumed %d tasks\n", consumer_id, data->tasks_consumed);
    return NULL;
}

int main() {
    printf("=== Volcom User Scheduler Multithreaded Test ===\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Initialize buffer
    struct task_buffer buffer;
    task_buffer_init(&buffer);
    
    printf("Configuration:\n");
    printf("  Producers: %d\n", NUM_PRODUCERS);
    printf("  Consumers: %d\n", NUM_CONSUMERS);
    printf("  Tasks per producer: %d\n", TASKS_PER_PRODUCER);
    printf("  Total tasks: %d\n", TOTAL_TASKS);
    printf("  Buffer capacity: %d\n", MAX_TASKS);
    printf("\n");
    
    // Create thread data structures
    struct thread_data producer_data[NUM_PRODUCERS];
    struct thread_data consumer_data[NUM_CONSUMERS];
    
    pthread_t producer_threads[NUM_PRODUCERS];
    pthread_t consumer_threads[NUM_CONSUMERS];
    
    // Initialize thread data
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_data[i].buffer = &buffer;
        producer_data[i].thread_id = i;
        producer_data[i].tasks_produced = 0;
        producer_data[i].tasks_consumed = 0;
    }
    
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_data[i].buffer = &buffer;
        consumer_data[i].thread_id = i;
        consumer_data[i].tasks_produced = 0;
        consumer_data[i].tasks_consumed = 0;
    }
    
    printf("Starting threads...\n");
    
    // Start consumer threads first
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        int result = pthread_create(&consumer_threads[i], NULL, consumer_thread, &consumer_data[i]);
        assert(result == 0);
    }
    
    // Start producer threads
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        int result = pthread_create(&producer_threads[i], NULL, producer_thread, &producer_data[i]);
        assert(result == 0);
    }
    
    // Wait for all producer threads to complete
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    
    printf("\nAll producers finished. Waiting for consumers to finish...\n");
    
    // Wait for all consumer threads to complete
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    printf("\n=== Test Results ===\n");
    printf("Total tasks produced: %d\n", total_produced);
    printf("Total tasks consumed: %d\n", total_consumed);
    printf("Final buffer size: %d\n", task_buffer_size(&buffer));
    
    // Verify results
    if (total_produced == TOTAL_TASKS && total_consumed == TOTAL_TASKS) {
        printf("✓ SUCCESS: All tasks produced and consumed correctly!\n");
    } else {
        printf("✗ FAILURE: Mismatch in task counts!\n");
    }
    
    // Show individual thread statistics
    printf("\nProducer Statistics:\n");
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        printf("  Producer %d: %d tasks produced\n", i, producer_data[i].tasks_produced);
    }
    
    printf("\nConsumer Statistics:\n");
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        printf("  Consumer %d: %d tasks consumed\n", i, consumer_data[i].tasks_consumed);
    }
    
    // Clean up
    task_buffer_destroy(&buffer);
    pthread_mutex_destroy(&stats_mutex);
    
    printf("\n=== Multithreaded test completed ===\n");
    return 0;
}
