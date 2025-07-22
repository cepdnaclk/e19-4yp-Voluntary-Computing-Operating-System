#include "volcom_net.h"
#include <net/if.h>

static int sockfd = -1;
static struct sockaddr_in broadcast_addr;

// TCP server state
static int tcp_server_sockfd = -1;
static struct sockaddr_in tcp_server_addr;

// TCP client state
static int tcp_client_sockfd = -1;
static struct sockaddr_in tcp_client_addr;

// Unix socket server state
static int unix_server_sockfd = -1;
static struct sockaddr_un unix_server_addr;

// Unix socket client state
static int unix_client_sockfd = -1;
static struct sockaddr_un unix_client_addr;

bool udp_broadcaster_init(struct udp_config_s *config){

    if (!config) {
        fprintf(stderr, "Invalid UDP config\n");
        return false;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return false;
    }

    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable))) {
        perror("setsockopt (SO_BROADCAST) failed");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(config->port);
    
     if (inet_pton(AF_INET, config->ip, &broadcast_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    printf("UDP broadcaster initialized on port %d, IP %s\n", config->port, config->ip);
    return true;
}

bool send_udp_broadcast1(const char* message) {
    
    if (sockfd < 0) {
        fprintf(stderr, "Broadcaster not initialized\n");
        return false;
    }

    ssize_t sent_bytes = sendto(sockfd, message, strlen(message), 0,
                              (const struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
    
    
    if (sent_bytes < 0) {
        perror("sendto failed");
        return false;
    }
    
    return true;
}

void udp_broadcaster_cleanup() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
}

void get_local_ip_n(char* buffer, size_t size) {
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        strncpy(buffer, "127.0.0.1", size - 1);
        buffer[size - 1] = '\0';
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
            char* ip_str = inet_ntoa(addr_in->sin_addr);
            
            // Skip loopback (127.x.x.x)
            if (strncmp(ip_str, "127.", 4) != 0) {
                strncpy(buffer, ip_str, size - 1);
                buffer[size - 1] = '\0';
                freeifaddrs(ifaddr);
                return;
            }
        }
    }

    // Fallback to localhost if no other interface found
    strncpy(buffer, "127.0.0.1", size - 1);
    buffer[size - 1] = '\0';
    freeifaddrs(ifaddr);
}

// TCP Server Functions
bool tcp_server_init(struct tcp_config_s *config) {

    if (!config) {
        fprintf(stderr, "Invalid TCP config\n");
        return false;
    }

    if ((tcp_server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP server socket creation failed");
        return false;
    }

    // Allow socket reuse
    int reuse = 1;
    if (setsockopt(tcp_server_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        perror("setsockopt (SO_REUSEADDR) failed");
        close(tcp_server_sockfd);
        tcp_server_sockfd = -1;
        return false;
    }

    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = htons(config->port);
    
    if (inet_pton(AF_INET, config->ip, &tcp_server_addr.sin_addr) <= 0) {
        perror("inet_pton failed for TCP server");
        close(tcp_server_sockfd);
        tcp_server_sockfd = -1;
        return false;
    }

    if (bind(tcp_server_sockfd, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        perror("TCP server bind failed");
        close(tcp_server_sockfd);
        tcp_server_sockfd = -1;
        return false;
    }

    if (listen(tcp_server_sockfd, config->max_connections) < 0) {
        perror("TCP server listen failed");
        close(tcp_server_sockfd);
        tcp_server_sockfd = -1;
        return false;
    }

    printf("TCP server initialized on port %d, IP %s, max connections: %d\n", 
           config->port, config->ip, config->max_connections);
    return true;
}

int tcp_server_accept_connection() {

    if (tcp_server_sockfd < 0) {
        fprintf(stderr, "TCP server not initialized\n");
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(tcp_server_sockfd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("TCP server accept failed");
        return -1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    printf("TCP server accepted connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

    return client_fd;
}

bool tcp_server_send_message(int client_fd, const char* message) {

    if (client_fd < 0 || !message) {
        fprintf(stderr, "Invalid client fd or message\n");
        return false;
    }

    size_t message_len = strlen(message);
    ssize_t sent_bytes = send(client_fd, message, message_len, 0);
    
    if (sent_bytes < 0) {
        perror("TCP server send failed");
        return false;
    }

    if ((size_t)sent_bytes != message_len) {
        fprintf(stderr, "TCP server partial send: %zd/%zu bytes\n", sent_bytes, message_len);
        return false;
    }

    return true;
}

ssize_t tcp_server_receive_message(int client_fd, char* buffer, size_t buffer_size) {

    if (client_fd < 0 || !buffer || buffer_size == 0) {
        fprintf(stderr, "Invalid parameters for TCP server receive\n");
        return -1;
    }

    ssize_t received_bytes = recv(client_fd, buffer, buffer_size - 1, 0);
    if (received_bytes < 0) {
        perror("TCP server receive failed");
        return -1;
    }

    if (received_bytes == 0) {
        printf("TCP server: Client disconnected\n");
        return 0;
    }

    buffer[received_bytes] = '\0';  // Null-terminate
    return received_bytes;
}

void tcp_server_cleanup() {

    if (tcp_server_sockfd >= 0) {
        close(tcp_server_sockfd);
        tcp_server_sockfd = -1;
    }
}

// TCP Client Functions
bool tcp_client_init(struct tcp_config_s *config) {

    if (!config) {
        fprintf(stderr, "Invalid TCP client config\n");
        return false;
    }

    if ((tcp_client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP client socket creation failed");
        return false;
    }

    memset(&tcp_client_addr, 0, sizeof(tcp_client_addr));
    tcp_client_addr.sin_family = AF_INET;
    tcp_client_addr.sin_port = htons(config->port);
    
    if (inet_pton(AF_INET, config->ip, &tcp_client_addr.sin_addr) <= 0) {
        perror("inet_pton failed for TCP client");
        close(tcp_client_sockfd);
        tcp_client_sockfd = -1;
        return false;
    }

    printf("TCP client initialized for %s:%d\n", config->ip, config->port);
    return true;
}

bool tcp_client_connect() {

    if (tcp_client_sockfd < 0) {
        fprintf(stderr, "TCP client not initialized\n");
        return false;
    }

    if (connect(tcp_client_sockfd, (struct sockaddr *)&tcp_client_addr, sizeof(tcp_client_addr)) < 0) {
        perror("TCP client connect failed");
        return false;
    }

    printf("TCP client connected successfully\n");
    return true;
}

bool tcp_client_send_message(const char* message) {
    
    if (tcp_client_sockfd < 0 || !message) {
        fprintf(stderr, "TCP client not connected or invalid message\n");
        return false;
    }

    size_t message_len = strlen(message);
    ssize_t sent_bytes = send(tcp_client_sockfd, message, message_len, 0);
    
    if (sent_bytes < 0) {
        perror("TCP client send failed");
        return false;
    }

    if ((size_t)sent_bytes != message_len) {
        fprintf(stderr, "TCP client partial send: %zd/%zu bytes\n", sent_bytes, message_len);
        return false;
    }

    return true;
}

ssize_t tcp_client_receive_message(char* buffer, size_t buffer_size) {

    if (tcp_client_sockfd < 0 || !buffer || buffer_size == 0) {
        fprintf(stderr, "Invalid parameters for TCP client receive\n");
        return -1;
    }

    ssize_t received_bytes = recv(tcp_client_sockfd, buffer, buffer_size - 1, 0);
    if (received_bytes < 0) {
        perror("TCP client receive failed");
        return -1;
    }

    if (received_bytes == 0) {
        printf("TCP client: Server disconnected\n");
        return 0;
    }

    buffer[received_bytes] = '\0';  // Null-terminate
    return received_bytes;
}

void tcp_client_cleanup() {
    
    if (tcp_client_sockfd >= 0) {
        close(tcp_client_sockfd);
        tcp_client_sockfd = -1;
    }
}

// Unix Socket Server Functions
bool unix_socket_server_init(struct unix_socket_config_s *config) {

    if (!config) {
        fprintf(stderr, "Invalid Unix socket config\n");
        return false;
    }

    if ((unix_server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Unix socket server creation failed");
        return false;
    }

    memset(&unix_server_addr, 0, sizeof(unix_server_addr));
    unix_server_addr.sun_family = AF_UNIX;
    strncpy(unix_server_addr.sun_path, config->socket_path, sizeof(unix_server_addr.sun_path) - 1);

    // Remove socket file if it exists
    unlink(config->socket_path);

    if (bind(unix_server_sockfd, (struct sockaddr *)&unix_server_addr, sizeof(unix_server_addr)) < 0) {
        perror("Unix socket server bind failed");
        close(unix_server_sockfd);
        unix_server_sockfd = -1;
        return false;
    }

    if (listen(unix_server_sockfd, 5) < 0) {
        perror("Unix socket server listen failed");
        close(unix_server_sockfd);
        unix_server_sockfd = -1;
        return false;
    }

    printf("Unix socket server initialized on path: %s\n", config->socket_path);
    return true;
}

int unix_socket_server_accept(void) {

    if (unix_server_sockfd < 0) {
        fprintf(stderr, "Unix socket server not initialized\n");
        return -1;
    }

    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(unix_server_sockfd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("Unix socket server accept failed");
        return -1;
    }

    printf("Unix socket server accepted connection\n");
    return client_fd;
}

bool unix_socket_server_send_message(int client_fd, const char* message) {

    if (client_fd < 0 || !message) {
        fprintf(stderr, "Invalid client fd or message\n");
        return false;
    }

    size_t message_len = strlen(message);
    ssize_t sent_bytes = send(client_fd, message, message_len, 0);
    
    if (sent_bytes < 0) {
        perror("Unix socket server send failed");
        return false;
    }

    if ((size_t)sent_bytes != message_len) {
        fprintf(stderr, "Unix socket server partial send: %zd/%zu bytes\n", sent_bytes, message_len);
        return false;
    }

    return true;
}

ssize_t unix_socket_server_receive_message(int client_fd, char* buffer, size_t buffer_size) {

    if (client_fd < 0 || !buffer || buffer_size == 0) {
        fprintf(stderr, "Invalid parameters for Unix socket server receive\n");
        return -1;
    }

    ssize_t received_bytes = recv(client_fd, buffer, buffer_size - 1, 0);
    if (received_bytes < 0) {
        perror("Unix socket server receive failed");
        return -1;
    }

    if (received_bytes == 0) {
        printf("Unix socket server: Client disconnected\n");
        return 0;
    }

    buffer[received_bytes] = '\0';  // Null-terminate
    return received_bytes;
}

void unix_socket_server_cleanup(void) {

    if (unix_server_sockfd >= 0) {
        close(unix_server_sockfd);
        unix_server_sockfd = -1;
    }
    // Remove socket file
    unlink(unix_server_addr.sun_path);
}

// Unix Socket Client Functions
bool unix_socket_client_init(struct unix_socket_config_s *config) {

    if (!config) {
        fprintf(stderr, "Invalid Unix socket client config\n");
        return false;
    }

    if ((unix_client_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Unix socket client creation failed");
        return false;
    }

    memset(&unix_client_addr, 0, sizeof(unix_client_addr));
    unix_client_addr.sun_family = AF_UNIX;
    strncpy(unix_client_addr.sun_path, config->socket_path, sizeof(unix_client_addr.sun_path) - 1);

    printf("Unix socket client initialized for path: %s\n", config->socket_path);
    return true;
}

bool unix_socket_client_connect(void) {

    if (unix_client_sockfd < 0) {
        fprintf(stderr, "Unix socket client not initialized\n");
        return false;
    }

    if (connect(unix_client_sockfd, (struct sockaddr *)&unix_client_addr, sizeof(unix_client_addr)) < 0) {
        perror("Unix socket client connect failed");
        return false;
    }

    printf("Unix socket client connected successfully\n");
    return true;
}

bool unix_socket_client_send_message(const char* message) {

    if (unix_client_sockfd < 0 || !message) {
        fprintf(stderr, "Unix socket client not connected or invalid message\n");
        return false;
    }

    size_t message_len = strlen(message);
    ssize_t sent_bytes = send(unix_client_sockfd, message, message_len, 0);
    
    if (sent_bytes < 0) {
        perror("Unix socket client send failed");
        return false;
    }

    if ((size_t)sent_bytes != message_len) {
        fprintf(stderr, "Unix socket client partial send: %zd/%zu bytes\n", sent_bytes, message_len);
        return false;
    }

    return true;
}

ssize_t unix_socket_client_receive_message(char* buffer, size_t buffer_size) {

    if (unix_client_sockfd < 0 || !buffer || buffer_size == 0) {
        fprintf(stderr, "Invalid parameters for Unix socket client receive\n");
        return -1;
    }

    ssize_t received_bytes = recv(unix_client_sockfd, buffer, buffer_size - 1, 0);
    if (received_bytes < 0) {
        perror("Unix socket client receive failed");
        return -1;
    }

    if (received_bytes == 0) {
        printf("Unix socket client: Server disconnected\n");
        return 0;
    }

    buffer[received_bytes] = '\0';  // Null-terminate
    return received_bytes;
}

void unix_socket_client_cleanup(void) {
    
    if (unix_client_sockfd >= 0) {
        close(unix_client_sockfd);
        unix_client_sockfd = -1;
    }
}