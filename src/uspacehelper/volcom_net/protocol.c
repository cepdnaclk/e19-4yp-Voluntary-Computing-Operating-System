#include "volcom_net.h"
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

// Protocol implementation (moved from utils/protocol.c)
protocol_status_t send_json(int sockfd, cJSON *json) {
    char *json_str = cJSON_PrintUnformatted(json);
    if (!json_str) return PROTOCOL_ERR;
    
    uint32_t len = strlen(json_str);
    uint32_t net_len = htonl(len);
    
    if (write(sockfd, &net_len, sizeof(net_len)) != sizeof(net_len)) {
        free(json_str);
        return PROTOCOL_ERR;
    }
    
    if (write(sockfd, json_str, len) != (ssize_t)len) {
        free(json_str);
        return PROTOCOL_ERR;
    }
    
    free(json_str);
    return PROTOCOL_OK;
}

protocol_status_t recv_json(int sockfd, cJSON **json_out) {
    uint32_t net_len;
    if (read(sockfd, &net_len, sizeof(net_len)) != sizeof(net_len)) {
        return PROTOCOL_ERR;
    }
    
    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 65536) {
        return PROTOCOL_ERR;
    }
    
    char *buf = malloc(len + 1);
    if (!buf) {
        return PROTOCOL_ERR;
    }
    
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = read(sockfd, buf + recvd, len - recvd);
        if (n <= 0) { 
            free(buf); 
            return PROTOCOL_ERR; 
        }
        recvd += n;
    }
    
    buf[len] = '\0';
    *json_out = cJSON_Parse(buf);
    free(buf);
    
    return *json_out ? PROTOCOL_OK : PROTOCOL_ERR;
}

// Metadata creation utilities
cJSON* create_task_metadata(const char *task_id, const char *chunk_filename, 
                           const char *sender_id, const char *receiver_id, const char *status) {
    cJSON *meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "type", "task");
    cJSON_AddStringToObject(meta, "version", "1");
    cJSON_AddStringToObject(meta, "task_id", task_id);
    cJSON_AddStringToObject(meta, "chunk_filename", chunk_filename);
    cJSON_AddStringToObject(meta, "sender_id", sender_id);
    cJSON_AddStringToObject(meta, "receiver_id", receiver_id);
    cJSON_AddStringToObject(meta, "status", status);
    return meta;
}

cJSON* create_result_metadata(const char *task_id, const char *chunk_filename, 
                             const char *sender_id, const char *receiver_id, const char *status) {
    cJSON *meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "type", "result");
    cJSON_AddStringToObject(meta, "version", "1");
    cJSON_AddStringToObject(meta, "task_id", task_id);
    cJSON_AddStringToObject(meta, "chunk_filename", chunk_filename);
    cJSON_AddStringToObject(meta, "sender_id", sender_id);
    cJSON_AddStringToObject(meta, "receiver_id", receiver_id);
    cJSON_AddStringToObject(meta, "status", status);
    return meta;
}

// TCP connection management
int create_tcp_connection(const char* host, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("TCP socket creation failed");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sockfd);
        return -1;
    }
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP connection failed");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

void close_tcp_connection(int sockfd) {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

// UDP broadcast management
int setup_udp_listener(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket creation failed");
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("UDP setsockopt failed");
        close(sockfd);
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("UDP bind failed");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int receive_udp_broadcast(int sockfd, char* buffer, size_t buffer_size, char* sender_ip) {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    ssize_t bytes_received = recvfrom(sockfd, buffer, buffer_size - 1, 0, 
                                     (struct sockaddr*)&sender_addr, &addr_len);
    
    if (bytes_received < 0) {
        perror("UDP receive failed");
        return -1;
    }
    
    buffer[bytes_received] = '\0';
    
    if (sender_ip) {
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
    }
    
    return bytes_received;
}

// Connection handler
int handle_connection(connection_info_t* conn_info) {
    if (!conn_info) {
        return -1;
    }
    
    // Basic connection handling - can be extended
    printf("Handling connection from %s:%d\n", conn_info->peer_ip, conn_info->peer_port);
    conn_info->connect_time = time(NULL);
    
    return 0;
}

void cleanup_connection(connection_info_t* conn_info) {
    if (conn_info) {
        if (conn_info->sockfd >= 0) {
            close(conn_info->sockfd);
            conn_info->sockfd = -1;
        }
        memset(conn_info, 0, sizeof(connection_info_t));
    }
}

// Function to peek at JSON data from a socket without consuming it
protocol_status_t recv_json_peek(int sockfd, cJSON** json) {
    if (!json) return PROTOCOL_ERR;

    uint32_t net_len;
    ssize_t bytes_read = recv(sockfd, &net_len, sizeof(net_len), MSG_PEEK);
    if (bytes_read != sizeof(net_len)) {
        if (bytes_read == 0) return PROTOCOL_CONN_CLOSED;
        return PROTOCOL_ERR;
    }

    uint32_t len = ntohl(net_len);
    if (len > MAX_JSON_SIZE) {
        return PROTOCOL_ERR;
    }

    // We need to peek at the length prefix + the data
    char* buffer = malloc(sizeof(uint32_t) + len + 1);
    if (!buffer) return PROTOCOL_ERR;

    bytes_read = recv(sockfd, buffer, sizeof(uint32_t) + len, MSG_PEEK);
    if (bytes_read != (ssize_t)(sizeof(uint32_t) + len)) {
        free(buffer);
        return PROTOCOL_ERR;
    }

    buffer[sizeof(uint32_t) + len] = '\0';

    *json = cJSON_Parse(buffer + sizeof(uint32_t));
    free(buffer);

    if (!*json) {
        return PROTOCOL_ERR;
    }

    return PROTOCOL_OK;
}


// Function to send a UDP broadcast message
void send_udp_broadcast(const char* message) {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    int broadcast = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt (SO_BROADCAST)");
        close(sockfd);
        return;
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(9876); // Port for volcom discovery
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("sendto");
    }

    close(sockfd);
}
