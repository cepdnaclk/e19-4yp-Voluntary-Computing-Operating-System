#include "task_buffer.h"
#include <string.h>

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
