#ifndef VOLCOM_AGENTS_H
#define VOLCOM_AGENTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h> // For INET_ADDRSTRLEN

#define MAX_FILENAME_LEN 256
#define MAX_EMPLOYEES 100
#define MAX_TASK_ASSIGNMENTS 1000
#define TASK_TIMEOUT_SECONDS 300 // 5 minutes

// Structure to hold information about a received task
typedef struct received_task_s {
    char task_id[MAX_FILENAME_LEN];
    char chunk_filename[MAX_FILENAME_LEN];
    char sender_id[64];
    void* data;
    size_t data_size;
    time_t received_time;
    bool is_processed;
    int frame_no; // Frame number for image/video tasks, -1 if not provided
} received_task_t;

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
int run_hybrid_mode(void);

// Agent lifecycle
int init_agent(agent_mode_t mode);
void cleanup_agent(void);
agent_status_t get_agent_status(void);
// int run_employer_mode(char* task_files[]);
// int run_employee_mode(void);


// Employee-specific functions
int start_resource_broadcasting(void);
int stop_resource_broadcasting(void);
int listen_for_tasks(void);
int process_received_task(const received_task_t* task);
int send_task_result(const char* task_id, const char* result_file);
int send_file_to_employee(int sockfd, const char* filepath, const char* task_id, const char* employee_ip);

// Employer-specific functions
// Forward-declare structs that depend on each other
struct received_task_s;
struct task_buffer_s;

// Represents the state of an employee from the employer's perspective
typedef enum {
    EMPLOYEE_STATE_NEW,
    EMPLOYEE_STATE_CONFIGURED
} employee_state_t;

// Structure to hold information about a discovered employee
typedef struct employee_node_s {
    char employee_id[64];
    char ip_address[INET_ADDRSTRLEN];
    time_t last_seen;
    int active_tasks;
    int reliability_score;
    int tasks_completed;
    int tasks_failed;
    bool is_available;
    int sockfd; // Persistent socket connection
    employee_state_t state; // Current state of the employee
} employee_node_t;

// Structure to hold information about a task result to be sent
typedef struct {
    char task_id[MAX_FILENAME_LEN];
    char result_filepath[MAX_FILENAME_LEN];
    char employer_ip[INET_ADDRSTRLEN];
} result_info_t;

// A simple circular buffer for received tasks
typedef struct task_buffer_s {
    received_task_t* tasks;
    int capacity;
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
} task_buffer_t;

// A simple circular queue for results waiting to be sent
typedef struct {
    result_info_t* results;
    int capacity;
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} result_queue_t;


// Structure to hold information about a task assignment
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

// Task Buffer and Result Queue functions
int init_task_buffer(struct task_buffer_s* buffer, int capacity);
void cleanup_task_buffer(struct task_buffer_s* buffer);
int add_task_to_buffer(struct task_buffer_s* buffer, const received_task_t* task);
int get_task_from_buffer(struct task_buffer_s* buffer, received_task_t* task);
bool is_task_buffer_empty(const struct task_buffer_s* buffer);
void print_task_buffer_status(const struct task_buffer_s* buffer, const char* context);

int init_result_queue(result_queue_t* queue, int capacity);
void cleanup_result_queue(result_queue_t* queue);
int add_result_to_queue(result_queue_t* queue, const result_info_t* result);
int get_result_from_queue(result_queue_t* queue, result_info_t* result);
bool is_result_queue_empty(const result_queue_t* queue);

// Hybrid mode placeholder
int run_hybrid_mode(void);
int start_agent(char* task_files[]);

#endif // VOLCOM_AGENTS_H
