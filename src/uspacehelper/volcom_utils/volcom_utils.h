#ifndef VOLCOM_UTILS_H
#define VOLCOM_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// System initialization and cleanup
int init_volcom_utils(void);
void cleanup_volcom_utils(void);

// System information display
void show_system_info(void);

// Network utilities
void send_udp_broadcast(const char* message);
void get_local_ip(char* buffer, size_t size);
int start_tcp_server(int port);
int accept_tcp_connection(int server_fd);

// Configuration utilities
int load_volcom_config(const char* config_file);
const char* get_volcom_config_value(const char* key);
void set_volcom_config_value(const char* key, const char* value);

// Task reception utilities
typedef struct {
    char task_id[64];
    char chunk_filename[256];
    char sender_id[64];
    char receiver_id[64];
    char status[32];
    size_t data_size;
    void* data;
} task_info_t;

int receive_task(int sockfd, task_info_t* task_info);
void cleanup_task_info(task_info_t* task_info);

// Result sending utilities
int send_result(int sockfd, const char* task_id, const char* result_file);

#endif // VOLCOM_UTILS_H
