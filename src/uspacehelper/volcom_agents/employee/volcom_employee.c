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
#include <cjson/cJSON.h>

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
static result_queue_t result_queue; // Global result queue
static agent_status_t employee_status;
static bool is_node_started = false;
static bool unix_socket_connected = false;

// Buffer for data chunks
static task_buffer_t data_chunk_buffer; // Buffer for incoming data chunks

// TODO: Move to a config file
struct unix_socket_config_s client_socket_config = {
    .socket_path = "/tmp/volcom_unix_socket",
    .buffer_size = 1024,
    .timeout_sec = 5
};

// ============================================================================
// SIGNAL HANDLING & UTILITY FUNCTIONS
// ============================================================================
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

// ============================================================================
// CORE WORKER THREAD - PROCESSES DATA CHUNKS VIA UNIX SOCKET
// ============================================================================

// Worker thread to process data chunks and communicate with node script
static void* worker_loop(void* arg) {
    (void)arg;
    
    while (employee_running) {
        // Send buffered data chunks to node script when ready
        if (is_node_started && unix_socket_connected) {
            received_task_t data_chunk;
            
            // Check for buffered data chunks to send to node
            if (get_task_from_buffer(&data_chunk_buffer, &data_chunk) == 0) {
                printf("[Employee] Sending data chunk %s to node script via Unix socket\n", data_chunk.task_id);
                
                // Send the actual file data to the node script
                if (unix_socket_client_send_message((char*)data_chunk.data)) {
                    printf("[Employee] Data chunk file content sent to node script\n");
                    
                    // Wait for response from node script
                    char response[4096];
                    ssize_t bytes = unix_socket_client_receive_message(response, sizeof(response));
                    if (bytes > 0) {
                        printf("[Employee] Node script response: %s\n", response);
                        
                        // Parse response and add to result queue
                        cJSON *response_json = cJSON_Parse(response);
                        if (response_json) {
                            const cJSON *status = cJSON_GetObjectItem(response_json, "status");
                            
                            if (status && cJSON_IsString(status)) {
                                // Create result info
                                result_info_t result_info;
                                strncpy(result_info.task_id, data_chunk.task_id, sizeof(result_info.task_id) - 1);
                                strncpy(result_info.employer_ip, data_chunk.sender_id, sizeof(result_info.employer_ip) - 1);
                                
                                // Create result file
                                char result_filename[512];
                                snprintf(result_filename, sizeof(result_filename), "/tmp/node_result_%s.json", data_chunk.task_id);
                                
                                FILE *result_file = fopen(result_filename, "w");
                                if (result_file) {
                                    fprintf(result_file, "%s", response);
                                    fclose(result_file);
                                    
                                    strncpy(result_info.result_filepath, result_filename, sizeof(result_info.result_filepath) - 1);
                                    
                                    // Add to result queue for sending back to employer
                                    if (add_result_to_queue(&result_queue, &result_info) == 0) {
                                        printf("[Employee] Node script result for task %s queued for sending to employer\n", data_chunk.task_id);
                                    } else {
                                        printf("[Employee] Failed to queue node script result for task %s\n", data_chunk.task_id);
                                    }
                                }
                            }
                            cJSON_Delete(response_json);
                        }
                    } else {
                        printf("[Employee] Failed to receive response from node script\n");
                    }
                } else {
                    printf("[Employee] Failed to send data chunk to node script, re-queuing\n");
                    // Re-add to buffer for retry
                    add_task_to_buffer(&data_chunk_buffer, &data_chunk);
                }
                
                // Clean up data chunk
                if (data_chunk.data) {
                    free(data_chunk.data);
                }
            }
        }
        
        // Sleep briefly if no tasks to process
        if (!is_node_started || !unix_socket_connected || is_task_buffer_empty(&data_chunk_buffer)) {
            usleep(100000); // 100ms
        }
    }
    
    printf("[Employee] Worker thread stopped\n");
    return NULL;
}

// ============================================================================
// RESULT TRANSMISSION TO EMPLOYER
// ============================================================================
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

// ============================================================================
// THREAD FUNCTIONS FOR FILE OPERATIONS AND NODE MANAGEMENT
// ============================================================================

// Helper structs for threads
typedef struct {
    char filepath[512];
    void *data;
    size_t data_size;
} file_save_args_t;

typedef struct {
    struct volcom_rcsmngr_s *manager;
    char task_id[128];
    char config_filepath[512];
} node_start_args_t;

// Thread function to save a file
void* save_file_thread(void* arg) {
    file_save_args_t *args = (file_save_args_t*)arg;
    FILE* f = fopen(args->filepath, "wb");
    if (f) {
        fwrite(args->data, 1, args->data_size, f);
        fclose(f);
        printf("[Employee] File saved to %s\n", args->filepath);
    } else {
        perror("[Employee] Failed to save file");
    }
    free(args->data);
    free(args);
    return NULL;
}

// Thread function to start node
void* start_node_thread(void* arg) {
    node_start_args_t *args = (node_start_args_t*)arg;
    
    printf("[Employee] Starting node in thread...\n");
    
    if (run_node_in_cgroup(args->manager, args->task_id, args->config_filepath) == 0) {
        printf("[Employee] Node process started successfully.\n");
        is_node_started = true;
        
        // Initialize Unix socket client
        printf("[Employee] Initializing Unix socket client...\n");
        if (!unix_socket_client_init(&client_socket_config)) {
            fprintf(stderr, "[Employee] Failed to initialize Unix socket client\n");
            free(args);
            return NULL;
        }
        
        // Wait a bit for the node script to set up the socket server
        printf("[Employee] Waiting for Node.js script to initialize socket server...\n");
        sleep(3);
        
        // Try to connect to Unix socket after node starts
        printf("[Employee] Attempting to connect to Unix socket...\n");
        int retry_count = 0;
        const int max_retries = 15;
        
        while (retry_count < max_retries && !unix_socket_connected && employee_running) {
            if (unix_socket_client_connect()) {
                unix_socket_connected = true;
                printf("[Employee] Connected to Unix socket server successfully!\n");
                break;
            } else {
                printf("[Employee] Waiting for Unix socket connection... (attempt %d/%d)\n", 
                       retry_count + 1, max_retries);
                sleep(1);
                retry_count++;
            }
        }
        
        if (!unix_socket_connected) {
            fprintf(stderr, "[Employee] Failed to connect to Unix socket after %d attempts\n", max_retries);
        }
    } else {
        fprintf(stderr, "[Employee] ERROR: Failed to start the node.\n");
    }
    
    free(args);
    return NULL;
}

// ============================================================================
// COMMUNICATION HANDLING WITH EMPLOYER
// ============================================================================
static void handle_persistent_connection(int employer_fd, struct volcom_rcsmngr_s *manager) {
    printf("[Employee] Now in persistent communication mode with employer.\n");
    is_node_started = false;
    while (employee_running) {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(employer_fd, &readfds);
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
                    char config_filepath[512];
                    snprintf(config_filepath, sizeof(config_filepath), "/tmp/config_%s.js", config_task.task_id);
                    // Save config in a thread
                    file_save_args_t *save_args = malloc(sizeof(file_save_args_t));
                    strcpy(save_args->filepath, config_filepath);
                    save_args->data = config_task.data;
                    save_args->data_size = config_task.data_size;
                    pthread_t save_thread;
                    pthread_create(&save_thread, NULL, save_file_thread, save_args);
                    pthread_detach(save_thread);
                    // Start node in a thread
                    node_start_args_t *node_args = malloc(sizeof(node_start_args_t));
                    node_args->manager = manager;
                    strcpy(node_args->task_id, config_task.task_id);
                    strcpy(node_args->config_filepath, config_filepath);
                    pthread_t node_thread;
                    pthread_create(&node_thread, NULL, start_node_thread, node_args);
                    pthread_detach(node_thread);
                    // Do not free config_task.data here, handled by thread
                } else {
                    fprintf(stderr, "[Employee] Failed to receive initial configuration.\n");
                }
            } else if (strcmp(msg_type, "data_chunk") == 0) {
                printf("[Employee] Receiving data chunk...\n");
                received_task_t data_chunk;
                memset(&data_chunk, 0, sizeof(data_chunk));
                
                if (receive_task_from_employer(employer_fd, &data_chunk) == 0) {
                    if (!is_node_started || !unix_socket_connected) {
                        printf("[Employee] Node not ready yet, buffering data chunk %s\n", data_chunk.task_id);
                        // Add to data chunk buffer to wait for node to be ready
                        if (add_task_to_buffer(&data_chunk_buffer, &data_chunk) == 0) {
                            printf("[Employee] Data chunk %s buffered successfully\n", data_chunk.task_id);
                        } else {
                            printf("[Employee] Failed to buffer data chunk %s\n", data_chunk.task_id);
                            if (data_chunk.data) free(data_chunk.data);
                        }
                    } else {
                        printf("[Employee] Node is ready, adding data chunk %s to processing queue\n", data_chunk.task_id);
                        // Node is ready, add directly to processing buffer
                        if (add_task_to_buffer(&data_chunk_buffer, &data_chunk) == 0) {
                            printf("[Employee] Data chunk %s added to processing queue\n", data_chunk.task_id);
                        } else {
                            printf("[Employee] Failed to add data chunk %s to processing queue\n", data_chunk.task_id);
                            if (data_chunk.data) free(data_chunk.data);
                        }
                    }
                } else {
                    printf("[Employee] Failed to receive data chunk or connection closed.\n");
                    break;
                }
            } else {
                printf("[Employee] Unknown message type received: %s\n", msg_type);
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
                    add_result_to_queue(&result_queue, &result_to_send);
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

// ============================================================================
// MAIN EMPLOYEE MODE FUNCTION
// ============================================================================
int run_employee_mode(struct volcom_rcsmngr_s *manager) {

    printf("[Employee] Starting Employee Mode...\n");

    if (init_agent(AGENT_MODE_EMPLOYEE) != 0) {
        fprintf(stderr, "Failed to initialize employee agent\n");
        return -1;
    }

    // Initialize data chunk buffer
    if (init_task_buffer(&data_chunk_buffer, 50) != 0) {
        fprintf(stderr, "Failed to initialize data chunk buffer\n");
        return -1;
    }

    // Initialize result queue
    if (init_result_queue(&result_queue, 10) != 0) {
        fprintf(stderr, "Failed to initialize result queue\n");
        cleanup_task_buffer(&data_chunk_buffer);
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
        cleanup_task_buffer(&data_chunk_buffer);
        return -1;
    }

    // Start worker thread
    if (pthread_create(&worker_thread, NULL, worker_loop, NULL) != 0) {
        perror("Failed to create worker thread");
        employee_running = false;
        pthread_cancel(broadcaster_thread);
        cleanup_task_buffer(&data_chunk_buffer);
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
        cleanup_task_buffer(&data_chunk_buffer);
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

    // Cleanup Unix socket connection
    if (unix_socket_connected) {
        unix_socket_client_cleanup();
        unix_socket_connected = false;
    }

    cleanup_task_buffer(&data_chunk_buffer);
    cleanup_result_queue(&result_queue);

    printf("[Employee] Employee mode stopped\n");
    return 0;
}

// ============================================================================
// CGROUP MANAGEMENT FOR NODE PROCESSES
// ============================================================================
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

        printf("[Employee] Node.js process started, process will run in background\n");
        
        // Don't wait for the child process here - it should run in background
        // The Unix socket connection will be handled by start_node_thread
        // The process will be monitored separately
        
        return 0; // Return success immediately
    }
}
