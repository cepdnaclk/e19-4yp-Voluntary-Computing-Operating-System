#ifndef VOLOCOM_NET_H
#define VOLOCOM_NET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/un.h>
#include <errno.h>

// Define broadcast port and TCP port
#define BROADCAST_PORT 9876
#define TCP_PORT 12345
#define UNIX_SOCKET_PATH "/tmp/volcom_unix_socket"

struct udp_config_s{
    int port;
    const char *ip;
    int interval_sec;
};

struct tcp_config_s{
    int port;
    const char *ip;
    int max_connections;  // For server mode
    int timeout_sec;      // For client operations
};

struct unix_socket_config_s{
    const char *socket_path;
    int buffer_size;
    int timeout_sec;
};

// UDP Functions
bool udp_broadcaster_init(struct udp_config_s *config);
bool send_udp_broadcast(const char* message);
void udp_broadcaster_cleanup(void);

// TCP Server Functions
bool tcp_server_init(struct tcp_config_s *config);
int tcp_server_accept_connection(void);
bool tcp_server_send_message(int client_fd, const char* message);
ssize_t tcp_server_receive_message(int client_fd, char* buffer, size_t buffer_size);
void tcp_server_cleanup(void);

// TCP Client Functions
bool tcp_client_init(struct tcp_config_s *config);
bool tcp_client_connect(void);
bool tcp_client_send_message(const char* message);
ssize_t tcp_client_receive_message(char* buffer, size_t buffer_size);
void tcp_client_cleanup(void);

// Unix Socket Functions
bool unix_socket_server_init(struct unix_socket_config_s *config);
int unix_socket_server_accept(void);
bool unix_socket_server_send_message(int client_fd, const char* message);
ssize_t unix_socket_server_receive_message(int client_fd, char* buffer, size_t buffer_size);
void unix_socket_server_cleanup(void);

bool unix_socket_client_init(struct unix_socket_config_s *config);
bool unix_socket_client_connect(void);
bool unix_socket_client_send_message(const char* message);
ssize_t unix_socket_client_receive_message(char* buffer, size_t buffer_size);
void unix_socket_client_cleanup(void);

// Utility Functions
void get_local_ip(char* buffer, size_t size);
int start_tcp_server(int port);
int accept_tcp_connection(int server_fd);

#endif // VOLOCOM_NET_H