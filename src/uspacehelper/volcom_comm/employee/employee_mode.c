#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "../../volcom_sysinfo/volcom_sysinfo.h"
#include "../include/net_utils.h"
#include "employee_mode.h"

#define BROADCAST_INTERVAL 5              // seconds
#define RESOURCE_THRESHOLD_PERCENT 80.0   // Max usage before stopping broadcast

static volatile bool keep_running = true;
static pthread_t broadcaster_thread;

void sigint_handler(int sig) {
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
    char ip_addr[64];
    get_local_ip(ip_addr, sizeof(ip_addr));

    while (keep_running) {
        double mem_percent = get_current_memory_percent();

        if (mem_percent < RESOURCE_THRESHOLD_PERCENT) {
            char message[256];
            snprintf(message, sizeof(message),
                     "VOLCOM_EMPLOYEE|IP:%s|MEM:%.2f%%", ip_addr, mem_percent);
            send_udp_broadcast(message);
            printf("[Broadcasting] %s\n", message);
        } else {
            printf("[Broadcasting Paused] High memory usage: %.2f%%\n", mem_percent);
        }

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

    // TODO: Implement connection handler loop
    printf("[Employee] Ready to accept connection requests (to be implemented)...\n");
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

        keep_running = false;  // stop broadcast thread
        pthread_cancel(broadcaster_thread);

        close(client_fd);
        close(server_fd);

        // ➕ TODO: Call task receiving handler
        system("./task_receiver"); // temporary stub

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
