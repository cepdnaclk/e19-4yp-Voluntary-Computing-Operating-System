#ifndef VOLCOM_USCHEDULER_H
#define VOLCOM_USCHEDULER_H

#define _GNU_SOURCE

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_TASKS 8
#define MAX_PATH 512

// TODO: Move
// Client information structure
struct client_info_s {
    char ip_address[64];       
    int port;                  
    char client_name[256];     
};

// Task information structure
struct task_info_s {
    int task_id;              
    char task_name[256];      
    int priority;              
    unsigned long memory_usage; 
    double cpu_usage;          
    char status[32];
    struct client_info_s client;           
};

// Task buffer structure
struct task_buffer{
    struct task_info_s tasks[8]; 
    int head;                    
    int tail;                    
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;                   
};

// Function declarations
void task_buffer_init(struct task_buffer *buf);
void task_buffer_destroy(struct task_buffer *buf);
bool task_buffer_enqueue(struct task_buffer *buf, struct task_info_s *task);
bool task_buffer_dequeue(struct task_buffer *buf, struct task_info_s *task_out);
int task_buffer_size(struct task_buffer *buf);

// Function declarations for printing information
void task_buffer_print(struct task_buffer *buf);

// Utility functions
int task_buffer_size(struct task_buffer *buf);

#endif // VOLCOM_USCHEDULER_H