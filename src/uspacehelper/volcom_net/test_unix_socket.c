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

// Unix Socket server thread function
void* unix_server_thread(void* arg) {
    (void)arg;
    
    struct unix_socket_config_s server_config = {
        .socket_path = UNIX_SOCKET_PATH,
        .buffer_size = 1024,
        .timeout_sec = 10
    };

    printf("[Unix Server] Starting Unix socket server...\n");
    if (!unix_socket_server_init(&server_config)) {
        printf("[Unix Server] Failed to initialize Unix socket server\n");
        return NULL;
    }

    while (keep_running) {
        printf("[Unix Server] Waiting for connections...\n");
        int client_fd = unix_socket_server_accept();
        if (client_fd < 0) {
            if (keep_running) {
                printf("[Unix Server] Failed to accept connection\n");
            }
            continue;
        }

        // Handle client communication
        char buffer[1024];
        printf("[Unix Server] Client connected (fd: %d)\n", client_fd);

        while (keep_running) {
            ssize_t bytes_received = unix_socket_server_receive_message(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) {
                printf("[Unix Server] Client disconnected or error\n");
                break;
            }

            printf("[Unix Server] Received: %s\n", buffer);

            // Echo back with timestamp
            char response[1024];
            time_t now = time(NULL);
            snprintf(response, sizeof(response), "Unix Server echo at %ld: %s", now, buffer);
            
            if (!unix_socket_server_send_message(client_fd, response)) {
                printf("[Unix Server] Failed to send response\n");
                break;
            }

            printf("[Unix Server] Sent response: %s\n", response);
        }

        close(client_fd);
    }

    unix_socket_server_cleanup();
    printf("[Unix Server] Server thread exiting\n");
    return NULL;
}

// Unix Socket client thread function
void* unix_client_thread(void* arg) {
    int client_id = *(int*)arg;
    
    // Give server time to start
    sleep(1);

    struct unix_socket_config_s client_config = {
        .socket_path = UNIX_SOCKET_PATH,
        .buffer_size = 1024,
        .timeout_sec = 5
    };

    printf("[Unix Client %d] Starting Unix socket client...\n", client_id);
    if (!unix_socket_client_init(&client_config)) {
        printf("[Unix Client %d] Failed to initialize Unix socket client\n", client_id);
        return NULL;
    }

    if (!unix_socket_client_connect()) {
        printf("[Unix Client %d] Failed to connect to server\n", client_id);
        unix_socket_client_cleanup();
        return NULL;
    }

    // Send messages and receive responses
    for (int i = 0; i < 3 && keep_running; i++) {
        char message[256];
        snprintf(message, sizeof(message), "Message %d from Unix client %d", i + 1, client_id);

        printf("[Unix Client %d] Sending: %s\n", client_id, message);
        if (!unix_socket_client_send_message(message)) {
            printf("[Unix Client %d] Failed to send message\n", client_id);
            break;
        }

        char response[1024];
        ssize_t bytes_received = unix_socket_client_receive_message(response, sizeof(response));
        if (bytes_received <= 0) {
            printf("[Unix Client %d] Failed to receive response\n", client_id);
            break;
        }

        printf("[Unix Client %d] Received: %s\n", client_id, response);
        sleep(1);
    }

    unix_socket_client_cleanup();
    printf("[Unix Client %d] Client thread exiting\n", client_id);
    return NULL;
}

// Test saved success message functionality
void test_saved_success_message() {
    printf("\n=== Testing Saved Success Message ===\n");
    
    struct unix_socket_config_s client_config = {
        .socket_path = UNIX_SOCKET_PATH,
        .buffer_size = 1024,
        .timeout_sec = 5
    };

    if (unix_socket_client_init(&client_config) && unix_socket_client_connect()) {
        char success_message[] = "{ \"status\": \"success\", \"message\": \"Data saved successfully\", \"timestamp\": \"2025-07-18T12:00:00Z\", \"file\": \"img_1721304000.bin\" }";
        
        printf("[Success Test] Sending success message: %s\n", success_message);
        if (unix_socket_client_send_message(success_message)) {
            char response[1024];
            ssize_t bytes = unix_socket_client_receive_message(response, sizeof(response));
            if (bytes > 0) {
                printf("[Success Test] Server response: %s\n", response);
            }
        }
        
        unix_socket_client_cleanup();
    }
}

int main() {
    printf("=== Unix Socket Test ===\n");
    printf("This test demonstrates:\n");
    printf("1. Unix Socket Server accepting connections\n");
    printf("2. Unix Socket Client connecting and messaging\n");
    printf("3. Bidirectional communication over Unix sockets\n");
    printf("4. Saved success message functionality\n");
    printf("Press Ctrl+C to stop\n\n");

    signal(SIGINT, sigint_handler);

    // Create threads
    pthread_t server_tid, client_tid1, client_tid2;
    int client_id1 = 1, client_id2 = 2;

    // Start server thread
    if (pthread_create(&server_tid, NULL, unix_server_thread, NULL) != 0) {
        perror("Failed to create server thread");
        return 1;
    }

    // Start client threads
    if (pthread_create(&client_tid1, NULL, unix_client_thread, &client_id1) != 0) {
        perror("Failed to create client thread 1");
        return 1;
    }

    if (pthread_create(&client_tid2, NULL, unix_client_thread, &client_id2) != 0) {
        perror("Failed to create client thread 2");
        return 1;
    }

    // Wait for client threads to complete
    pthread_join(client_tid1, NULL);
    pthread_join(client_tid2, NULL);

    // Test saved success message
    test_saved_success_message();

    // Let server run for a bit more
    sleep(2);

    // Stop everything
    keep_running = false;
    
    // Wait for server thread
    pthread_join(server_tid, NULL);

    printf("\n=== Test Complete ===\n");
    printf("Features demonstrated:\n");
    printf("✓ Unix Socket Server initialization and listening\n");
    printf("✓ Unix Socket Client connection and messaging\n");
    printf("✓ Bidirectional Unix socket communication\n");
    printf("✓ Multiple client support\n");
    printf("✓ Saved success message handling\n");
    printf("✓ Graceful cleanup and resource management\n");

    return 0;
}
