#include "volcom_uscheduler.h"

void task_buffer_init(struct task_buffer *buf) {

    buf->head = buf->tail = buf->count = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
    pthread_cond_init(&buf->not_full, NULL);
}

void task_buffer_destroy(struct task_buffer *buf) {

    pthread_mutex_destroy(&buf->mutex);
    pthread_cond_destroy(&buf->not_empty);
    pthread_cond_destroy(&buf->not_full);
}

bool task_buffer_enqueue(struct task_buffer *buf, struct task_info_s *task) {

    pthread_mutex_lock(&buf->mutex);

    // TODO: Signal to employee that buffer is full
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

bool task_buffer_dequeue(struct task_buffer *buf, struct task_info_s *task_out) {

    pthread_mutex_lock(&buf->mutex);

    // TODO: Signal to employer that buffer is empty
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

void task_buffer_print(struct task_buffer *buf) {

    pthread_mutex_lock(&buf->mutex);

    printf("Task Buffer State:\n");
    printf("  Head: %d, Tail: %d, Count: %d\n", buf->head, buf->tail, buf->count);

    for (int i = 0; i < buf->count; i++) {
        int index = (buf->head + i) % MAX_TASKS;
        printf("  Task %d: ID=%d, Name=%s, Priority=%d\n", i + 1, buf->tasks[index].task_id,
               buf->tasks[index].task_name, buf->tasks[index].priority);
    }

    pthread_mutex_unlock(&buf->mutex);
}

int task_buffer_size(struct task_buffer *buf) {

    pthread_mutex_lock(&buf->mutex);
    int size = buf->count;
    pthread_mutex_unlock(&buf->mutex);
    return size;
}