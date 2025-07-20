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
        pthread_mutex_unlock(&buffer->mutex);
        return -1; // Buffer full
    }
    buffer->tasks[buffer->head] = *task;
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count++;
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
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->count--;
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

bool is_task_buffer_empty(const task_buffer_t* buffer) {
    pthread_mutex_lock((pthread_mutex_t*)&buffer->mutex);
    bool is_empty = (buffer->count == 0);
    pthread_mutex_unlock((pthread_mutex_t*)&buffer->mutex);
    return is_empty;
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