#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h> 
#include <netinet/in.h>  // for sockaddr_in, INET_ADDRSTRLEN
#include <arpa/inet.h>   // for inet_ntop
#include "../../volcom_sysinfo/volcom_sysinfo.h"
#include "../include/net_utils.h"
#include "employee_mode.h"
#include "task_receiver.h" 
#include "send_result.h"
#include "task_buffer.h"

#define BROADCAST_INTERVAL 5              // seconds
#define RESOURCE_THRESHOLD_PERCENT 80.0   // Max usage before stopping broadcast


static volatile bool keep_running = true;
static pthread_t broadcaster_thread;
static pthread_t worker_thread;
static TaskBuffer task_buffer;

void sigint_handler(int sig) {
    (void)sig;
    keep_running = false;
    pthread_cancel(broadcaster_thread);
    printf("\n[Employee] Stopping broadcast and exiting...\n");
}

// Get memory usage percent
double get_current_memory_percent() {
    struct memory_info_s mem = get_memory_info();
    return calculate_memory_usage_percent(mem);
}

// Broadcast thread
void* broadcast_loop(void* arg) {
    (void)arg;

    char ip[64] = "Unknown";
    get_local_ip(ip, sizeof(ip));

    while (keep_running) {
        struct memory_info_s mem_info = get_memory_info();
        struct cpu_info_s cpu_info = get_cpu_info();
        cpu_info = get_cpu_usage(cpu_info);

        double mem_percent = calculate_memory_usage_percent(mem_info);
        double cpu_percent = cpu_info.overall_usage.usage_percent;
        unsigned long free_mem_mb = mem_info.free / 1024;

        // Get buffer status
        int buffer_size = task_buffer_size(&task_buffer);
        int buffer_capacity = task_buffer_capacity(&task_buffer);
        double buffer_usage_percent = (double)buffer_size / buffer_capacity * 100.0;

        // Construct JSON-style string with buffer status
        char message[1024];
        snprintf(message, sizeof(message),
            "{"
              "\"ip\":\"%s\","
              "\"free_mem_mb\":%lu,"
              "\"mem_usage_percent\":%.2f,"
              "\"cpu_usage_percent\":%.2f,"
              "\"cpu_model\":\"%s\","
              "\"logical_cores\":%d,"
              "\"buffer_size\":%d,"
              "\"buffer_capacity\":%d,"
              "\"buffer_usage_percent\":%.2f"
            "}",
            ip,
            free_mem_mb,
            mem_percent,
            cpu_percent,
            cpu_info.model,
            cpu_info.logical_processors,
            buffer_size,
            buffer_capacity,
            buffer_usage_percent
        );

        // Only broadcast if resources are available and buffer isn't full
        if (mem_percent < RESOURCE_THRESHOLD_PERCENT && buffer_usage_percent < 90.0) {
            send_udp_broadcast(message);
            printf("[Broadcasting] %s\n", message);
        } else {
            if (mem_percent >= RESOURCE_THRESHOLD_PERCENT) {
                printf("[Broadcasting Paused] High memory usage: %.2f%%\n", mem_percent);
            }
            if (buffer_usage_percent >= 90.0) {
                printf("[Broadcasting Paused] Buffer nearly full: %.2f%%\n", buffer_usage_percent);
            }
        }

        free_cpu_usage(&cpu_info);
        sleep(BROADCAST_INTERVAL);
    }

    return NULL;
}



void* worker_loop(void* arg) {
    (void)arg;
    time_t last_status_report = time(NULL);
    const int STATUS_REPORT_INTERVAL = 10; // seconds
    
    while (keep_running) {
        Task task;
        
        // Check if we should send buffer status
        time_t current_time = time(NULL);
        if (current_time - last_status_report >= STATUS_REPORT_INTERVAL) {
            int buffer_size = task_buffer_size(&task_buffer);
            int buffer_capacity = task_buffer_capacity(&task_buffer);
            
            printf("[Worker] Buffer Status: %d/%d tasks queued\n", buffer_size, buffer_capacity);
            
            // You could send this status to employers or log it
            // For example, broadcast buffer status along with system info
            if (buffer_size > buffer_capacity * 0.8) {
                printf("[Worker] Warning: Buffer is nearly full (%d/%d)\n", buffer_size, buffer_capacity);
            }
            
            last_status_report = current_time;
        }
        
        // Try to dequeue a task (with timeout to allow status checking)
        if (task_buffer_dequeue_timeout(&task_buffer, &task, 1000)) { // 1 second timeout
            printf("[Worker] Processing task: %s from %s\n", task.file_path, task.employer_ip);
            
            // Process the task
            // TODO: Add your actual processing logic here
            // For now, just echo back the file as result
            send_output_to_employer(task.client_fd, task.file_path);
            printf("[Worker] Successfully sent result for task: %s\n", task.file_path);
            
            close(task.client_fd);
            
            // Report buffer status after processing
            int remaining_tasks = task_buffer_size(&task_buffer);
            printf("[Worker] Task completed. Remaining tasks: %d\n", remaining_tasks);
        }
    }
    return NULL;
}

void run_employee_mode() {
    printf("[Employee] Monitoring system and broadcasting availability...\n");

    signal(SIGINT, sigint_handler);  // Graceful Ctrl+C exit

    task_buffer_init(&task_buffer);

    if (pthread_create(&broadcaster_thread, NULL, broadcast_loop, NULL) != 0) {
        perror("Failed to create broadcaster thread");
        return;
    }

    if (pthread_create(&worker_thread, NULL, worker_loop, NULL) != 0) {
        perror("Failed to create worker thread");
        return;
    }

    printf("[Employee] Ready to accept connection requests...\n");

    int server_fd = start_tcp_server(12345);
    if (server_fd < 0) {
        fprintf(stderr, "[Employee] TCP server failed to start\n");
        return;
    }

    while (keep_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) continue;

        // Get employer IP address
        char employer_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, employer_ip, sizeof(employer_ip));
        printf("[Employee] Connection request from %s\n", employer_ip);

        double mem_percent = get_current_memory_percent();
        int buffer_size = task_buffer_size(&task_buffer);
        int buffer_capacity = task_buffer_capacity(&task_buffer);
        double buffer_usage_percent = (double)buffer_size / buffer_capacity * 100.0;

        // Accept connection only if resources and buffer space are available
        if (mem_percent < RESOURCE_THRESHOLD_PERCENT && buffer_usage_percent < 95.0) {
            char accept_msg[256];
            snprintf(accept_msg, sizeof(accept_msg), 
                "ACCEPT:BUFFER_SPACE=%d/%d:MEM_USAGE=%.2f", 
                buffer_size, buffer_capacity, mem_percent);
            send(client_fd, accept_msg, strlen(accept_msg), 0);
            printf("[Employee] Connection Accepted. Buffer: %d/%d, Memory: %.2f%%\n", 
                   buffer_size, buffer_capacity, mem_percent);

            // Receive task (also extracts original filename from header)
            char *received_file = handle_task_receive(client_fd, employer_ip);
            if (!received_file) {
                printf("[Employee] Failed to receive file.\n");
                close(client_fd);
                continue;
            }

            // Prepare task struct
            Task task;
            strncpy(task.file_path, received_file, sizeof(task.file_path)-1);
            task.file_path[sizeof(task.file_path)-1] = '\0';
            strncpy(task.employer_ip, employer_ip, sizeof(task.employer_ip)-1);
            task.employer_ip[sizeof(task.employer_ip)-1] = '\0';
            task.client_fd = client_fd;
            // TODO: Fill task.meta with metadata if needed

            free(received_file);

            // Enqueue task
            if (task_buffer_enqueue(&task_buffer, &task)) {
                printf("[Employee] Task queued successfully. Buffer: %d/%d\n", 
                       task_buffer_size(&task_buffer), buffer_capacity);
            } else {
                printf("[Employee] Failed to queue task - buffer full\n");
                close(client_fd);
            }

        } else {
            char reject_msg[256];
            if (mem_percent >= RESOURCE_THRESHOLD_PERCENT) {
                snprintf(reject_msg, sizeof(reject_msg), 
                    "REJECT:HIGH_MEMORY_USAGE=%.2f%%", mem_percent);
            } else {
                snprintf(reject_msg, sizeof(reject_msg), 
                    "REJECT:BUFFER_FULL=%d/%d", buffer_size, buffer_capacity);
            }
            send(client_fd, reject_msg, strlen(reject_msg), 0);
            close(client_fd);
            printf("[Employee] Rejected connection: %s\n", reject_msg);
        }
    }

    pthread_join(broadcaster_thread, NULL);
    pthread_join(worker_thread, NULL);
}
