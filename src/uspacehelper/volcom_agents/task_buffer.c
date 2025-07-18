#include "volcom_agents.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Task buffer implementation
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t buffer_cond = PTHREAD_COND_INITIALIZER;

int init_task_buffer(task_buffer_t* buffer, int capacity) {
    if (!buffer || capacity <= 0) {
        return -1;
    }
    
    buffer->tasks = malloc(capacity * sizeof(received_task_t));
    if (!buffer->tasks) {
        return -1;
    }
    
    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    
    printf("Task buffer initialized with capacity %d\n", capacity);
    return 0;
}

void cleanup_task_buffer(task_buffer_t* buffer) {
    if (!buffer) return;
    
    pthread_mutex_lock(&buffer_mutex);
    
    // Clean up any remaining tasks
    for (int i = 0; i < buffer->count; i++) {
        int index = (buffer->head + i) % buffer->capacity;
        if (buffer->tasks[index].data) {
            free(buffer->tasks[index].data);
        }
    }
    
    if (buffer->tasks) {
        free(buffer->tasks);
        buffer->tasks = NULL;
    }
    
    buffer->capacity = 0;
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    
    pthread_mutex_unlock(&buffer_mutex);
    printf("Task buffer cleaned up\n");
}

int add_task_to_buffer(task_buffer_t* buffer, const received_task_t* task) {
    if (!buffer || !task) {
        return -1;
    }
    
    pthread_mutex_lock(&buffer_mutex);
    
    if (buffer->count >= buffer->capacity) {
        pthread_mutex_unlock(&buffer_mutex);
        return -1; // Buffer full
    }
    
    // Add task to tail
    memcpy(&buffer->tasks[buffer->tail], task, sizeof(received_task_t));
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->count++;
    
    // Signal waiting threads
    pthread_cond_signal(&buffer_cond);
    
    pthread_mutex_unlock(&buffer_mutex);
    return 0;
}

int get_task_from_buffer(task_buffer_t* buffer, received_task_t* task) {
    if (!buffer || !task) {
        return -1;
    }
    
    pthread_mutex_lock(&buffer_mutex);
    
    if (buffer->count == 0) {
        pthread_mutex_unlock(&buffer_mutex);
        return -1; // Buffer empty
    }
    
    // Get task from head
    memcpy(task, &buffer->tasks[buffer->head], sizeof(received_task_t));
    
    // Clear the slot
    memset(&buffer->tasks[buffer->head], 0, sizeof(received_task_t));
    
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count--;
    
    pthread_mutex_unlock(&buffer_mutex);
    return 0;
}

bool is_task_buffer_empty(const task_buffer_t* buffer) {
    if (!buffer) return true;
    
    pthread_mutex_lock(&buffer_mutex);
    bool empty = (buffer->count == 0);
    pthread_mutex_unlock(&buffer_mutex);
    
    return empty;
}

bool is_task_buffer_full(const task_buffer_t* buffer) {
    if (!buffer) return true;
    
    pthread_mutex_lock(&buffer_mutex);
    bool full = (buffer->count >= buffer->capacity);
    pthread_mutex_unlock(&buffer_mutex);
    
    return full;
}
