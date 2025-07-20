#define _GNU_SOURCE
#include "volcom_agents.h"
#include "../volcom_utils/volcom_utils.h"
#include "../volcom_net/volcom_net.h"
#include "../volcom_sysinfo/volcom_sysinfo.h"
#include "../volcom_rcsmngr/volcom_rcsmngr.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define BROADCAST_INTERVAL 5
#define RESOURCE_THRESHOLD_PERCENT 80.0
#define EMPLOYEE_PORT 12345

int run_node_in_cgroup(struct volcom_rcsmngr_s *manager, const char *task_name, const char *script_path);

// Forward declarations
static int send_result_to_employer(int sockfd, const result_info_t* result);
static int receive_task_from_employer(int sockfd, received_task_t* task);

// Global state for employee mode
static bool employee_running = false;
static pthread_t broadcaster_thread;
static pthread_t worker_thread;
static task_buffer_t task_buffer;
static result_queue_t result_queue; // Global result queue
static agent_status_t employee_status;
static bool is_node_started = false;

// Forward declaration for the function to be imported
// extern int start_node(const char* config_path);

// TODO: Move
// Signal handler
static void employee_signal_handler(int sig) {
    (void)sig;
    employee_running = false;
    printf("\n[Employee] Stopping broadcast and exiting...\n");
}

// Get memory usage percent
static double get_current_memory_percent(void) {
    struct memory_info_s mem = get_memory_info();
    return calculate_memory_usage_percent(mem);
}

// Broadcast thread
static void* broadcast_loop(void* arg) {
    (void)arg;

    char ip[64] = "Unknown";
    get_local_ip(ip, sizeof(ip));

    while (employee_running) {
        struct memory_info_s mem_info = get_memory_info();
        struct cpu_info_s cpu_info = get_cpu_info();
        cpu_info = get_cpu_usage(cpu_info);

        double mem_percent = calculate_memory_usage_percent(mem_info);
        double cpu_percent = cpu_info.overall_usage.usage_percent;
        unsigned long free_mem_mb = mem_info.free / 1024;

        // Construct JSON broadcast message
        char message[1024];
        snprintf(message, sizeof(message),
            "{"
              "\"type\":\"volcom_broadcast\","
              "\"mode\":\"employee\","
              "\"employee_id\":\"%s\","
              "\"ip\":\"%s\","
              "\"free_mem_mb\":%lu,"
              "\"memory_percent\":%.2f,"
              "\"cpu_percent\":%.2f,"
              "\"cpu_model\":\"%s\","
              "\"logical_cores\":%d,"
              "\"timestamp\":%ld"
            "}",
            employee_status.agent_id,
            ip,
            free_mem_mb,
            mem_percent,
            cpu_percent,
            cpu_info.model,
            cpu_info.logical_processors,
            time(NULL)
        );

        if (mem_percent < RESOURCE_THRESHOLD_PERCENT) {
            send_udp_broadcast(message);
            printf("[Employee] Broadcasting: Memory %.2f%%, CPU %.2f%%\n", 
                   mem_percent, cpu_percent);
        } else {
            printf("[Employee] Broadcasting paused - high memory usage: %.2f%%\n", mem_percent);
        }

        free_cpu_usage(&cpu_info);
        sleep(BROADCAST_INTERVAL);
    }

    return NULL;
}

// ###########################################################################

// ###########################################################################

// Worker thread to process tasks
static void* worker_loop(void* arg) {
    (void)arg;
    
    while (employee_running) {

        if (!is_node_started) {
            usleep(100000); // Wait for the node to be started
            continue;
        }

        received_task_t task;
        
        if (get_task_from_buffer(&task_buffer, &task) == 0) {
            printf("[Employee] Processing task: %s\n", task.task_id);
            
            // Simulate task processing (replace with actual processing logic)
            printf("[Employee] Task %s: Processing file %s...\n", task.task_id, task.chunk_filename);
            
            // Simulate processing time based on file size
            int processing_time = (task.data_size / 1024) + 2; // 2 seconds + 1 sec per KB
            if (processing_time > 30) processing_time = 30; // Max 30 seconds
            
            for (int i = 0; i < processing_time && employee_running; i++) {
                printf("[Employee] Task %s: Processing... %d/%d seconds\n", 
                       task.task_id, i + 1, processing_time);
                sleep(1);
            }
            
            if (!employee_running) {
                printf("[Employee] Task %s interrupted due to shutdown\n", task.task_id);
                break;
            }
            
            // Mark task as processed
            task.is_processed = true;
            
            // Create result file (for demonstration)
            char result_filename[512];
            snprintf(result_filename, sizeof(result_filename), "/tmp/result_%s.txt", task.task_id);
            
            FILE *result_file = fopen(result_filename, "w");
            if (result_file) {
                fprintf(result_file, "Task ID: %s\n", task.task_id);
                fprintf(result_file, "Original file: %s\n", task.chunk_filename);
                fprintf(result_file, "Processing time: %d seconds\n", processing_time);
                fprintf(result_file, "File size: %zu bytes\n", task.data_size);
                fprintf(result_file, "Processed by: %s\n", employee_status.agent_id);
                fprintf(result_file, "Completed at: %ld\n", time(NULL));
                
                // Echo back the original file content (as an example)
                if (task.data) {
                    fprintf(result_file, "\n--- Original Content ---\n");
                    fwrite(task.data, 1, task.data_size, result_file);
                    fprintf(result_file, "\n--- End Original Content ---\n");
                }
                
                fclose(result_file);
                printf("[Employee] Result saved to: %s\n", result_filename);

                // Add result to the queue for sending
                result_info_t result_info;
                strncpy(result_info.task_id, task.task_id, sizeof(result_info.task_id) - 1);
                strncpy(result_info.result_filepath, result_filename, sizeof(result_info.result_filepath) - 1);
                strncpy(result_info.employer_ip, task.sender_id, sizeof(result_info.employer_ip) - 1); // Assuming sender_id is the employer's IP
                
                if (add_result_to_queue(&result_queue, &result_info) == 0) {
                    printf("[Employee] Result for task %s queued for sending.\n", task.task_id);
                } else {
                    printf("[Employee] Failed to queue result for task %s.\n", task.task_id);
                }
            }
            
            // Clean up task data
            if (task.data) {
                free(task.data);
            }
            
        } else {
            // No tasks available, sleep briefly
            usleep(100000); // 100ms
        }
    }
    
    printf("[Employee] Worker thread stopped\n");
    return NULL;
}

// Send result back to employer using the persistent connection
static int send_result_to_employer(int sockfd, const result_info_t* result) {
    if (sockfd < 0 || !result) {
        return -1;
    }
    
    printf("[Employee] Sending result for task %s back to employer\n", result->task_id);
    
    // 1. Send result metadata as JSON
    cJSON *metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(metadata, "type", "task_result");
    cJSON_AddStringToObject(metadata, "task_id", result->task_id);
    
    if (send_json(sockfd, metadata) != PROTOCOL_OK) {
        printf("[Employee] Failed to send result metadata for task %s\n", result->task_id);
        cJSON_Delete(metadata);
        return -1;
    }
    cJSON_Delete(metadata);
    
    // 2. Send result file content
    FILE *file = fopen(result->result_filepath, "rb");
    if (!file) {
        printf("[Employee] Failed to open result file %s\n", result->result_filepath);
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Send file size first
    uint32_t net_size = htonl((uint32_t)file_size);
    if (send(sockfd, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
        printf("[Employee] Failed to send result file size for task %s\n", result->task_id);
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
            printf("[Employee] Failed to send result file data for %s. Connection may be lost.\n", result->task_id);
            fclose(file);
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    fclose(file);
    
    printf("[Employee] Result transmission completed for task %s\n", result->task_id);
    return 0;
}

// Handle incoming connections and task requests
static void handle_persistent_connection(int employer_fd, struct volcom_rcsmngr_s *manager) {
    
    printf("[Employee] Now in persistent communication mode with employer.\n");

    is_node_started = false;

    while (employee_running) {
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(employer_fd, &readfds);
        
        // We also need to check for results to send, so a short timeout is fine.
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(employer_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            perror("[Employee] Select error");
            break;
        }
        
        // 1. Check for incoming data from the employer
        if (activity > 0 && FD_ISSET(employer_fd, &readfds)) {
            cJSON* initial_check = NULL;
            //memset(&task, 0, sizeof(task));

            if(recv_json_peek(employer_fd, &initial_check) != PROTOCOL_OK) {
                printf("[Employee] Connection closed by employer.\n");
                break;
            }

            const cJSON* msg_type_item = cJSON_GetObjectItem(initial_check, "message_type");

            if (!msg_type_item || !cJSON_IsString(msg_type_item)) {
                printf("[Employee] Invalid message format: missing 'message_type'.\n");
                cJSON_Delete(initial_check);
                break; 
            }

            char* msg_type = msg_type_item->valuestring;

            if (strcmp(msg_type, "initial_config") == 0) {
                printf("[Employee] Receiving initial configuration...\n");

                received_task_t config_task;
                memset(&config_task, 0, sizeof(config_task));

                if (receive_task_from_employer(employer_fd, &config_task) == 0) {
                    // Save the config file and start the node
                    char config_filepath[512];
                    snprintf(config_filepath, sizeof(config_filepath), "/tmp/config_%s.js", config_task.task_id);
                    FILE* f = fopen(config_filepath, "wb");

                    if(f) {
                        fwrite(config_task.data, 1, config_task.data_size, f);
                        fclose(f);
                        printf("[Employee] Configuration saved to %s\n", config_filepath);

                        // Call the external start_node function
                        // TODO: 
                        if (run_node_in_cgroup(manager, config_task.task_id, config_filepath) == 0) {
                            printf("[Employee] Node started successfully.\n");
                            is_node_started = true;
                        } else {
                            fprintf(stderr, "[Employee] ERROR: Failed to start the node.\n");
                        }
                    } else {
                        perror("[Employee] Failed to save config file");
                    }

                    if(config_task.data) free(config_task.data);
                } else {
                    fprintf(stderr, "[Employee] Failed to receive initial configuration.\n");
                }
            } else if (strcmp(msg_type, "data_chunk") == 0) {
                if (!is_node_started) {
                    // TODO: Why ??? cann't we use buffer
                    printf("[Employee] Warning: Received data chunk before node was configured. Ignoring.\n");
                    // Discard the message fully
                    received_task_t temp_task;
                    receive_task_from_employer(employer_fd, &temp_task);
                    if(temp_task.data) free(temp_task.data);
                } else {
                    received_task_t task;
                    memset(&task, 0, sizeof(task));
                    if (receive_task_from_employer(employer_fd, &task) == 0) {
                        if (add_task_to_buffer(&task_buffer, &task) == 0) {
                            printf("[Employee] Data chunk %s successfully received and queued\n", task.task_id);
                        } else {
                            printf("[Employee] Task buffer full, rejecting chunk %s\n", task.task_id);
                            if (task.data) free(task.data);
                        }
                    } else {
                        printf("[Employee] Failed to receive data chunk or connection closed.\n");
                        break; // Assume connection is lost
                    }
                }
            } else {
                printf("[Employee] Unknown message type received: %s\n", msg_type);
                // Discard message
                cJSON* temp_json = NULL;
                recv_json(employer_fd, &temp_json);
                cJSON_Delete(temp_json);
            }

            cJSON_Delete(initial_check);
        }

        // 2. Check for and send any completed task results
        if (!is_result_queue_empty(&result_queue)) {
            result_info_t result_to_send;
            if (get_result_from_queue(&result_queue, &result_to_send) == 0) {
                printf("[Employee] Dequeued result for task %s to send.\n", result_to_send.task_id);
                if (send_result_to_employer(employer_fd, &result_to_send) == 0) {
                    printf("[Employee] Successfully sent result for task %s.\n", result_to_send.task_id);
                    employee_status.tasks_completed++;
                } else {
                    printf("[Employee] Failed to send result for task %s. Re-queueing.\n", result_to_send.task_id);
                    add_result_to_queue(&result_queue, &result_to_send); // Re-add to queue for retry
                    employee_status.tasks_failed++;
                }
            }
        }
    }

    printf("[Employee] Connection with employer lost. Returning to listening mode.\n");
    close(employer_fd);
}

// Proper task reception from employer
static int receive_task_from_employer(int sockfd, received_task_t* task) {
    if (!task) return -1;
    
    // Receive task metadata (JSON)
    cJSON *metadata = NULL;
    if (recv_json(sockfd, &metadata) != PROTOCOL_OK) {
        printf("[Employee] Failed to receive task metadata\n");
        return -1;
    }
    
    // Parse metadata
    const cJSON *task_id = cJSON_GetObjectItem(metadata, "task_id");
    const cJSON *chunk_filename = cJSON_GetObjectItem(metadata, "chunk_filename");
    const cJSON *sender_id = cJSON_GetObjectItem(metadata, "sender_id");
    const cJSON *message_type = cJSON_GetObjectItem(metadata, "message_type");
    
    if (!task_id || !cJSON_IsString(task_id) ||
        !chunk_filename || !cJSON_IsString(chunk_filename) ||
        !sender_id || !cJSON_IsString(sender_id) ||
        !message_type || !cJSON_IsString(message_type)) {
        printf("[Employee] Invalid task metadata\n");
        cJSON_Delete(metadata);
        return -1;
    }
    
    // Copy metadata to task structure
    strncpy(task->task_id, task_id->valuestring, sizeof(task->task_id) - 1);
    strncpy(task->chunk_filename, chunk_filename->valuestring, sizeof(task->chunk_filename) - 1);
    strncpy(task->sender_id, sender_id->valuestring, sizeof(task->sender_id) - 1);
    task->received_time = time(NULL);
    task->is_processed = false;
    
    printf("[Employee] Receiving task: %s, file: %s\n", task->task_id, task->chunk_filename);
    
    cJSON_Delete(metadata);
    
    // Receive file size
    uint32_t net_size;
    if (recv(sockfd, &net_size, sizeof(net_size), MSG_WAITALL) != sizeof(net_size)) {
        printf("[Employee] Failed to receive file size\n");
        return -1;
    }
    
    uint32_t file_size = ntohl(net_size);
    printf("[Employee] Expecting file of size: %u bytes\n", file_size);
    
    if (file_size == 0 || file_size > 100 * 1024 * 1024) { // Max 100MB
        printf("[Employee] Invalid file size: %u\n", file_size);
        return -1;
    }
    
    // Allocate memory for file data
    task->data = malloc(file_size);
    if (!task->data) {
        printf("[Employee] Failed to allocate memory for task data\n");
        return -1;
    }
    task->data_size = file_size;
    
    // Receive file content
    size_t total_received = 0;
    char *data_ptr = (char*)task->data;
    
    while (total_received < file_size) {
        size_t remaining = file_size - total_received;
        size_t to_receive = remaining < 4096 ? remaining : 4096;
        
        ssize_t bytes_received = recv(sockfd, data_ptr + total_received, to_receive, 0);
        if (bytes_received <= 0) {
            printf("[Employee] Failed to receive file data (received %zu/%u bytes)\n", 
                   total_received, file_size);
            free(task->data);
            task->data = NULL;
            return -1;
        }
        
        total_received += bytes_received;
    }
    
    printf("[Employee] Successfully received task file: %s (%u bytes)\n", 
           task->task_id, file_size);
    
    // Save file to local storage
    char local_filename[512];
    snprintf(local_filename, sizeof(local_filename), "/tmp/employee_task_%s", task->task_id);
    
    FILE *local_file = fopen(local_filename, "wb");
    if (local_file) {
        fwrite(task->data, 1, file_size, local_file);
        fclose(local_file);
        printf("[Employee] Task file saved as: %s\n", local_filename);
        
        // Update chunk_filename to point to local file
        strncpy(task->chunk_filename, local_filename, sizeof(task->chunk_filename) - 1);
    } else {
        printf("[Employee] Warning: Could not save task file locally\n");
    }
    
    return 0;
}

// Employee mode main function
int run_employee_mode(struct volcom_rcsmngr_s *manager) {
    printf("[Employee] Starting Employee Mode...\n");

    if (init_agent(AGENT_MODE_EMPLOYEE) != 0) {
        fprintf(stderr, "Failed to initialize employee agent\n");
        return -1;
    }

    // Initialize task buffer
    if (init_task_buffer(&task_buffer, 10) != 0) {
        fprintf(stderr, "Failed to initialize task buffer\n");
        return -1;
    }

    // Initialize result queue
    if (init_result_queue(&result_queue, 10) != 0) {
        fprintf(stderr, "Failed to initialize result queue\n");
        cleanup_task_buffer(&task_buffer);
        return -1;
    }

    // Initialize default config
    struct config_s config = {0};
    config.mem_config.allocated_memory_size_max = 1024 * 1024 * 1024;  // 1GB default
    config.mem_config.allocated_memory_size_high = 800 * 1024 * 1024;  // 800MB high
    config.cpu_config.allocated_logical_processors = 2;  // 2 cores default
    config.cpu_config.allocated_cpu_share = 80;  // 80% default

    if (volcom_rcsmngr_init(manager, "volcom") != 0) {
        fprintf(stderr, "Failed to initialize resource manager\n");
        return 1;
    }

    if (volcom_create_main_cgroup(manager, config) != 0) {
        fprintf(stderr, "Failed to create main cgroup\n");
        volcom_rcsmngr_cleanup(manager);
        return 1;
    }

    volcom_print_cgroup_info(manager);

    signal(SIGINT, employee_signal_handler);
    signal(SIGTERM, employee_signal_handler);

    employee_running = true;
    employee_status = get_agent_status();
    employee_status.is_active = true;

    // Start broadcaster thread
    if (pthread_create(&broadcaster_thread, NULL, broadcast_loop, NULL) != 0) {
        perror("Failed to create broadcaster thread");
        employee_running = false;
        cleanup_task_buffer(&task_buffer);
        return -1;
    }

    // Start worker thread
    if (pthread_create(&worker_thread, NULL, worker_loop, NULL) != 0) {
        perror("Failed to create worker thread");
        employee_running = false;
        pthread_cancel(broadcaster_thread);
        cleanup_task_buffer(&task_buffer);
        return -1;
    }

    printf("[Employee] Ready to accept task requests on port %d...\n", EMPLOYEE_PORT);

    // Start TCP server for receiving tasks
    int server_fd = start_tcp_server(EMPLOYEE_PORT);
    if (server_fd < 0) {
        fprintf(stderr, "[Employee] Failed to start TCP server\n");
        employee_running = false;
        pthread_cancel(broadcaster_thread);
        pthread_cancel(worker_thread);
        cleanup_task_buffer(&task_buffer);
        return -1;
    }

    // Main loop to accept connections
    while (employee_running) {
        printf("[Employee] Waiting for an employer to connect on port %d...\n", EMPLOYEE_PORT);
        
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int employer_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        
        if (employer_fd < 0) {
            if (errno != EINTR) {
                perror("[Employee] Accept failed");
            }
            continue;
        }

        char employer_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, employer_ip, sizeof(employer_ip));
        printf("[Employee] Connection accepted from employer at %s\n", employer_ip);

        double mem_percent = get_current_memory_percent();

        if (mem_percent < RESOURCE_THRESHOLD_PERCENT) {
            // Enter persistent communication loop
            handle_persistent_connection(employer_fd, manager);
        } else {
            const char reject_msg[] = "REJECT:HIGH_RESOURCE_USAGE";
            send(employer_fd, reject_msg, strlen(reject_msg), 0);
            printf("[Employee] Connection rejected - high resource usage: %.2f%%\n", mem_percent);
            close(employer_fd);
        }
    }

    // Cleanup
    close(server_fd);
    pthread_cancel(broadcaster_thread);
    pthread_cancel(worker_thread);
    
    pthread_join(broadcaster_thread, NULL);
    pthread_join(worker_thread, NULL);

    cleanup_task_buffer(&task_buffer);
    cleanup_result_queue(&result_queue);

    printf("[Employee] Employee mode stopped\n");
    return 0;
}

// Employee-specific function implementations
int start_resource_broadcasting(void) {
    if (!employee_running) {
        return pthread_create(&broadcaster_thread, NULL, broadcast_loop, NULL);
    }
    return 0; // Already running
}

int stop_resource_broadcasting(void) {
    if (employee_running) {
        pthread_cancel(broadcaster_thread);
        return 0;
    }
    return -1;
}

int listen_for_tasks(void) {
    // This functionality is integrated into the main employee loop
    return 0;
}

int process_received_task(const received_task_t* task) {
    if (!task) return -1;
    
    printf("[Employee] Processing task %s\n", task->task_id);
    
    // Placeholder processing logic
    // In a real implementation, this would:
    // 1. Execute the task
    // 2. Generate results
    // 3. Prepare results for sending back
    
    return 0;
}

int send_task_result(const char* task_id, const char* result_file) {
    if (!task_id || !result_file) return -1;
    
    printf("[Employee] Sending result for task %s from file %s\n", task_id, result_file);
    
    // Placeholder implementation
    // In practice, this would connect to the employer and send the result
    
    return 0;
}

// Hybrid mode placeholder
int run_hybrid_mode(void) {
    printf("Hybrid mode not yet implemented\n");
    return -1;
}

int run_node_in_cgroup(struct volcom_rcsmngr_s *manager, const char *task_name, const char *script_path) {

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return -1;
    }

    if (pid == 0) {
        // Child process - execute Node.js script with data point as argument
        printf("Child process %d starting Node.js task: %s \n", getpid(), task_name);
        execlp("node", "node", script_path);
        perror("execlp failed - Node.js not found or script error");
        exit(1);
    } else {
        // Parent process - add child to cgroup
        printf("Created child process %d for task '%s'\n", pid, task_name);
        
        // Add the process to the main cgroup
        if (volcom_add_pid_to_cgroup(manager, manager->main_cgroup.name, pid) == 0) {
            printf("Added process %d to main cgroup '%s'\n", pid, manager->main_cgroup.name);
        } else {
            printf("Failed to add process %d to cgroup\n", pid);
        }

        // Wait for the child process to complete
        int status;
        printf("Waiting for Node.js task to complete...\n");
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("Task '%s' completed with exit code %d\n", task_name, exit_code);
            return exit_code;
        } else if (WIFSIGNALED(status)) {
            printf("Task '%s' terminated by signal %d\n", task_name, WTERMSIG(status));
            return -1;
        }
        
        return 0;
    }
}
