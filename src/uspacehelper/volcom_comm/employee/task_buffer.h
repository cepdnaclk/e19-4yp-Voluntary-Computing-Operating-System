#ifndef TASK_BUFFER_H
#define TASK_BUFFER_H
#include <pthread.h>
#include <stdbool.h>
#define MAX_TASKS 8
#define MAX_PATH 512
#define MAX_META 1024

typedef struct {
    char file_path[MAX_PATH];
    char meta[MAX_META]; // JSON metadata as string
    char employer_ip[64];
    int client_fd; // Socket to send result back
} Task;

typedef struct {
    Task tasks[MAX_TASKS];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} TaskBuffer;

void task_buffer_init(TaskBuffer *buf);
bool task_buffer_enqueue(TaskBuffer *buf, Task *task);
bool task_buffer_dequeue(TaskBuffer *buf, Task *task_out);
bool task_buffer_dequeue_timeout(TaskBuffer *buf, Task *task_out, int timeout_ms);
int task_buffer_size(TaskBuffer *buf);
int task_buffer_capacity(TaskBuffer *buf);
double task_buffer_usage_percentage(TaskBuffer *buf);
void task_buffer_destroy(TaskBuffer *buf);

#endif // TASK_BUFFER_H
