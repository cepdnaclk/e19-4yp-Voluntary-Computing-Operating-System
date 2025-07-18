#define _GNU_SOURCE
#include "volcom_agents.h"
#include "../volcom_utils/volcom_utils.h"
#include "../volcom_net/volcom_net.h"
#include "../volcom_scheduler/volcom_scheduler.h"
#include "../volcom_sysinfo/volcom_sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>

#define PORT 9876
#define BUFFER_SIZE 2048
#define COLLECTION_TIMEOUT 5
#define STALE_THRESHOLD 15
#define MAX_EMPLOYEES 100
#define CHUNKED_SET_PATH "/home/geeth99/Desktop/chuncked_set"
#define MAX_CHUNKS 1024
#define MAX_FILENAME_LEN 256
#define EMPLOYEE_PORT 12345

// Task assignment management
#define MAX_TASK_ASSIGNMENTS 100
#define TASK_TIMEOUT_SECONDS 60

static task_assignment_t task_assignments[MAX_TASK_ASSIGNMENTS];
static int assignment_count = 0;
static pthread_mutex_t assignment_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global state
static agent_status_t agent_status;
static employee_node_t employees[MAX_EMPLOYEES];
static int employee_count = 0;
static bool agent_running = false;
static pthread_t employer_thread;
static char chunk_files[MAX_CHUNKS][MAX_FILENAME_LEN];
static int num_chunks = 0;

// Function forward declarations
static void signal_handler(int sig);
static void scan_chunked_set(void);
static void remove_stale_employees(void);
static int add_or_update_employee(const char* ip, const cJSON* broadcast_data);
static void* employer_main_loop(void* arg);
static void distribute_tasks_round_robin(void);
static void manage_task_execution(void);

// Signal handler
static void signal_handler(int sig) {
    (void)sig;
    agent_running = false;
    printf("\nShutting down agent...\n");
}

// Scan chunked set directory
static void scan_chunked_set(void) {
    DIR *dir = opendir(CHUNKED_SET_PATH);
    if (!dir) {
        printf("[Employer] Warning: Cannot open chunked set directory %s\n", CHUNKED_SET_PATH);
        return;
    }
    
    num_chunks = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && num_chunks < MAX_CHUNKS) {
        if (entry->d_type == DT_REG) { // Regular file
            strncpy(chunk_files[num_chunks], entry->d_name, MAX_FILENAME_LEN - 1);
            chunk_files[num_chunks][MAX_FILENAME_LEN - 1] = '\0';
            num_chunks++;
        }
    }
    
    closedir(dir);
    printf("[Employer] Found %d chunk files in %s\n", num_chunks, CHUNKED_SET_PATH);
}

// Remove stale employees
static void remove_stale_employees(void) {
    time_t current_time = time(NULL);
    int i = 0;
    
    while (i < employee_count) {
        if (current_time - employees[i].last_seen > STALE_THRESHOLD) {
            printf("[Employer] Removing stale employee %s (%s)\n", 
                   employees[i].employee_id, employees[i].ip_address);
            
            // Move last employee to this position
            if (i < employee_count - 1) {
                memcpy(&employees[i], &employees[employee_count - 1], sizeof(employee_node_t));
            }
            employee_count--;
        } else {
            i++;
        }
    }
}

// Add or update employee
static int add_or_update_employee(const char* ip, const cJSON* broadcast_data) {
    time_t current_time = time(NULL);
    
    // Check if employee already exists
    for (int i = 0; i < employee_count; i++) {
        if (strcmp(employees[i].ip_address, ip) == 0) {
            employees[i].last_seen = current_time;
            employees[i].is_available = true;
            return i;
        }
    }
    
    // Add new employee if space available
    if (employee_count < MAX_EMPLOYEES) {
        employee_node_t *emp = &employees[employee_count];
        
        strncpy(emp->ip_address, ip, sizeof(emp->ip_address) - 1);
        emp->ip_address[sizeof(emp->ip_address) - 1] = '\0';
        
        // Extract employee info from broadcast data
        const cJSON *id = cJSON_GetObjectItem(broadcast_data, "employee_id");
        if (id && cJSON_IsString(id)) {
            strncpy(emp->employee_id, id->valuestring, sizeof(emp->employee_id) - 1);
            emp->employee_id[sizeof(emp->employee_id) - 1] = '\0';
        } else {
            snprintf(emp->employee_id, sizeof(emp->employee_id), "emp_%d", employee_count);
        }
        
        emp->last_seen = current_time;
        emp->is_available = true;
        emp->active_tasks = 0;
        emp->reliability_score = 100;
        
        employee_count++;
        printf("[Employer] Added new employee %s at %s\n", emp->employee_id, ip);
        return employee_count - 1;
    }
    
    return -1; // No space for new employee
}

// Task assignment functions
int add_task_assignment(const task_assignment_t* assignment) {
    if (!assignment) return -1;
    
    pthread_mutex_lock(&assignment_mutex);
    
    if (assignment_count >= MAX_TASK_ASSIGNMENTS) {
        pthread_mutex_unlock(&assignment_mutex);
        return -1;
    }
    
    memcpy(&task_assignments[assignment_count], assignment, sizeof(task_assignment_t));
    assignment_count++;
    
    pthread_mutex_unlock(&assignment_mutex);
    return 0;
}

int send_pending_tasks(void) {
    pthread_mutex_lock(&assignment_mutex);
    
    int sent_count = 0;
    for (int i = 0; i < assignment_count; i++) {
        if (!task_assignments[i].is_sent && !task_assignments[i].is_completed) {
            // Send task to employee
            int result = send_file_to_employee(task_assignments[i].employee_ip, 
                                             task_assignments[i].chunk_file,
                                             task_assignments[i].task_id);
            
            if (result == 0) {
                task_assignments[i].is_sent = true;
                printf("[Employer] Successfully sent task %s to %s\n", 
                       task_assignments[i].task_id, task_assignments[i].employee_ip);
                sent_count++;
            } else {
                task_assignments[i].retry_count++;
                printf("[Employer] Failed to send task %s to %s (retry %d)\n", 
                       task_assignments[i].task_id, task_assignments[i].employee_ip,
                       task_assignments[i].retry_count);
            }
        }
    }
    
    pthread_mutex_unlock(&assignment_mutex);
    return sent_count;
}

int check_and_collect_results(void) {
    pthread_mutex_lock(&assignment_mutex);
    
    int collected_count = 0;
    for (int i = 0; i < assignment_count; i++) {
        if (task_assignments[i].is_sent && !task_assignments[i].is_completed) {
            // Check if result is available (placeholder implementation)
            // In a full implementation, this would check for result files or network responses
            
            // For now, simulate result collection after some time
            time_t current_time = time(NULL);
            if (current_time - task_assignments[i].assigned_time > 10) { // 10 seconds for demo
                task_assignments[i].is_completed = true;
                task_assignments[i].completed_time = current_time;
                
                printf("[Employer] Collected result for task %s from %s\n", 
                       task_assignments[i].task_id, task_assignments[i].employee_ip);
                
                // Update employee status
                for (int j = 0; j < employee_count; j++) {
                    if (strcmp(employees[j].ip_address, task_assignments[i].employee_ip) == 0) {
                        employees[j].active_tasks--;
                        break;
                    }
                }
                
                collected_count++;
                agent_status.tasks_completed++;
            }
        }
    }
    
    pthread_mutex_unlock(&assignment_mutex);
    return collected_count;
}

int count_completed_tasks(void) {
    pthread_mutex_lock(&assignment_mutex);
    
    int completed = 0;
    for (int i = 0; i < assignment_count; i++) {
        if (task_assignments[i].is_completed) {
            completed++;
        }
    }
    
    pthread_mutex_unlock(&assignment_mutex);
    return completed;
}

int handle_task_timeouts(void) {
    pthread_mutex_lock(&assignment_mutex);
    
    time_t current_time = time(NULL);
    int timeout_count = 0;
    
    for (int i = 0; i < assignment_count; i++) {
        if (task_assignments[i].is_sent && !task_assignments[i].is_completed) {
            if (current_time - task_assignments[i].assigned_time > TASK_TIMEOUT_SECONDS) {
                printf("[Employer] Task %s timed out on %s, will reassign\n", 
                       task_assignments[i].task_id, task_assignments[i].employee_ip);
                
                // Mark for reassignment
                task_assignments[i].is_sent = false;
                task_assignments[i].retry_count++;
                
                // Update employee reliability
                for (int j = 0; j < employee_count; j++) {
                    if (strcmp(employees[j].ip_address, task_assignments[i].employee_ip) == 0) {
                        employees[j].reliability_score -= 10;
                        employees[j].active_tasks--;
                        break;
                    }
                }
                
                timeout_count++;
            }
        }
    }
    
    pthread_mutex_unlock(&assignment_mutex);
    return timeout_count;
}

int send_file_to_employee(const char* employee_ip, const char* filepath, const char* task_id) {
    if (!employee_ip || !filepath || !task_id) {
        return -1;
    }
    
    printf("[Employer] Connecting to employee %s to send task %s\n", employee_ip, task_id);
    
    // Create TCP connection to employee
    int sockfd = create_tcp_connection(employee_ip, EMPLOYEE_PORT);
    if (sockfd < 0) {
        printf("[Employer] Failed to connect to employee %s\n", employee_ip);
        return -1;
    }
    
    // Wait for acceptance/rejection
    char response[64];
    ssize_t bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    if (bytes_received <= 0) {
        printf("[Employer] No response from employee %s\n", employee_ip);
        close(sockfd);
        return -1;
    }
    
    response[bytes_received] = '\0';
    
    if (strncmp(response, "ACCEPT", 6) != 0) {
        printf("[Employer] Employee %s rejected task: %s\n", employee_ip, response);
        close(sockfd);
        return -1;
    }
    
    printf("[Employer] Employee %s accepted connection, sending file...\n", employee_ip);
    
    // Send task metadata
    cJSON *metadata = create_task_metadata(task_id, filepath, "employer", employee_ip, "pending");
    if (send_json(sockfd, metadata) != PROTOCOL_OK) {
        printf("[Employer] Failed to send metadata to %s\n", employee_ip);
        cJSON_Delete(metadata);
        close(sockfd);
        return -1;
    }
    cJSON_Delete(metadata);
    
    // Send file content
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        printf("[Employer] Failed to open file %s\n", filepath);
        close(sockfd);
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Send file size first
    uint32_t net_size = htonl((uint32_t)file_size);
    if (send(sockfd, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
        printf("[Employer] Failed to send file size to %s\n", employee_ip);
        fclose(file);
        close(sockfd);
        return -1;
    }
    
    // Send file content in chunks
    char buffer[4096];
    size_t total_sent = 0;
    while (total_sent < (size_t)file_size) {
        size_t to_read = sizeof(buffer);
        if (total_sent + to_read > (size_t)file_size) {
            to_read = file_size - total_sent;
        }
        
        size_t bytes_read = fread(buffer, 1, to_read, file);
        if (bytes_read == 0) break;
        
        ssize_t bytes_sent = send(sockfd, buffer, bytes_read, 0);
        if (bytes_sent <= 0) {
            printf("[Employer] Failed to send file data to %s\n", employee_ip);
            fclose(file);
            close(sockfd);
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    fclose(file);
    
    printf("[Employer] Successfully sent file %s (%ld bytes) to %s\n", 
           filepath, file_size, employee_ip);
    
    // Keep connection open for result reception (in a real implementation)
    // For now, close it
    close(sockfd);
    
    return 0;
}

// Round-robin task distribution
static void distribute_tasks_round_robin(void) {
    int current_employee = 0;
    
    for (int chunk_idx = 0; chunk_idx < num_chunks; chunk_idx++) {
        // Find next available employee using round-robin
        int attempts = 0;
        while (attempts < employee_count) {
            if (employees[current_employee].is_available && 
                employees[current_employee].active_tasks < 3) { // Max 3 tasks per employee
                
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s", CHUNKED_SET_PATH, chunk_files[chunk_idx]);
                
                printf("[Employer] Assigning %s to employee %s (%s)\n", 
                       chunk_files[chunk_idx], 
                       employees[current_employee].employee_id,
                       employees[current_employee].ip_address);
                
                // Create task assignment
                task_assignment_t assignment;
                strncpy(assignment.task_id, chunk_files[chunk_idx], sizeof(assignment.task_id) - 1);
                strncpy(assignment.chunk_file, filepath, sizeof(assignment.chunk_file) - 1);
                strncpy(assignment.employee_id, employees[current_employee].employee_id, sizeof(assignment.employee_id) - 1);
                strncpy(assignment.employee_ip, employees[current_employee].ip_address, sizeof(assignment.employee_ip) - 1);
                assignment.assigned_time = time(NULL);
                assignment.is_completed = false;
                assignment.is_sent = false;
                
                // Add to task queue
                if (add_task_assignment(&assignment) == 0) {
                    employees[current_employee].active_tasks++;
                    printf("[Employer] Task %s queued for %s\n", assignment.task_id, assignment.employee_ip);
                } else {
                    printf("[Employer] Failed to queue task %s\n", chunk_files[chunk_idx]);
                }
                
                break;
            }
            
            current_employee = (current_employee + 1) % employee_count;
            attempts++;
        }
        
        if (attempts >= employee_count) {
            printf("[Employer] No available employees for task %s, will retry later\n", chunk_files[chunk_idx]);
        }
        
        // Move to next employee for next task (round-robin)
        current_employee = (current_employee + 1) % employee_count;
    }
}

// Task execution management
static void manage_task_execution(void) {
    printf("[Employer] Starting task execution management...\n");
    
    int completed_tasks = 0;
    int total_tasks = num_chunks;
    time_t last_status_update = time(NULL);
    
    while (completed_tasks < total_tasks && agent_running) {
        // Send pending tasks
        send_pending_tasks();
        
        // Check for completed tasks and collect results
        check_and_collect_results();
        
        // Update completion count
        completed_tasks = count_completed_tasks();
        
        // Print status update every 10 seconds
        time_t current_time = time(NULL);
        if (current_time - last_status_update >= 10) {
            printf("[Employer] Progress: %d/%d tasks completed\n", completed_tasks, total_tasks);
            last_status_update = current_time;
        }
        
        // Handle timeouts and reassign failed tasks
        handle_task_timeouts();
        
        // Brief sleep to prevent busy waiting
        sleep(1);
    }
    
    printf("[Employer] All tasks completed! (%d/%d)\n", completed_tasks, total_tasks);
}

// Main employer loop
static void* employer_main_loop(void* arg) {
    (void)arg;
    
    int sockfd;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    fd_set readfds;
    struct timeval timeout;
    
    // Create UDP socket for employee discovery
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }
    
    // Enable broadcast and address reuse
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt broadcast");
        close(sockfd);
        return NULL;
    }
    
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt reuse");
        close(sockfd);
        return NULL;
    }
    
    // Bind socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return NULL;
    }
    
    printf("[Employer] Listening for employee broadcasts on port %d\n", PORT);
    
    agent_status.mode = AGENT_MODE_EMPLOYER;
    agent_status.start_time = time(NULL);
    agent_status.is_running = true;
    
    // Scan for chunk files
    scan_chunked_set();
    if (num_chunks == 0) {
        printf("[Employer] No chunk files found. Cannot proceed.\n");
        close(sockfd);
        return NULL;
    }
    
    // Employee discovery phase
    printf("[Employer] Starting employee discovery phase...\n");
    time_t discovery_start = time(NULL);
    
    while (agent_running && (time(NULL) - discovery_start) < COLLECTION_TIMEOUT) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        if (activity > 0 && FD_ISSET(sockfd, &readfds)) {
            ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                                            (struct sockaddr*)&client_addr, &client_len);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                
                // Parse JSON broadcast
                cJSON *json = cJSON_Parse(buffer);
                if (json) {
                    const cJSON *type = cJSON_GetObjectItem(json, "type");
                    const cJSON *mode = cJSON_GetObjectItem(json, "mode");
                    
                    if (type && cJSON_IsString(type) && 
                        strcmp(type->valuestring, "volcom_broadcast") == 0 &&
                        mode && cJSON_IsString(mode) && 
                        strcmp(mode->valuestring, "employee") == 0) {
                        
                        add_or_update_employee(client_ip, json);
                    }
                    
                    cJSON_Delete(json);
                }
            }
        }
        
        // Remove stale employees
        remove_stale_employees();
    }
    
    printf("[Employer] Discovery phase complete. Found %d employees\n", employee_count);
    
    if (employee_count == 0) {
        printf("[Employer] No employees found. Cannot proceed with task distribution.\n");
        close(sockfd);
        return NULL;
    }
    
    // Task distribution and management
    distribute_tasks_round_robin();
    manage_task_execution();
    
    close(sockfd);
    return NULL;
}

// Agent lifecycle
int init_agent(agent_mode_t mode) {
    memset(&agent_status, 0, sizeof(agent_status));
    agent_status.mode = mode;
    
    return 0;
}

int start_agent(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    agent_running = true;
    
    if (pthread_create(&employer_thread, NULL, employer_main_loop, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }
    
    return 0;
}

int stop_agent(void) {
    agent_running = false;
    
    if (employer_thread) {
        pthread_join(employer_thread, NULL);
    }
    
    agent_status.is_running = false;
    return 0;
}

int get_agent_status(agent_status_t* status) {
    if (!status) return -1;
    
    memcpy(status, &agent_status, sizeof(agent_status_t));
    return 0;
}

int run_employer_mode(void) {
    printf("[Employer] Starting employer mode...\n");
    
    if (init_agent(AGENT_MODE_EMPLOYER) != 0) {
        fprintf(stderr, "Failed to initialize agent\n");
        return -1;
    }
    
    if (start_agent() != 0) {
        fprintf(stderr, "Failed to start agent\n");
        return -1;
    }
    
    // Wait for completion or interruption
    pthread_join(employer_thread, NULL);
    
    printf("[Employer] Employer mode completed\n");
    return 0;
}
