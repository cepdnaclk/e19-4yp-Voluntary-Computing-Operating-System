#include "volcom_scheduler.h"
#include "../volcom_net/volcom_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_TASKS 100
#define MAX_CHUNKS 1000

// Global state
static task_descriptor_t tasks[MAX_TASKS];
static chunk_info_t chunks[MAX_CHUNKS];
static int task_count = 0;
static int chunk_count = 0;
static scheduling_policy_t current_policy = SCHEDULE_ROUND_ROBIN;

// Scheduler initialization
int init_scheduler(void) {
    printf("Initializing task scheduler...\n");
    task_count = 0;
    chunk_count = 0;
    current_policy = SCHEDULE_ROUND_ROBIN;
    return 0;
}

void cleanup_scheduler(void) {
    printf("Cleaning up scheduler...\n");
    task_count = 0;
    chunk_count = 0;
}

// Task management
int add_task(const task_descriptor_t* task) {
    if (!task || task_count >= MAX_TASKS) {
        return -1;
    }
    
    // Check if task already exists
    for (int i = 0; i < task_count; i++) {
        if (strcmp(tasks[i].task_id, task->task_id) == 0) {
            return -1; // Task already exists
        }
    }
    
    memcpy(&tasks[task_count], task, sizeof(task_descriptor_t));
    task_count++;
    
    printf("Task %s added to scheduler\n", task->task_id);
    return 0;
}

int remove_task(const char* task_id) {
    if (!task_id) return -1;
    
    for (int i = 0; i < task_count; i++) {
        if (strcmp(tasks[i].task_id, task_id) == 0) {
            // Remove chunks associated with this task
            for (int j = 0; j < chunk_count; j++) {
                if (strcmp(chunks[j].task_id, task_id) == 0) {
                    // Shift chunks array
                    memmove(&chunks[j], &chunks[j + 1], 
                           (chunk_count - j - 1) * sizeof(chunk_info_t));
                    chunk_count--;
                    j--; // Recheck this position
                }
            }
            
            // Remove task
            memmove(&tasks[i], &tasks[i + 1], 
                   (task_count - i - 1) * sizeof(task_descriptor_t));
            task_count--;
            
            printf("Task %s removed from scheduler\n", task_id);
            return 0;
        }
    }
    
    return -1; // Task not found
}

task_descriptor_t* get_task(const char* task_id) {
    if (!task_id) return NULL;
    
    for (int i = 0; i < task_count; i++) {
        if (strcmp(tasks[i].task_id, task_id) == 0) {
            return &tasks[i];
        }
    }
    
    return NULL;
}

int get_pending_tasks(task_descriptor_t** tasks_out, int max_tasks) {
    if (!tasks_out) return -1;
    
    int pending_count = 0;
    for (int i = 0; i < task_count && pending_count < max_tasks; i++) {
        // Check if task has incomplete chunks
        bool has_incomplete = false;
        for (int j = 0; j < chunk_count; j++) {
            if (strcmp(chunks[j].task_id, tasks[i].task_id) == 0 && !chunks[j].is_completed) {
                has_incomplete = true;
                break;
            }
        }
        
        if (has_incomplete || !tasks[i].is_chunked) {
            tasks_out[pending_count] = &tasks[i];
            pending_count++;
        }
    }
    
    return pending_count;
}

// Chunk management
int create_chunks(const char* task_id, size_t chunk_size) {
    if (!task_id) return -1;
    
    task_descriptor_t* task = get_task(task_id);
    if (!task) return -1;
    
    // Check if file exists
    struct stat st;
    if (stat(task->input_file, &st) != 0) {
        printf("Input file %s not found\n", task->input_file);
        return -1;
    }
    
    size_t file_size = st.st_size;
    int total_chunks = (file_size + chunk_size - 1) / chunk_size;
    
    // Create chunk entries
    for (int i = 0; i < total_chunks && chunk_count < MAX_CHUNKS; i++) {
        chunk_info_t chunk;
        snprintf(chunk.chunk_id, sizeof(chunk.chunk_id), "%s_chunk_%d", task_id, i);
        strncpy(chunk.task_id, task_id, sizeof(chunk.task_id) - 1);
        snprintf(chunk.chunk_file, sizeof(chunk.chunk_file), 
                "/tmp/%s_chunk_%d.dat", task_id, i);
        
        chunk.chunk_size = (i == total_chunks - 1) ? 
                          (file_size - i * chunk_size) : chunk_size;
        chunk.chunk_index = i;
        chunk.total_chunks = total_chunks;
        chunk.assigned_employee[0] = '\0';
        chunk.assigned_time = 0;
        chunk.is_completed = false;
        
        chunks[chunk_count] = chunk;
        chunk_count++;
    }
    
    task->is_chunked = true;
    printf("Created %d chunks for task %s\n", total_chunks, task_id);
    return total_chunks;
}

int get_next_chunk(chunk_info_t* chunk_info) {
    if (!chunk_info) return -1;
    
    for (int i = 0; i < chunk_count; i++) {
        if (!chunks[i].is_completed && chunks[i].assigned_employee[0] == '\0') {
            memcpy(chunk_info, &chunks[i], sizeof(chunk_info_t));
            return 0;
        }
    }
    
    return -1; // No available chunks
}

int assign_chunk(const char* chunk_id, const char* employee_id) {
    if (!chunk_id || !employee_id) return -1;
    
    for (int i = 0; i < chunk_count; i++) {
        if (strcmp(chunks[i].chunk_id, chunk_id) == 0) {
            strncpy(chunks[i].assigned_employee, employee_id, 
                   sizeof(chunks[i].assigned_employee) - 1);
            chunks[i].assigned_time = time(NULL);
            printf("Chunk %s assigned to employee %s\n", chunk_id, employee_id);
            return 0;
        }
    }
    
    return -1; // Chunk not found
}

int mark_chunk_completed(const char* chunk_id) {
    if (!chunk_id) return -1;
    
    for (int i = 0; i < chunk_count; i++) {
        if (strcmp(chunks[i].chunk_id, chunk_id) == 0) {
            chunks[i].is_completed = true;
            printf("Chunk %s marked as completed\n", chunk_id);
            return 0;
        }
    }
    
    return -1; // Chunk not found
}

int get_chunk_status(const char* task_id, int* completed, int* total) {
    if (!task_id || !completed || !total) return -1;
    
    *completed = 0;
    *total = 0;
    
    for (int i = 0; i < chunk_count; i++) {
        if (strcmp(chunks[i].task_id, task_id) == 0) {
            (*total)++;
            if (chunks[i].is_completed) {
                (*completed)++;
            }
        }
    }
    
    return 0;
}

// Task distribution (simplified implementation)
int send_task_to_employee(const char* task_id, const char* employee_id) {
    if (!task_id || !employee_id) return -1;
    
    printf("Sending task %s to employee %s\n", task_id, employee_id);
    // Implementation will be completed when we integrate with volcom_net
    return 0;
}

int select_best_employee(employee_info_t* employees, int count, const task_descriptor_t* task) {
    if (!employees || count <= 0 || !task) return -1;
    
    int best_index = -1;
    double best_score = -1.0;
    
    for (int i = 0; i < count; i++) {
        if (!employees[i].is_available) continue;
        
        // Simple scoring based on current policy
        double score = 0.0;
        
        switch (current_policy) {
            case SCHEDULE_LOAD_BALANCED:
                // Prefer employees with lower CPU and memory usage
                score = (100.0 - employees[i].cpu_usage) + (100.0 - employees[i].memory_usage);
                score -= employees[i].active_tasks * 10.0;
                break;
                
            case SCHEDULE_ROUND_ROBIN:
                // Simple round-robin based on active tasks
                score = 100.0 - employees[i].active_tasks;
                break;
                
            case SCHEDULE_PRIORITY_BASED:
                // Consider task priority and employee capability
                score = (100.0 - employees[i].cpu_usage) * (task->priority / 10.0);
                break;
                
            case SCHEDULE_DEADLINE_AWARE:
                // Consider task deadline and employee load
                time_t now = time(NULL);
                double time_remaining = difftime(task->deadline, now);
                score = time_remaining / (employees[i].active_tasks + 1);
                break;
        }
        
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }
    
    return best_index;
}

// Scheduling policies
int set_scheduling_policy(scheduling_policy_t policy) {
    current_policy = policy;
    printf("Scheduling policy set to %d\n", policy);
    return 0;
}

scheduling_policy_t get_scheduling_policy(void) {
    return current_policy;
}
