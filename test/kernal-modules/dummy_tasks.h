#ifndef DUMMY_TASKS_H
#define DUMMY_TASKS_H

struct client_info_s {
    char ip[32]; // IP of the client that submitted the task
};

typedef struct task_info_s {
    int task_id;
    char task_name[256];
    int priority;
    unsigned long memory_usage;      // in MB
    unsigned int cpu_usage_percent;  // as integer percentage (0-100)
    char status[32];                 // e.g., "NEW", "RUNNING"
    struct client_info_s client;
} task_info_s;

// Declare global list and count
extern task_info_s global_task_list[];
extern int global_task_count;

#endif
