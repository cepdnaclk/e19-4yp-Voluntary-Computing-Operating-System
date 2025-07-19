#define _GNU_SOURCE
#include "volcom_utils.h"
#include "../volcom_sysinfo/volcom_sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/types.h>

#define BROADCAST_PORT 9876
#define TCP_PORT 12345

// Global configuration storage
static char config_values[100][2][256]; // Simple key-value storage
static int config_count = 0;

// System initialization
int init_volcom_utils(void) {
    printf("Initializing Voluntary Computing utilities...\n");
    // Initialize any required resources
    config_count = 0;
    return 0;
}

void cleanup_volcom_utils(void) {
    printf("Cleaning up utilities...\n");
    // Cleanup any allocated resources
    config_count = 0;
}

// System information display
void show_system_info(void) {
    printf("\n=== System Information ===\n");
    
    struct memory_info_s mem_info = get_memory_info();
    struct cpu_info_s cpu_info = get_cpu_info();
    cpu_info = get_cpu_usage(cpu_info);
    
    printf("Memory Usage: %.2f%%\n", calculate_memory_usage_percent(mem_info));
    printf("Total Memory: %lu KB\n", mem_info.total);
    printf("Free Memory: %lu KB\n", mem_info.free);
    printf("CPU Count: %d\n", cpu_info.cpu_count);
    printf("Logical Processors: %d\n", cpu_info.logical_processors);
    printf("CPU Model: %s\n", cpu_info.model);
    printf("CPU Usage: %.2f%%\n", cpu_info.overall_usage.usage_percent);
    
    char ip[64] = "Unknown";
    get_local_ip(ip, sizeof(ip));
    printf("Local IP: %s\n", ip);
    printf("==========================\n");
    
    // Cleanup CPU info
    free_cpu_usage(&cpu_info);
}

// Network utilities (moved from net_utils.c)
// void send_udp_broadcast(const char* message) {
//     int sockfd;
//     struct sockaddr_in addr;
//     int broadcast = 1;
//
//     if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
//         perror("socket failed");
//         return;
//     }
//
//     if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
//         perror("setsockopt failed");
//         close(sockfd);
//         return;
//     }
//
//     memset(&addr, 0, sizeof(addr));
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(BROADCAST_PORT);
//     addr.sin_addr.s_addr = inet_addr("255.255.255.255");
//
//     sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&addr, sizeof(addr));
//     close(sockfd);
// }

void get_local_ip(char* buffer, size_t size) {
    struct ifaddrs *ifaddr, *ifa;
    getifaddrs(&ifaddr);

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr &&
            ifa->ifa_addr->sa_family == AF_INET &&
            !(ifa->ifa_flags & IFF_LOOPBACK)) {
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), buffer, size, NULL, 0, NI_NUMERICHOST);
            break;
        }
    }

    freeifaddrs(ifaddr);
}

int start_tcp_server(int port) {
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP socket failed");
        return -1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("TCP bind failed");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 5) < 0) {
        perror("TCP listen failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int accept_tcp_connection(int server_fd) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    return accept(server_fd, (struct sockaddr*)&client, &len);
}

// Configuration utilities
int load_volcom_config(const char* config_file) {
    FILE* file = fopen(config_file, "r");
    if (!file) {
        printf("Warning: Could not load config file %s, using defaults\n", config_file);
        return -1;
    }
    
    char line[512];
    config_count = 0;
    
    while (fgets(line, sizeof(line), file) && config_count < 100) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        
        if (key && value) {
            strncpy(config_values[config_count][0], key, 255);
            strncpy(config_values[config_count][1], value, 255);
            config_count++;
        }
    }
    
    fclose(file);
    return 0;
}

const char* get_volcom_config_value(const char* key) {
    for (int i = 0; i < config_count; i++) {
        if (strcmp(config_values[i][0], key) == 0) {
            return config_values[i][1];
        }
    }
    return NULL;
}

void set_volcom_config_value(const char* key, const char* value) {
    // Update existing or add new
    for (int i = 0; i < config_count; i++) {
        if (strcmp(config_values[i][0], key) == 0) {
            strncpy(config_values[i][1], value, 255);
            return;
        }
    }
    
    // Add new if space available
    if (config_count < 100) {
        strncpy(config_values[config_count][0], key, 255);
        strncpy(config_values[config_count][1], value, 255);
        config_count++;
    }
}

// Task reception utilities - placeholder implementations
int receive_task(int sockfd, task_info_t* task_info) {
    // This will be implemented properly when we move task_receiver.c
    (void)sockfd;
    (void)task_info;
    printf("Task reception functionality not yet implemented\n");
    return -1;
}

void cleanup_task_info(task_info_t* task_info) {
    if (task_info && task_info->data) {
        free(task_info->data);
        task_info->data = NULL;
        task_info->data_size = 0;
    }
}

// Result sending utilities - placeholder implementation
int send_result(int sockfd, const char* task_id, const char* result_file) {
    // This will be implemented properly when we move send_result.c
    (void)sockfd;
    (void)task_id;
    (void)result_file;
    printf("Result sending functionality not yet implemented\n");
    return -1;
}
