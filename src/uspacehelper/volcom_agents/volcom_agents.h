#ifndef VOLCOM_AGENTS_H
#define VOLCOM_AGENTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Agent modes
typedef enum {
    AGENT_MODE_EMPLOYER,
    AGENT_MODE_EMPLOYEE,
    AGENT_MODE_HYBRID
} agent_mode_t;

// Agent status
typedef struct {
    char agent_id[64];
    agent_mode_t mode;
    char ip_address[64];
    int port;
    time_t start_time;
    bool is_active;
    int tasks_completed;
    int tasks_failed;
} agent_status_t;

// Main agent functions
int run_employer_mode(void);
int run_employee_mode(void);
int run_hybrid_mode(void);

// Agent lifecycle
int init_agent(agent_mode_t mode);
void cleanup_agent(void);
agent_status_t get_agent_status(void);

// Employer-specific functions
typedef struct {
    char employee_id[64];
    char ip_address[64];
    int sockfd; // Socket for persistent TCP connection
    double cpu_usage;
    double memory_usage;
    int active_tasks;
    time_t last_seen;
    bool is_available;
    int reliability_score;
} employee_node_t;

// Task assignment structure for employers
typedef struct {
    char task_id[64];
    char chunk_file[256];
    char employee_id[64];
    char employee_ip[64];
    time_t assigned_time;
    time_t completed_time;
    bool is_sent;
    bool is_completed;
    int retry_count;
} task_assignment_t;

int discover_employees(void);
int get_employee_list(employee_node_t** employees, int* count);
int select_employee_for_task(const char* task_id, char* selected_employee_id);
int broadcast_task_availability(void);

// Task assignment management
int add_task_assignment(const task_assignment_t* assignment);
int send_pending_tasks(void);
int check_and_collect_results(void);
int count_completed_tasks(void);
int handle_task_timeouts(void);
int send_file_to_employee(int sockfd, const char* filepath, const char* task_id, const char* employee_ip);

// Employee-specific functions
typedef struct {
    char task_id[64];
    char chunk_filename[256];
    char sender_id[64];
    size_t data_size;
    void* data;
    time_t received_time;
    bool is_processed;
} received_task_t;

int start_resource_broadcasting(void);
int stop_resource_broadcasting(void);
int listen_for_tasks(void);
int process_received_task(const received_task_t* task);
int send_task_result(const char* task_id, const char* result_file);

// Task buffer management (for employee)
typedef struct {
    received_task_t* tasks;
    int capacity;
    int count;
    int head;
    int tail;
} task_buffer_t;

int init_task_buffer(task_buffer_t* buffer, int capacity);
void cleanup_task_buffer(task_buffer_t* buffer);
int add_task_to_buffer(task_buffer_t* buffer, const received_task_t* task);
int get_task_from_buffer(task_buffer_t* buffer, received_task_t* task);
bool is_task_buffer_empty(const task_buffer_t* buffer);
bool is_task_buffer_full(const task_buffer_t* buffer);

// Result queue for sending completed tasks back to employer
typedef struct {
    char task_id[64];
    char result_filepath[512];
    char employer_ip[64]; // To know where to send it
} result_info_t;

typedef struct {
    result_info_t* results;
    int capacity;
    int count;
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} result_queue_t;

int init_result_queue(result_queue_t* queue, int capacity);
void cleanup_result_queue(result_queue_t* queue);
int add_result_to_queue(result_queue_t* queue, const result_info_t* result);
int get_result_from_queue(result_queue_t* queue, result_info_t* result);
bool is_result_queue_empty(const result_queue_t* queue);


#endif // VOLCOM_AGENTS_H
