#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h> 
#include "../../volcom_sysinfo/volcom_sysinfo.h"
#include "../include/net_utils.h"
#include "employee_mode.h"
#include "task_receiver.h" 

#define BROADCAST_INTERVAL 5              // seconds
#define RESOURCE_THRESHOLD_PERCENT 80.0   // Max usage before stopping broadcast

static volatile bool keep_running = true;
static pthread_t broadcaster_thread;

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

        // Construct JSON-style string
        char message[1024];
        snprintf(message, sizeof(message),
            "{"
              "\"ip\":\"%s\","
              "\"free_mem_mb\":%lu,"
              "\"mem_usage_percent\":%.2f,"
              "\"cpu_usage_percent\":%.2f,"
              "\"cpu_model\":\"%s\","
              "\"logical_cores\":%d"
            "}",
            ip,
            free_mem_mb,
            mem_percent,
            cpu_percent,
            cpu_info.model,
            cpu_info.logical_processors
        );

        if (mem_percent < RESOURCE_THRESHOLD_PERCENT) {
            send_udp_broadcast(message);
            printf("[Broadcasting] %s\n", message);
        } else {
            printf("[Broadcasting Paused] High memory usage: %.2f%%\n", mem_percent);
        }

        free_cpu_usage(&cpu_info);
        sleep(BROADCAST_INTERVAL);
    }

    return NULL;
}


void run_employee_mode() {
    printf("[Employee] Monitoring system and broadcasting availability...\n");

    signal(SIGINT, sigint_handler);  // Graceful Ctrl+C exit

    // Start broadcaster thread
    if (pthread_create(&broadcaster_thread, NULL, broadcast_loop, NULL) != 0) {
        perror("Failed to create broadcaster thread");
        return;
    }

    printf("[Employee] Ready to accept connection request...\n");
    // TCP connection listener for employer requests
int server_fd = start_tcp_server(12345);
if (server_fd < 0) {
    fprintf(stderr, "[Employee] TCP server failed to start\n");
    return;
}

while (keep_running) {
    int client_fd = accept_tcp_connection(server_fd);
    if (client_fd < 0) continue;

    double mem_percent = get_current_memory_percent();

    if (mem_percent < RESOURCE_THRESHOLD_PERCENT) {
        char accept_msg[] = "ACCEPT";
        send(client_fd, accept_msg, strlen(accept_msg), 0);
        printf("[Employee] Connection Accepted. Stopping broadcast...\n");

        keep_running = false;
        pthread_cancel(broadcaster_thread);
        close(server_fd);  // Only server FD is closed; client remains open

        //  Receive file while keeping client_fd open
        char *received_file = handle_task_receive(client_fd);
        if (!received_file) {
            printf("[Employee] Failed to receive file.\n");
            close(client_fd);
            break;
        }

        // Future logic:
        // - execute_task(received_file)
        // - send_output_to_employer(client_fd, result_path)
        // before sending result file back over client_fd use following signal to alert it about the result
        char *header = "RESULT\n";
        send(client_fd, header, strlen(header), 0);

        // Then send the actual result file content...


        free(received_file);  // Clean up path string
        close(client_fd);
        break;

    } else {
        char reject_msg[] = "REJECT";
        send(client_fd, reject_msg, strlen(reject_msg), 0);
        close(client_fd);
        printf("[Employee] Rejected connection due to high usage.\n");
    }
}


    pthread_join(broadcaster_thread, NULL);
}
