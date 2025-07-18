#include "task_buffer.h"
#include <string.h>
#include <sys/time.h>
#include <errno.h>

void task_buffer_init(TaskBuffer *buf) {
    buf->head = buf->tail = buf->count = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
    pthread_cond_init(&buf->not_full, NULL);
}

bool task_buffer_enqueue(TaskBuffer *buf, Task *task) {
    pthread_mutex_lock(&buf->mutex);
    while (buf->count == MAX_TASKS) {
        pthread_cond_wait(&buf->not_full, &buf->mutex);
    }
    buf->tasks[buf->tail] = *task;
    buf->tail = (buf->tail + 1) % MAX_TASKS;
    buf->count++;
    pthread_cond_signal(&buf->not_empty);
    pthread_mutex_unlock(&buf->mutex);
    return true;
}

bool task_buffer_dequeue(TaskBuffer *buf, Task *task_out) {
    pthread_mutex_lock(&buf->mutex);
    while (buf->count == 0) {
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }
    *task_out = buf->tasks[buf->head];
    buf->head = (buf->head + 1) % MAX_TASKS;
    buf->count--;
    pthread_cond_signal(&buf->not_full);
    pthread_mutex_unlock(&buf->mutex);
    return true;
}

int task_buffer_size(TaskBuffer *buf) {
    pthread_mutex_lock(&buf->mutex);
    int size = buf->count;
    pthread_mutex_unlock(&buf->mutex);
    return size;
}

bool task_buffer_dequeue_timeout(TaskBuffer *buf, Task *task_out, int timeout_ms) {
    struct timespec timeout;
    struct timeval now;
    
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + timeout_ms / 1000;
    timeout.tv_nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
    
    // Handle nanosecond overflow
    if (timeout.tv_nsec >= 1000000000) {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000;
    }
    
    pthread_mutex_lock(&buf->mutex);
    
    while (buf->count == 0) {
        int ret = pthread_cond_timedwait(&buf->not_empty, &buf->mutex, &timeout);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&buf->mutex);
            return false;
        }
    }
    
    *task_out = buf->tasks[buf->head];
    buf->head = (buf->head + 1) % MAX_TASKS;
    buf->count--;
    
    pthread_cond_signal(&buf->not_full);
    pthread_mutex_unlock(&buf->mutex);
    
    return true;
}

int task_buffer_capacity(TaskBuffer *buf) {
    (void)buf; // Suppress unused parameter warning
    return MAX_TASKS;
}

double task_buffer_usage_percentage(TaskBuffer *buf) {
    pthread_mutex_lock(&buf->mutex);
    double usage = (double)buf->count / MAX_TASKS * 100.0;
    pthread_mutex_unlock(&buf->mutex);
    return usage;
}

void task_buffer_destroy(TaskBuffer *buf) {
    pthread_mutex_lock(&buf->mutex);
    
    // Clear the buffer
    buf->head = buf->tail = buf->count = 0;
    
    pthread_mutex_unlock(&buf->mutex);
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&buf->not_empty);
    pthread_cond_destroy(&buf->not_full);
    pthread_mutex_destroy(&buf->mutex);
}
