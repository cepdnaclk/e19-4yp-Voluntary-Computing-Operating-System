#ifndef VOLCOM_SCHEDULER_H
#define VOLCOM_SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

// Task scheduling structures
typedef struct {
    char task_id[64];
    char task_name[128];
    char input_file[256];
    char output_file[256];
    size_t chunk_size;
    int priority;
    time_t created_time;
    time_t deadline;
    bool is_chunked;
} task_descriptor_t;

typedef struct {
    char chunk_id[64];
    char task_id[64];
    char chunk_file[256];
    size_t chunk_size;
    int chunk_index;
    int total_chunks;
    char assigned_employee[64];
    time_t assigned_time;
    bool is_completed;
} chunk_info_t;

// Task scheduling functions
int init_scheduler(void);
void cleanup_scheduler(void);

// Task management
int add_task(const task_descriptor_t* task);
int remove_task(const char* task_id);
task_descriptor_t* get_task(const char* task_id);
int get_pending_tasks(task_descriptor_t** tasks, int max_tasks);

// Chunk management
int create_chunks(const char* task_id, size_t chunk_size);
int get_next_chunk(chunk_info_t* chunk_info);
int assign_chunk(const char* chunk_id, const char* employee_id);
int mark_chunk_completed(const char* chunk_id);
int get_chunk_status(const char* task_id, int* completed, int* total);

// Task distribution
typedef struct {
    char employee_id[64];
    char ip_address[64];
    double cpu_usage;
    double memory_usage;
    int active_tasks;
    time_t last_seen;
    bool is_available;
} employee_info_t;

int send_task_to_employee(const char* task_id, const char* employee_id);
int select_best_employee(employee_info_t* employees, int count, const task_descriptor_t* task);

// Scheduling policies
typedef enum {
    SCHEDULE_ROUND_ROBIN,
    SCHEDULE_LOAD_BALANCED,
    SCHEDULE_PRIORITY_BASED,
    SCHEDULE_DEADLINE_AWARE
} scheduling_policy_t;

int set_scheduling_policy(scheduling_policy_t policy);
scheduling_policy_t get_scheduling_policy(void);

#endif // VOLCOM_SCHEDULER_H
