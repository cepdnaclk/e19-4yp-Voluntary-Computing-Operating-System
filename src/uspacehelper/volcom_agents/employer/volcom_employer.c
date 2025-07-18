#define _GNU_SOURCE

#include "volcom_agents.h"

#include "volcom_employer.h"
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

// TODO: Get those from config
#define PORT 9876
#define BUFFER_SIZE 2048
#define STALE_THRESHOLD 15
#define MAX_EMPLOYEES 100
#define CHUNKED_SET_PATH "/home/dasun/Projects/e19-4yp-Voluntary-Computing-Operating-System/src/uspacehelper/scripts"
#define RESULTS_PATH "/home/dasun/Projects/e19-4yp-Voluntary-Computing-Operating-System/src/uspacehelper/results"
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
static int receive_result_from_employee(employee_node_t* employee);

// TODO: Move
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
            // If connection was dropped, try to reconnect
            if (employees[i].sockfd < 0) {
                employees[i].sockfd = create_tcp_connection(ip, EMPLOYEE_PORT);
                if (employees[i].sockfd >= 0) {
                    printf("[Employer] Re-established connection with employee %s\n", ip);
                }
            }
            employees[i].is_available = (employees[i].sockfd >= 0);
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
        emp->active_tasks = 0;
        emp->reliability_score = 100;
        
        // Attempt to create persistent TCP connection
        printf("[Employer] New employee %s discovered. Attempting to connect...\n", ip);
        emp->sockfd = create_tcp_connection(ip, EMPLOYEE_PORT);
        if (emp->sockfd < 0) {
            printf("[Employer] Warning: Failed to establish persistent connection with %s\n", ip);
            emp->is_available = false;
        } else {
            printf("[Employer] Persistent connection established with %s\n", ip);
            emp->is_available = true;
        }
        
        employee_count++;
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
            // Find the employee for this task
            employee_node_t* employee = NULL;
            for (int j = 0; j < employee_count; j++) {
                if (strcmp(employees[j].ip_address, task_assignments[i].employee_ip) == 0) {
                    employee = &employees[j];
                    break;
                }
            }

            if (employee && employee->is_available && employee->sockfd >= 0) {
                // Send task to employee using the persistent connection
                int result = send_file_to_employee(employee->sockfd, 
                                                 task_assignments[i].chunk_file,
                                                 task_assignments[i].task_id,
                                                 employee->ip_address);
                
                if (result == 0) {
                    task_assignments[i].is_sent = true;
                    printf("[Employer] Successfully sent task %s to %s\n", 
                           task_assignments[i].task_id, task_assignments[i].employee_ip);
                    sent_count++;
                } else {
                    // If sending fails, the connection is likely dead.
                    // Close the socket and mark employee as unavailable for now.
                    // The main loop will attempt to reconnect later.
                    close(employee->sockfd);
                    employee->sockfd = -1;
                    employee->is_available = false;
                    
                    task_assignments[i].retry_count++;
                    printf("[Employer] Failed to send task %s to %s (retry %d). Connection lost.\n", 
                           task_assignments[i].task_id, task_assignments[i].employee_ip,
                           task_assignments[i].retry_count);
                }
            }
        }
    }
    
    pthread_mutex_unlock(&assignment_mutex);
    return sent_count;
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

// Modified to use a persistent connection
int send_file_to_employee(int sockfd, const char* filepath, const char* task_id, const char* employee_ip) {

    if (sockfd < 0 || !filepath || !task_id) {
        return -1;
    }
    
    printf("[Employer] Using persistent connection to send task %s to %s\n", task_id, employee_ip);
    
    // Send task metadata
    cJSON *metadata = create_task_metadata(task_id, filepath, "employer", employee_ip, "pending");
    if (send_json(sockfd, metadata) != PROTOCOL_OK) {
        printf("[Employer] Failed to send metadata to %s\n", employee_ip);
        cJSON_Delete(metadata);
        return -1;
    }
    cJSON_Delete(metadata);
    
    // Send file content
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        printf("[Employer] Failed to open file %s\n", filepath);
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
            printf("[Employer] Failed to send file data to %s. Connection may be lost.\n", employee_ip);
            fclose(file);
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    fclose(file);
    
    printf("[Employer] Successfully sent file %s (%ld bytes) to %s\n", 
           filepath, file_size, employee_ip);
    
    // The connection is kept open for future communication
    
    return 0;
}

static int receive_result_from_employee(employee_node_t* employee) {

    cJSON *metadata = NULL;
    if (recv_json(employee->sockfd, &metadata) != PROTOCOL_OK) {
        printf("[Employer] Failed to receive result metadata from %s\n", employee->ip_address);
        return -1;
    }

    const cJSON *type = cJSON_GetObjectItem(metadata, "type");
    const cJSON *task_id_json = cJSON_GetObjectItem(metadata, "task_id");

    if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "task_result") != 0 || !task_id_json || !cJSON_IsString(task_id_json)) {
        printf("[Employer] Invalid result metadata from %s\n", employee->ip_address);
        cJSON_Delete(metadata);
        return 0; // Not a fatal error, just wrong message type
    }

    char task_id[MAX_FILENAME_LEN];
    strncpy(task_id, task_id_json->valuestring, sizeof(task_id) - 1);
    cJSON_Delete(metadata);

    // Receive file size
    uint32_t net_size;
    if (recv(employee->sockfd, &net_size, sizeof(net_size), MSG_WAITALL) != sizeof(net_size)) {
        printf("[Employer] Failed to receive result file size from %s\n", employee->ip_address);
        return -1;
    }
    uint32_t file_size = ntohl(net_size);

    // Construct result filepath
    char result_filepath[512];
    snprintf(result_filepath, sizeof(result_filepath), "%s/result_%s", RESULTS_PATH, task_id);

    FILE* file = fopen(result_filepath, "wb");
    if (!file) {
        perror("fopen result file");
        // Consume and discard the data from the socket to avoid desync
        char discard_buffer[1024];
        size_t total_discarded = 0;
        while (total_discarded < file_size) {
            size_t to_read = sizeof(discard_buffer);
            if (total_discarded + to_read > file_size) to_read = file_size - total_discarded;
            ssize_t discarded = recv(employee->sockfd, discard_buffer, to_read, 0);
            if (discarded <= 0) break;
            total_discarded += discarded;
        }
        return 0; // Not a connection error
    }

    // Receive file content
    char buffer[4096];
    size_t total_received = 0;
    while (total_received < file_size) {
        size_t to_receive = sizeof(buffer);
        if (total_received + to_receive > file_size) to_receive = file_size - total_received;
        ssize_t bytes_received = recv(employee->sockfd, buffer, to_receive, 0);
        if (bytes_received <= 0) {
            printf("[Employer] Failed to receive result file data from %s\n", employee->ip_address);
            fclose(file);
            return -1; // Connection error
        }
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;
    }
    fclose(file);

    printf("[Employer] Successfully received result for task %s. Saved to %s\n", task_id, result_filepath);

    // Update task and employee status
    pthread_mutex_lock(&assignment_mutex);
    for (int i = 0; i < assignment_count; i++) {
        if (!task_assignments[i].is_completed && strcmp(task_assignments[i].task_id, task_id) == 0) {
            task_assignments[i].is_completed = true;
            task_assignments[i].completed_time = time(NULL);
            if (employee->active_tasks > 0) {
                employee->active_tasks--;
            }
            break;
        }
    }
    pthread_mutex_unlock(&assignment_mutex);

    return 0; // Success
}

// Distributes unassigned tasks to available employees
static void distribute_new_tasks(void) {

    static int last_assigned_chunk = 0;
    static int last_used_employee = 0;

    if (employee_count == 0) return;

    // Check if all tasks have been assigned
    if (last_assigned_chunk >= num_chunks) {
        return;
    }

    // Find next available employee using round-robin
    int attempts = 0;
    while (attempts < employee_count) {
        employee_node_t* current_employee = &employees[last_used_employee];

        if (current_employee->is_available && current_employee->active_tasks < 3) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", CHUNKED_SET_PATH, chunk_files[last_assigned_chunk]);

            printf("[Employer] Assigning %s to employee %s (%s)\n", 
                   chunk_files[last_assigned_chunk], 
                   current_employee->employee_id,
                   current_employee->ip_address);

            // Create task assignment
            task_assignment_t assignment;
            strncpy(assignment.task_id, chunk_files[last_assigned_chunk], sizeof(assignment.task_id) - 1);
            strncpy(assignment.chunk_file, filepath, sizeof(assignment.chunk_file) - 1);
            strncpy(assignment.employee_id, current_employee->employee_id, sizeof(assignment.employee_id) - 1);
            strncpy(assignment.employee_ip, current_employee->ip_address, sizeof(assignment.employee_ip) - 1);
            assignment.assigned_time = time(NULL);
            assignment.is_completed = false;
            assignment.is_sent = false;
            assignment.retry_count = 0;

            // Add to task queue
            if (add_task_assignment(&assignment) == 0) {
                current_employee->active_tasks++;
                printf("[Employer] Task %s queued for %s\n", assignment.task_id, assignment.employee_ip);
                last_assigned_chunk++; // Move to the next chunk
            } else {
                printf("[Employer] Failed to queue task %s\n", chunk_files[last_assigned_chunk]);
            }
            
            // Move to the next employee for the next assignment
            last_used_employee = (last_used_employee + 1) % employee_count;
            return; // Assign one task per call
        }
        
        last_used_employee = (last_used_employee + 1) % employee_count;
        attempts++;
    }
}


// Main employer loop - refactored for continuous discovery
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
    agent_status.is_active = true;
    
    // Create results directory if it doesn't exist
    mkdir(RESULTS_PATH, 0777);

    // Scan for chunk files once at the start
    scan_chunked_set();
    if (num_chunks == 0) {
        printf("[Employer] No chunk files found. Exiting.\n");
        close(sockfd);
        return NULL;
    }
    
    time_t last_status_update = time(NULL);
    int completed_tasks = 0;

    // Main loop for continuous discovery and task management
    while (agent_running) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds); // Add UDP listener
        int max_fd = sockfd;

        // Add all active employee sockets to the set
        for (int i = 0; i < employee_count; i++) {
            if (employees[i].sockfd >= 0) {
                FD_SET(employees[i].sockfd, &readfds);
                if (employees[i].sockfd > max_fd) {
                    max_fd = employees[i].sockfd;
                }
            }
        }
        
        timeout.tv_sec = 1; // Set timeout to 1 second
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }
        
        // 1. Discover Employees (if there's data on the UDP socket)
        if (FD_ISSET(sockfd, &readfds)) {
            ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                                            (struct sockaddr*)&client_addr, &client_len);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                
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
        
        // 2. Check for incoming results from employees
        for (int i = 0; i < employee_count; i++) {
            if (employees[i].sockfd >= 0 && FD_ISSET(employees[i].sockfd, &readfds)) {
                if (receive_result_from_employee(&employees[i]) != 0) {
                    // Handle error/disconnection
                    printf("[Employer] Connection lost with employee %s while receiving result.\n", employees[i].ip_address);
                    close(employees[i].sockfd);
                    employees[i].sockfd = -1;
                    employees[i].is_available = false;
                }
            }
        }

        // 3. Distribute New Tasks
        distribute_new_tasks();

        // 4. Manage Ongoing Tasks
        send_pending_tasks();
        handle_task_timeouts();
        
        // 5. Maintain Employee List
        remove_stale_employees();
        
        // 6. Report Status
        completed_tasks = count_completed_tasks();
        time_t current_time = time(NULL);
        if (current_time - last_status_update >= 10) {
            printf("[Employer] Status: %d employees | %d/%d tasks completed.\n", 
                   employee_count, completed_tasks, num_chunks);
            last_status_update = current_time;
        }

        // 7. Check for Completion
        if (completed_tasks >= num_chunks) {
            printf("[Employer] All tasks completed! Shutting down in 10 seconds.\n");
            sleep(10);
            agent_running = false;
        }
    }
    
    printf("[Employer] Main loop finished.\n");

    // Clean up: close all persistent connections
    for (int i = 0; i < employee_count; i++) {
        if (employees[i].sockfd >= 0) {
            close(employees[i].sockfd);
        }
    }

    close(sockfd);
    return NULL;
}

// Agent lifecycle
int init_agent(agent_mode_t mode) {

    memset(&agent_status, 0, sizeof(agent_status));
    agent_status.mode = mode;
    
    return 0;
}

void cleanup_agent(void) {

    // Clean up any resources
    agent_running = false;
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
    
    agent_status.is_active = false;
    return 0;
}

agent_status_t get_agent_status(void) {
    
    return agent_status;
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
