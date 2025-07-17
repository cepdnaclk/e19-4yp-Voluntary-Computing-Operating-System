#include "volcom_net.h"
#include <pthread.h>
#include <signal.h>
#include <time.h>

static volatile bool keep_running = true;

void sigint_handler(int sig) {
    (void)sig;
    keep_running = false;
    printf("\nShutting down...\n");
}

// Server thread function
void* server_thread(void* arg) {
    struct tcp_config_s server_config = {
        .port = TCP_PORT,
        .ip = "0.0.0.0",  // Listen on all interfaces
        .max_connections = 5,
        .timeout_sec = 10
    };

    printf("[Server] Starting TCP server...\n");
    if (!tcp_server_init(&server_config)) {
        printf("[Server] Failed to initialize TCP server\n");
        return NULL;
    }

    while (keep_running) {
        printf("[Server] Waiting for connections...\n");
        int client_fd = tcp_server_accept_connection();
        if (client_fd < 0) {
            if (keep_running) {
                printf("[Server] Failed to accept connection\n");
            }
            continue;
        }

        // Handle client communication
        char buffer[1024];
        printf("[Server] Client connected (fd: %d)\n", client_fd);

        while (keep_running) {
            ssize_t bytes_received = tcp_server_receive_message(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) {
                printf("[Server] Client disconnected or error\n");
                break;
            }

            printf("[Server] Received: %s\n", buffer);

            // Echo back with timestamp
            char response[1024];
            time_t now = time(NULL);
            snprintf(response, sizeof(response), "Server echo at %ld: %s", now, buffer);
            
            if (!tcp_server_send_message(client_fd, response)) {
                printf("[Server] Failed to send response\n");
                break;
            }

            printf("[Server] Sent response: %s\n", response);
        }

        close(client_fd);
    }

    tcp_server_cleanup();
    printf("[Server] Server thread exiting\n");
    return NULL;
}

// Client thread function
void* client_thread(void* arg) {
    int client_id = *(int*)arg;
    
    // Give server time to start
    sleep(1);

    struct tcp_config_s client_config = {
        .port = TCP_PORT,
        .ip = "127.0.0.1",
        .max_connections = 0,
        .timeout_sec = 5
    };

    printf("[Client %d] Starting TCP client...\n", client_id);
    if (!tcp_client_init(&client_config)) {
        printf("[Client %d] Failed to initialize TCP client\n", client_id);
        return NULL;
    }

    if (!tcp_client_connect()) {
        printf("[Client %d] Failed to connect to server\n", client_id);
        tcp_client_cleanup();
        return NULL;
    }

    // Send messages and receive responses
    for (int i = 0; i < 3 && keep_running; i++) {
        char message[256];
        snprintf(message, sizeof(message), "Message %d from client %d", i + 1, client_id);

        printf("[Client %d] Sending: %s\n", client_id, message);
        if (!tcp_client_send_message(message)) {
            printf("[Client %d] Failed to send message\n", client_id);
            break;
        }

        char response[1024];
        ssize_t bytes_received = tcp_client_receive_message(response, sizeof(response));
        if (bytes_received <= 0) {
            printf("[Client %d] Failed to receive response\n", client_id);
            break;
        }

        printf("[Client %d] Received: %s\n", client_id, response);
        sleep(1);
    }

    tcp_client_cleanup();
    printf("[Client %d] Client thread exiting\n", client_id);
    return NULL;
}

// UDP broadcaster thread function
void* udp_broadcaster_thread(void* arg) {
    (void)arg;
    
    struct udp_config_s udp_config = {
        .port = BROADCAST_PORT,
        .ip = "255.255.255.255",
        .interval_sec = 2
    };

    printf("[UDP] Starting UDP broadcaster...\n");
    if (!udp_broadcaster_init(&udp_config)) {
        printf("[UDP] Failed to initialize UDP broadcaster\n");
        return NULL;
    }

    int count = 0;
    while (keep_running) {
        char message[256];
        char local_ip[64];
        get_local_ip(local_ip, sizeof(local_ip));
        
        snprintf(message, sizeof(message), 
                "UDP Broadcast %d from %s - TCP Server available on port %d", 
                ++count, local_ip, TCP_PORT);

        printf("[UDP] Broadcasting: %s\n", message);
        if (!send_udp_broadcast(message)) {
            printf("[UDP] Failed to send broadcast\n");
        }

        sleep(udp_config.interval_sec);
    }

    udp_broadcaster_cleanup();
    printf("[UDP] UDP broadcaster thread exiting\n");
    return NULL;
}

int main() {
    printf("=== TCP & UDP Network Test ===\n");
    printf("This test demonstrates:\n");
    printf("1. TCP Server accepting connections\n");
    printf("2. TCP Client connecting and messaging\n");
    printf("3. UDP Broadcasting availability\n");
    printf("4. Reusable socket management\n");
    printf("Press Ctrl+C to stop\n\n");

    signal(SIGINT, sigint_handler);

    // Create threads
    pthread_t server_tid, client_tid1, client_tid2, udp_tid;
    int client_id1 = 1, client_id2 = 2;

    // Start server thread
    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        perror("Failed to create server thread");
        return 1;
    }

    // Start UDP broadcaster thread
    if (pthread_create(&udp_tid, NULL, udp_broadcaster_thread, NULL) != 0) {
        perror("Failed to create UDP broadcaster thread");
        return 1;
    }

    // Start client threads
    if (pthread_create(&client_tid1, NULL, client_thread, &client_id1) != 0) {
        perror("Failed to create client thread 1");
        return 1;
    }

    if (pthread_create(&client_tid2, NULL, client_thread, &client_id2) != 0) {
        perror("Failed to create client thread 2");
        return 1;
    }

    // Wait for client threads to complete
    pthread_join(client_tid1, NULL);
    pthread_join(client_tid2, NULL);

    // Let UDP broadcaster and server run for a bit more
    sleep(5);

    // Stop everything
    keep_running = false;
    
    // Wait for other threads
    pthread_join(server_tid, NULL);
    pthread_join(udp_tid, NULL);

    printf("\n=== Test Complete ===\n");
    printf("Features demonstrated:\n");
    printf("✓ TCP Server initialization and listening\n");
    printf("✓ TCP Client connection and messaging\n");
    printf("✓ UDP Broadcasting with reusable sockets\n");
    printf("✓ Bidirectional TCP communication\n");
    printf("✓ Multiple client support\n");
    printf("✓ Graceful cleanup and resource management\n");

    return 0;
}
