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
#define CHUNKED_SET_PATH "./scripts/"
#define RESULTS_PATH "./results"
#define MAX_CHUNKS 1024
#define MAX_FILENAME_LEN 256
#define EMPLOYEE_PORT 12345

static task_assignment_t task_assignments[MAX_TASK_ASSIGNMENTS];
static int assignment_count = 0;
static pthread_mutex_t assignment_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global state
static agent_status_t agent_status;
static employee_node_t* employees[MAX_EMPLOYEES];
static int employee_count = 0;
static pthread_mutex_t employee_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static int send_initial_config(employee_node_t* employee);
static int receive_result_from_employee(employee_node_t* employee);

// TODO: Move
// Signal handler
static void signal_handler(int sig) {

    (void)sig;
    // agent_running = false; // This should be handled by the main executable
    printf("\nShutting down agent...\n");
}

// Remove stale employees
static void remove_stale_employees(void) {

    pthread_mutex_lock(&employee_mutex);
    time_t current_time = time(NULL);
    int i = 0;

    while (i < employee_count) {
        if (current_time - employees[i]->last_seen > STALE_THRESHOLD) {
            printf("[Employer] Removing stale employee %s (%s)\n", 
                   employees[i]->employee_id, employees[i]->ip_address);

            if(employees[i]->sockfd >= 0) close(employees[i]->sockfd);
            free(employees[i]);

            // Move last employee to this position
            if (i < employee_count - 1) {
                employees[i] = employees[employee_count - 1];
            }
            employee_count--;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&employee_mutex);
}

// Add or update employee
static employee_node_t* add_or_update_employee(const char* ip, const cJSON* broadcast_data) {

    pthread_mutex_lock(&employee_mutex);
    time_t current_time = time(NULL);

    // Check if employee already exists
    for (int i = 0; i < employee_count; i++) {
        if (strcmp(employees[i]->ip_address, ip) == 0) {
            employees[i]->last_seen = current_time;
            // If connection was dropped, try to reconnect
            if (employees[i]->sockfd < 0) {
                employees[i]->sockfd = create_tcp_connection(ip, EMPLOYEE_PORT);
                if (employees[i]->sockfd >= 0) {
                    printf("[Employer] Re-established connection with employee %s\n", ip);
                    employees[i]->state = EMPLOYEE_STATE_NEW; // Reset state on reconnect
                }
            }
            pthread_mutex_unlock(&employee_mutex);
            return employees[i];
        }
    }

    // Add new employee if space available
    if (employee_count < MAX_EMPLOYEES) {
        employee_node_t *new_employee = (employee_node_t*)malloc(sizeof(employee_node_t));
        if (!new_employee) {
            pthread_mutex_unlock(&employee_mutex);
            return NULL;
        }

        strncpy(new_employee->ip_address, ip, sizeof(new_employee->ip_address) - 1);

        // Extract employee info from broadcast data
        const cJSON *id = cJSON_GetObjectItem(broadcast_data, "employee_id");
        if (id && cJSON_IsString(id)) {
            strncpy(new_employee->employee_id, id->valuestring, sizeof(new_employee->employee_id) - 1);
        } else {
            snprintf(new_employee->employee_id, sizeof(new_employee->employee_id), "emp_%d", employee_count);
        }

        new_employee->last_seen = current_time;
        new_employee->active_tasks = 0;
        new_employee->reliability_score = 100;
        new_employee->tasks_completed = 0;
        new_employee->tasks_failed = 0;
        new_employee->state = EMPLOYEE_STATE_NEW; // Initial state

        // Establish persistent TCP connection
        new_employee->sockfd = create_tcp_connection(ip, EMPLOYEE_PORT);
        if (new_employee->sockfd < 0) {
            printf("[Employer] Warning: Failed to establish persistent connection with %s\n", ip);
        } else {
            printf("[Employer] Persistent connection established with %s\n", ip);
        }

        employees[employee_count] = new_employee;
        employee_count++;

        pthread_mutex_unlock(&employee_mutex);
        return new_employee;
    }

    pthread_mutex_unlock(&employee_mutex);
    return NULL; // No space for new employee
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
                if (strcmp(employees[j]->ip_address, task_assignments[i].employee_ip) == 0) {
                    employee = employees[j];
                    break;
                }
            }

            if (employee && employee->sockfd >= 0 && employee->state == EMPLOYEE_STATE_CONFIGURED) {
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
                pthread_mutex_lock(&employee_mutex);
                for (int j = 0; j < employee_count; j++) {
                    if (strcmp(employees[j]->ip_address, task_assignments[i].employee_ip) == 0) {
                        employees[j]->reliability_score -= 10;
                        if(employees[j]->active_tasks > 0) employees[j]->active_tasks--;
                        break;
                    }
                }

                pthread_mutex_unlock(&employee_mutex);
                
                timeout_count++;
            }
        }
    }
    
    pthread_mutex_unlock(&assignment_mutex);
    return timeout_count;
}

// Send the initial configuration script to a new employee
static int send_initial_config(employee_node_t* employee) {
    // Assuming the script is named "script"
    const char *filename = "/unix_socket.js";
    char config_filepath[512];  // Make sure the buffer is large enough

    snprintf(config_filepath, sizeof(config_filepath), "%s%s", CHUNKED_SET_PATH, filename);
    printf("[Employer] Sending initial config '%s' to %s\n", config_filepath, employee->ip_address);

    // 1. Send metadata
    // TODO: get file type not hardcoded
    cJSON *metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(metadata, "message_type", "initial_config");
    cJSON_AddStringToObject(metadata, "task_id", "init_script");
    cJSON_AddStringToObject(metadata, "chunk_filename", "script.js");
    cJSON_AddStringToObject(metadata, "sender_id", "employer");
    if (send_json(employee->sockfd, metadata) != PROTOCOL_OK) {
        printf("[Employer] Failed to send initial_config metadata to %s\n", employee->ip_address);
        cJSON_Delete(metadata);
        return -1;
    }
    cJSON_Delete(metadata);

    // 2. Send file content (reusing parts of send_file_to_employee logic)
    FILE *file = fopen(config_filepath, "rb");
    if (!file) {
        printf("[Employer] ERROR: Could not open initial config file '%s'\n", config_filepath);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint32_t net_size = htonl((uint32_t)file_size);
    if (send(employee->sockfd, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
        printf("[Employer] Failed to send config file size to %s\n", employee->ip_address);
        fclose(file);
        return -1;
    }

    char buffer[4096];
    size_t total_sent = 0;
    while (total_sent < (size_t)file_size) {
        size_t to_read = sizeof(buffer);
        if (total_sent + to_read > (size_t)file_size) {
            to_read = file_size - total_sent;
        }
        size_t bytes_read = fread(buffer, 1, to_read, file);
        if (bytes_read == 0) break;

        ssize_t bytes_sent = send(employee->sockfd, buffer, bytes_read, 0);
        if (bytes_sent <= 0) {
            printf("[Employer] Failed to send config file data to %s.\n", employee->ip_address);
            fclose(file);
            return -1;
        }
        total_sent += bytes_sent;
    }
    fclose(file);

    printf("[Employer] Successfully sent initial config to %s\n", employee->ip_address);
    employee->state = EMPLOYEE_STATE_CONFIGURED; // Update state
    return 0;
}


// Modified to use a persistent connection and send data chunks
int send_file_to_employee(int sockfd, const char* filepath, const char* task_id, const char* employee_ip) {

    if (sockfd < 0 || !filepath || !task_id) {
        return -1;
    }
    
    printf("[Employer] Using persistent connection to send data chunk %s to %s\n", task_id, employee_ip);
    
    // Send task metadata
    cJSON *metadata = create_task_metadata(task_id, filepath, "employer", employee_ip, "pending");
    cJSON_AddStringToObject(metadata, "message_type", "data_chunk"); // Specify message type
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
static void distribute_new_tasks(const char* task_file_path) {

    static int last_used_employee = 0;

    if (employee_count == 0) return;

    // Find next available employee using round-robin
    int attempts = 0;
    pthread_mutex_lock(&employee_mutex);
    while (attempts < employee_count) {
        employee_node_t* current_employee = employees[last_used_employee];

         if (current_employee->sockfd >= 0 && current_employee->state == EMPLOYEE_STATE_CONFIGURED && current_employee->active_tasks < 3) {

            char task_id[MAX_FILENAME_LEN];
            snprintf(task_id, sizeof(task_id), "task_%ld", time(NULL));

            printf("[Employer] Assigning %s to employee %s (%s)\n", 
                   task_file_path, 
                   current_employee->employee_id,
                   current_employee->ip_address);

            // Create task assignment
            task_assignment_t assignment;
            strncpy(assignment.task_id, task_id, sizeof(assignment.task_id) - 1);
            strncpy(assignment.chunk_file, task_file_path, sizeof(assignment.chunk_file) - 1);
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
                //last_assigned_chunk++; // Move to the next chunk
            } else {
                printf("[Employer] Failed to queue task %s\n", task_id);
            }
            
            // Move to the next employee for the next assignment
            last_used_employee = (last_used_employee + 1) % employee_count;
            pthread_mutex_unlock(&employee_mutex);
            return; // Assign one task per call
        }
        
        last_used_employee = (last_used_employee + 1) % employee_count;
        attempts++;
    }
    pthread_mutex_unlock(&employee_mutex);
}


// Scan CHUNKED_SET_PATH for .json files and queue them as tasks
void populate_chunked_tasks() {
    DIR *dir = opendir(CHUNKED_SET_PATH);
    if (!dir) {
        perror("opendir");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", CHUNKED_SET_PATH, entry->d_name);

            task_assignment_t assignment = {0};
            strncpy(assignment.task_id, entry->d_name, sizeof(assignment.task_id) - 1);
            strncpy(assignment.chunk_file, filepath, sizeof(assignment.chunk_file) - 1);
            assignment.is_completed = false;
            assignment.is_sent = false;
            assignment.retry_count = 0;
            // employee_id and employee_ip will be set when assigned
            add_task_assignment(&assignment);
        }
    }
    closedir(dir);
}

// Main employer loop - refactored for continuous discovery and dynamic task queue
void* employer_main_loop(void* arg) {
    (void)arg; // Unused

    // Scan chunked set directory and queue all .json files as tasks
    populate_chunked_tasks();

    int discovery_sockfd;
    struct sockaddr_in addr;
    
    // Create UDP socket for employee discovery
    discovery_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (discovery_sockfd < 0) {
        perror("socket");
        return NULL;
    }

    // Enable broadcast and address reuse
    int broadcast = 1;
    if (setsockopt(discovery_sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt broadcast");
        close(discovery_sockfd);
        return NULL;
    }

    int reuse = 1;
    if (setsockopt(discovery_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt reuse");
        close(discovery_sockfd);
        return NULL;
    }

    // Bind socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(discovery_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(discovery_sockfd);
        return NULL;
    }

    printf("[Employer] Listening for employee broadcasts on port %d\n", PORT);

    agent_status.mode = AGENT_MODE_EMPLOYER;
    agent_status.start_time = time(NULL);
    agent_status.is_active = true;

    // Create results directory if it doesn't exist
    mkdir(RESULTS_PATH, 0777);

    time_t last_status_update = time(NULL);
    int completed_tasks_count = 0;

    // Main loop for continuous discovery and task management
    while (agent_status.is_active) {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(discovery_sockfd, &readfds); // Add UDP listener
        int max_fd = discovery_sockfd;

        // Add all active employee sockets to the set
        pthread_mutex_lock(&employee_mutex);
        for (int i = 0; i < employee_count; i++) {
            if (employees[i]->sockfd >= 0) {
                FD_SET(employees[i]->sockfd, &readfds);
                if (employees[i]->sockfd > max_fd) {
                    max_fd = employees[i]->sockfd;
                }
            }
        }
        pthread_mutex_unlock(&employee_mutex);

        timeout.tv_sec = 1; // Set timeout to 1 second
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }

        // 1. Discover Employees (if there's data on the UDP socket)
        if (FD_ISSET(discovery_sockfd, &readfds)) {
            char buffer[BUFFER_SIZE];
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            ssize_t bytes_received = recvfrom(discovery_sockfd, buffer, BUFFER_SIZE - 1, 0,
                                            (struct sockaddr*)&client_addr, &client_len);

            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

                cJSON *json = cJSON_Parse(buffer);
                if (json) {
                    add_or_update_employee(client_ip, json);
                    cJSON_Delete(json);
                }
            }
        }

        // 2. Check for incoming results from employees
        pthread_mutex_lock(&employee_mutex);
        for (int i = 0; i < employee_count; i++) {
            if (employees[i]->sockfd >= 0 && FD_ISSET(employees[i]->sockfd, &readfds)) {
                if (receive_result_from_employee(employees[i]) != 0) {
                    // Handle error/disconnection
                    printf("[Employer] Connection lost with employee %s while receiving result.\n", employees[i]->ip_address);
                    close(employees[i]->sockfd);
                    employees[i]->sockfd = -1;
                }
            }
        }
        pthread_mutex_unlock(&employee_mutex);

        // 3. Send initial configs to new employees
        pthread_mutex_lock(&employee_mutex);
        for (int i = 0; i < employee_count; i++) {
            if (employees[i]->sockfd >= 0) {
                if (employees[i]->state == EMPLOYEE_STATE_NEW) {
                    if (send_initial_config(employees[i]) != 0) {
                        printf("[Employer] Failed to send initial config to %s. Marking as failed.\n", employees[i]->ip_address);
                        close(employees[i]->sockfd);
                        employees[i]->sockfd = -1; // Mark as disconnected
                    }
                }
            }
        }
        pthread_mutex_unlock(&employee_mutex);

        // 4. Assign unassigned tasks to available employees
        pthread_mutex_lock(&assignment_mutex);
        for (int i = 0; i < assignment_count; i++) {
            if (!task_assignments[i].is_sent && !task_assignments[i].is_completed) {
                // Find an available employee
                pthread_mutex_lock(&employee_mutex);
                for (int j = 0; j < employee_count; j++) {
                    employee_node_t* emp = employees[j];
                    if (emp->sockfd >= 0 && emp->state == EMPLOYEE_STATE_CONFIGURED && emp->active_tasks < 3) {
                        // Assign task to this employee
                        strncpy(task_assignments[i].employee_id, emp->employee_id, sizeof(task_assignments[i].employee_id) - 1);
                        strncpy(task_assignments[i].employee_ip, emp->ip_address, sizeof(task_assignments[i].employee_ip) - 1);
                        task_assignments[i].assigned_time = time(NULL);
                        emp->active_tasks++;
                        printf("[Employer] Task %s assigned to %s\n", task_assignments[i].task_id, emp->ip_address);
                        break;
                    }
                }
                pthread_mutex_unlock(&employee_mutex);
            }
        }
        pthread_mutex_unlock(&assignment_mutex);

        // 5. Manage Ongoing Tasks
        send_pending_tasks();
        handle_task_timeouts();

        // 6. Maintain Employee List
        remove_stale_employees();

        // 7. Report Status
        completed_tasks_count = count_completed_tasks();
        int total_task_count = assignment_count;
        time_t current_time = time(NULL);
        if (current_time - last_status_update >= 10) {
            printf("[Employer] Status: %d employees | %d/%d tasks completed.\n", 
                   employee_count, completed_tasks_count, total_task_count);
            last_status_update = current_time;
        }

        // 8. Check for Completion
        if (total_task_count > 0 && completed_tasks_count >= total_task_count) {
            printf("[Employer] All tasks completed! Shutting down in 10 seconds.\n");
            sleep(10);
            agent_status.is_active = false;
        }
    }

    printf("[Employer] Main loop finished.\n");

    // Clean up: close all persistent connections
    pthread_mutex_lock(&employee_mutex);
    for (int i = 0; i < employee_count; i++) {
        if (employees[i]->sockfd >= 0) {
            close(employees[i]->sockfd);
        }
        free(employees[i]);
    }
    employee_count = 0;
    pthread_mutex_unlock(&employee_mutex);

    close(discovery_sockfd);
    return NULL;
}

// Agent lifecycle
int init_agent(agent_mode_t mode) {

    memset(&agent_status, 0, sizeof(agent_status));
    agent_status.mode = mode;
    
    return 0;
}

void cleanup_agent(void) {

    agent_status.is_active = false;
}

agent_status_t get_agent_status(void) {
    
    return agent_status;
}

// Main employer entry point
int run_employer_mode(char* task_files[]) {
    (void)task_files; // No longer used
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    agent_status.is_active = true;

    pthread_t employer_thread;
    if (pthread_create(&employer_thread, NULL, employer_main_loop, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }

    pthread_join(employer_thread, NULL);

    return 0;
}
