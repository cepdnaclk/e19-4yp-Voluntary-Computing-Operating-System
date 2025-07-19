#ifndef VOLCOM_NET_H
#define VOLCOM_NET_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <cjson/cJSON.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_JSON_SIZE 65536
#define MAX_FILENAME_LEN 256
#define MAX_EMPLOYEES 100
#define MAX_TASK_ASSIGNMENTS 1000

// Protocol definitions
#define PROTOCOL_VERSION 1

typedef enum { 
    PROTOCOL_OK = 0, 
    PROTOCOL_ERR = -1,
    PROTOCOL_CONN_CLOSED
} protocol_status_t;

// Protocol functions
protocol_status_t send_json(int sockfd, cJSON *json);
protocol_status_t recv_json(int sockfd, cJSON **json_out);
protocol_status_t recv_json_peek(int sockfd, cJSON **json_out);

// Metadata creation utilities
cJSON* create_task_metadata(const char *task_id, const char *chunk_filename, 
                           const char *sender_id, const char *receiver_id, const char *status);
cJSON* create_result_metadata(const char *task_id, const char *chunk_filename, 
                             const char *sender_id, const char *receiver_id, const char *status);

// TCP connection management
int create_tcp_connection(const char* host, int port);
void close_tcp_connection(int sockfd);

// UDP broadcast management
int setup_udp_listener(int port);
int receive_udp_broadcast(int sockfd, char* buffer, size_t buffer_size, char* sender_ip);

// Connection handler
typedef struct {
    int sockfd;
    char peer_ip[64];
    int peer_port;
    time_t connect_time;
} connection_info_t;

int handle_connection(connection_info_t* conn_info);
void cleanup_connection(connection_info_t* conn_info);

#endif // VOLCOM_NET_H
