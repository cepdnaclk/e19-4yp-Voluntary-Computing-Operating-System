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

// Global state
static agent_status_t agent_status;
static employee_node_t employees[MAX_EMPLOYEES];
static int employee_count = 0;
static bool agent_running = false;
static pthread_t employer_thread;
static char chunk_files[MAX_CHUNKS][MAX_FILENAME_LEN];
static int num_chunks = 0;

// Signal handler
static void signal_handler(int sig) {
    (void)sig;
    agent_running = false;
    printf("\nShutting down agent...\n");
}

// Agent lifecycle
int init_agent(agent_mode_t mode) {
    memset(&agent_status, 0, sizeof(agent_status));
    snprintf(agent_status.agent_id, sizeof(agent_status.agent_id), "agent_%ld", time(NULL));
    agent_status.mode = mode;
    agent_status.start_time = time(NULL);
    agent_status.is_active = false;
    agent_status.tasks_completed = 0;
    agent_status.tasks_failed = 0;
    
    get_local_ip(agent_status.ip_address, sizeof(agent_status.ip_address));
    agent_status.port = PORT;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    return 0;
}

void cleanup_agent(void) {
    agent_running = false;
    agent_status.is_active = false;
    employee_count = 0;
    printf("Agent cleanup completed\n");
}

agent_status_t get_agent_status(void) {
    return agent_status;
}

// Utility functions
static void scan_chunked_set(void) {
    DIR *dir;
    struct dirent *entry;

    num_chunks = 0;
    dir = opendir(CHUNKED_SET_PATH);
    if (!dir) {
        printf("Warning: Failed to open chunked_set directory %s\n", CHUNKED_SET_PATH);
        return;
    }

    while ((entry = readdir(dir)) != NULL && num_chunks < MAX_CHUNKS) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Build full path and check if it's a regular file
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", CHUNKED_SET_PATH, entry->d_name);
        
        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(chunk_files[num_chunks], entry->d_name, MAX_FILENAME_LEN - 1);
            chunk_files[num_chunks][MAX_FILENAME_LEN - 1] = '\0';
            num_chunks++;
        }
    }
    closedir(dir);

    printf("[Employer] Found %d chunk files in chunked_set.\n", num_chunks);
}

static void remove_stale_employees(void) {
    time_t current_time = time(NULL);
    
    for (int i = 0; i < employee_count; i++) {
        if (current_time - employees[i].last_seen > STALE_THRESHOLD) {
            printf("[Employer] Removing stale employee %s\n", employees[i].employee_id);
            
            // Shift array elements
            memmove(&employees[i], &employees[i + 1], 
                   (employee_count - i - 1) * sizeof(employee_node_t));
            employee_count--;
            i--; // Recheck this position
        }
    }
}

static int add_or_update_employee(const char* ip, const cJSON* broadcast_data) {
    time_t current_time = time(NULL);
    
    // Check if employee already exists
    for (int i = 0; i < employee_count; i++) {
        if (strcmp(employees[i].ip_address, ip) == 0) {
            // Update existing employee
            employees[i].last_seen = current_time;
            employees[i].is_available = true;
            
            // Update system info if available
            const cJSON* cpu = cJSON_GetObjectItem(broadcast_data, "cpu_percent");
            const cJSON* memory = cJSON_GetObjectItem(broadcast_data, "memory_percent");
            
            if (cpu && cJSON_IsNumber(cpu)) {
                employees[i].cpu_usage = cpu->valuedouble;
            }
            if (memory && cJSON_IsNumber(memory)) {
                employees[i].memory_usage = memory->valuedouble;
            }
            
            return i;
        }
    }
    
    // Add new employee if space available
    if (employee_count < MAX_EMPLOYEES) {
        employee_node_t* emp = &employees[employee_count];
        
        snprintf(emp->employee_id, sizeof(emp->employee_id), "emp_%s_%ld", ip, current_time);
        strncpy(emp->ip_address, ip, sizeof(emp->ip_address) - 1);
        emp->last_seen = current_time;
        emp->is_available = true;
        emp->active_tasks = 0;
        emp->reliability_score = 100; // Start with high reliability
        
        // Extract system info from broadcast
        const cJSON* cpu = cJSON_GetObjectItem(broadcast_data, "cpu_percent");
        const cJSON* memory = cJSON_GetObjectItem(broadcast_data, "memory_percent");
        
        emp->cpu_usage = cpu && cJSON_IsNumber(cpu) ? cpu->valuedouble : 0.0;
        emp->memory_usage = memory && cJSON_IsNumber(memory) ? memory->valuedouble : 0.0;
        
        employee_count++;
        printf("[Employer] Added new employee %s at %s\n", emp->employee_id, ip);
        return employee_count - 1;
    }
    
    return -1; // No space for new employee
}

// New function to handle round-robin task distribution
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

// New function to manage task execution and result collection
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

// Employer mode implementation
static void* employer_main_loop(void* arg) {
    (void)arg;
    
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Scan the chunked_set directory for chunk files
    scan_chunked_set();

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return NULL;
    }

    printf("[Employer] Listening for employee broadcasts on port %d...\n", PORT);
    agent_status.is_active = true;

    time_t first_found_time = 0;
    time_t last_cleanup = time(NULL);

    while (agent_running) {
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (result < 0) {
            if (errno != EINTR) {
                perror("select");
            }
            continue;
        }
        
        if (result == 0) {
            // Timeout - perform maintenance tasks
            time_t current_time = time(NULL);
            if (current_time - last_cleanup > STALE_THRESHOLD / 2) {
                remove_stale_employees();
                last_cleanup = current_time;
            }
            continue;
        }
        
        if (!FD_ISSET(sockfd, &readfds)) {
            continue;
        }

        ssize_t len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr *) &client_addr, &addr_len);
        if (len < 0) {
            if (errno != EINTR) {
                perror("recvfrom");
            }
            continue;
        }

        buffer[len] = '\0';
        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);

        // Parse JSON broadcast
        cJSON *json = cJSON_Parse(buffer);
        if (!json) {
            printf("[Employer] Invalid JSON from %s\n", sender_ip);
            continue;
        }

        // Validate it's an employee broadcast
        const cJSON* type = cJSON_GetObjectItem(json, "type");
        if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "employee_broadcast") != 0) {
            cJSON_Delete(json);
            continue;
        }

        // Add or update employee
        int emp_index = add_or_update_employee(sender_ip, json);
        if (emp_index >= 0) {
            if (first_found_time == 0) {
                first_found_time = time(NULL);
                printf("[Employer] First employee found. Starting collection timer...\n");
            }
        }

        cJSON_Delete(json);

        // Check if we should start distributing tasks
        if (first_found_time > 0 && 
            (time(NULL) - first_found_time) >= COLLECTION_TIMEOUT && 
            employee_count > 0) {
            
            printf("[Employer] Collection timeout reached. Found %d employees.\n", employee_count);
            printf("[Employer] Starting task distribution...\n");
            
            // Start round-robin task distribution
            distribute_tasks_round_robin();
            
            break; // Exit discovery loop to start task management
        }
    }

    close(sockfd);
    
    // Continue with task management and result collection
    if (employee_count > 0 && num_chunks > 0) {
        manage_task_execution();
    }
    
    agent_status.is_active = false;
    printf("[Employer] Employer mode stopped\n");
    return NULL;
}

// New function to handle round-robin task distribution
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

// New function to manage task execution and result collection
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

// Task assignment management
#define MAX_TASK_ASSIGNMENTS 100
#define TASK_TIMEOUT_SECONDS 60

static task_assignment_t task_assignments[MAX_TASK_ASSIGNMENTS];
static int assignment_count = 0;
static pthread_mutex_t assignment_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function forward declarations
static void signal_handler(int sig);
static void scan_chunked_set(void);
static void remove_stale_employees(void);
static int add_or_update_employee(const char* ip, const cJSON* broadcast_data);
static void* employer_main_loop(void* arg);
static void distribute_tasks_round_robin(void);
static void manage_task_execution(void);

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
