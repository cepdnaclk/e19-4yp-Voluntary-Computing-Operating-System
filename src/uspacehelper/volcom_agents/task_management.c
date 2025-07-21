#include "volcom_agents.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Task Buffer Implementation
int init_task_buffer(task_buffer_t* buffer, int capacity) {
    if (!buffer) return -1;
    buffer->tasks = malloc(sizeof(received_task_t) * capacity);
    if (!buffer->tasks) return -1;
    buffer->capacity = capacity;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    return 0;
}

void cleanup_task_buffer(task_buffer_t* buffer) {
    if (buffer && buffer->tasks) {
        free(buffer->tasks);
        pthread_mutex_destroy(&buffer->mutex);
    }
}

int add_task_to_buffer(task_buffer_t* buffer, const received_task_t* task) {
    pthread_mutex_lock(&buffer->mutex);
    if (buffer->count >= buffer->capacity) {
        printf("[Task Buffer] Buffer full (%d/%d), dropping oldest task to make room\n", 
               buffer->count, buffer->capacity);
        // Drop the oldest task to make room (circular buffer behavior)
        if (buffer->tasks[buffer->tail].data) {
            free(buffer->tasks[buffer->tail].data);
        }
        buffer->tail = (buffer->tail + 1) % buffer->capacity;
        buffer->count--;
    }
    
    // Deep copy the task data
    buffer->tasks[buffer->head] = *task;
    if (task->data && task->data_size > 0) {
        buffer->tasks[buffer->head].data = malloc(task->data_size);
        if (buffer->tasks[buffer->head].data) {
            memcpy(buffer->tasks[buffer->head].data, task->data, task->data_size);
        } else {
            printf("[Task Buffer] Failed to allocate memory for task data\n");
            pthread_mutex_unlock(&buffer->mutex);
            return -1;
        }
    }
    
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count++;
    printf("[Task Buffer] Added task %s, buffer now %d/%d full\n", 
           task->task_id, buffer->count, buffer->capacity);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

int get_task_from_buffer(task_buffer_t* buffer, received_task_t* task) {
    pthread_mutex_lock(&buffer->mutex);
    if (buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return -1; // Buffer empty
    }
    *task = buffer->tasks[buffer->tail];
    // Don't free the data here - it will be freed by the worker thread
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->count--;
    printf("[Task Buffer] Retrieved task %s, buffer now %d/%d full\n", 
           task->task_id, buffer->count, buffer->capacity);
    
    // Add validation logging
    if (!task->data) {
        printf("[Task Buffer] WARNING: Task %s has NULL data pointer!\n", task->task_id);
    } else if (task->data_size == 0) {
        printf("[Task Buffer] WARNING: Task %s has zero data size!\n", task->task_id);
    } else {
        printf("[Task Buffer] Task %s data validated: %zu bytes\n", task->task_id, task->data_size);
    }
    
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

bool is_task_buffer_empty(const task_buffer_t* buffer) {
    pthread_mutex_lock((pthread_mutex_t*)&buffer->mutex);
    bool is_empty = (buffer->count == 0);
    pthread_mutex_unlock((pthread_mutex_t*)&buffer->mutex);
    return is_empty;
}

// Additional debugging function
void print_task_buffer_status(const task_buffer_t* buffer, const char* context) {
    pthread_mutex_lock((pthread_mutex_t*)&buffer->mutex);
    printf("[Task Buffer Status] %s: %d/%d tasks (head=%d, tail=%d)\n", 
           context, buffer->count, buffer->capacity, buffer->head, buffer->tail);
    
    // Print task IDs if buffer is not empty
    if (buffer->count > 0) {
        printf("[Task Buffer Contents] ");
        int current = buffer->tail;
        for (int i = 0; i < buffer->count; i++) {
            printf("%s ", buffer->tasks[current].task_id);
            current = (current + 1) % buffer->capacity;
        }
        printf("\n");
    }
    pthread_mutex_unlock((pthread_mutex_t*)&buffer->mutex);
}

// Result Queue Implementation
int init_result_queue(result_queue_t* queue, int capacity) {
    if (!queue) return -1;
    queue->results = malloc(sizeof(result_info_t) * capacity);
    if (!queue->results) return -1;
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    return 0;
}

void cleanup_result_queue(result_queue_t* queue) {
    if (queue && queue->results) {
        free(queue->results);
        pthread_mutex_destroy(&queue->mutex);
    }
}

int add_result_to_queue(result_queue_t* queue, const result_info_t* result) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->count >= queue->capacity) {
        pthread_mutex_unlock(&queue->mutex);
        return -1; // Queue full
    }
    queue->results[queue->head] = *result;
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count++;
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int get_result_from_queue(result_queue_t* queue, result_info_t* result) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return -1; // Queue empty
    }
    *result = queue->results[queue->tail];
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count--;
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

bool is_result_queue_empty(const result_queue_t* queue) {
    pthread_mutex_lock((pthread_mutex_t*)&queue->mutex);
    bool is_empty = (queue->count == 0);
    pthread_mutex_unlock((pthread_mutex_t*)&queue->mutex);
    return is_empty;
}