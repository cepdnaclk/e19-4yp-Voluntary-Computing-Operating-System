#include "volcom_agents.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Initialize the result queue
int init_result_queue(result_queue_t* queue, int capacity) {
    if (!queue || capacity <= 0) return -1;

    queue->results = malloc(sizeof(result_info_t) * capacity);
    if (!queue->results) {
        perror("Failed to allocate result queue");
        return -1;
    }

    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    return 0;
}

// Clean up the result queue
void cleanup_result_queue(result_queue_t* queue) {
    if (!queue) return;

    free(queue->results);
    queue->results = NULL;
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
}

// Add a result to the queue (thread-safe)
int add_result_to_queue(result_queue_t* queue, const result_info_t* result) {
    if (!queue || !result) return -1;

    pthread_mutex_lock(&queue->mutex);

    while (queue->count >= queue->capacity) {
        // Wait until there is space in the queue
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    // Copy result info to the queue
    memcpy(&queue->results[queue->tail], result, sizeof(result_info_t));
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    // Signal that the queue is no longer empty
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

// Get a result from the queue (thread-safe)
int get_result_from_queue(result_queue_t* queue, result_info_t* result) {
    if (!queue || !result) return -1;

    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0) {
        // Wait until there is something in the queue
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    // Copy result info from the queue
    memcpy(result, &queue->results[queue->head], sizeof(result_info_t));
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    // Signal that the queue is no longer full
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

// Check if the result queue is empty
bool is_result_queue_empty(const result_queue_t* queue) {
    if (!queue) return true;
    
    pthread_mutex_t* mutex = (pthread_mutex_t*)&queue->mutex;
    pthread_mutex_lock(mutex);
    bool is_empty = (queue->count == 0);
    pthread_mutex_unlock(mutex);
    
    return is_empty;
}
