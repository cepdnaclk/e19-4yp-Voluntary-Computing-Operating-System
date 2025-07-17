#include "volcom_net.h"
#include <pthread.h>
#include <signal.h>

static volatile bool running = true;

void sigint_handler(int sig) {
    (void)sig;
    running = false;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    
    if (argc != 2) {
        printf("Usage: %s <server|client>\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "server") == 0) {
        printf("=== TCP Server Mode ===\n");
        
        struct tcp_config_s config = {
            .port = TCP_PORT,
            .ip = "0.0.0.0",
            .max_connections = 5,
            .timeout_sec = 10
        };
        
        if (!tcp_server_init(&config)) {
            printf("Failed to initialize TCP server\n");
            return 1;
        }
        
        printf("Server listening on port %d. Press Ctrl+C to stop.\n", TCP_PORT);
        
        while (running) {
            int client_fd = tcp_server_accept_connection();
            if (client_fd < 0) continue;
            
            char buffer[1024];
            ssize_t bytes = tcp_server_receive_message(client_fd, buffer, sizeof(buffer));
            if (bytes > 0) {
                printf("Received: %s\n", buffer);
                
                char response[1024];
                snprintf(response, sizeof(response), "Server processed: %s", buffer);
                tcp_server_send_message(client_fd, response);
                printf("Sent response: %s\n", response);
            }
            
            close(client_fd);
        }
        
        tcp_server_cleanup();
        printf("Server stopped.\n");
        
    } else if (strcmp(argv[1], "client") == 0) {
        printf("=== TCP Client Mode ===\n");
        
        struct tcp_config_s config = {
            .port = TCP_PORT,
            .ip = "127.0.0.1",
            .max_connections = 0,
            .timeout_sec = 5
        };
        
        if (!tcp_client_init(&config)) {
            printf("Failed to initialize TCP client\n");
            return 1;
        }
        
        if (!tcp_client_connect()) {
            printf("Failed to connect to server\n");
            tcp_client_cleanup();
            return 1;
        }
        
        printf("Connected to server. Type messages (or 'quit' to exit):\n");
        
        char input[1024];
        while (running && fgets(input, sizeof(input), stdin)) {
            // Remove newline
            input[strcspn(input, "\n")] = '\0';
            
            if (strcmp(input, "quit") == 0) break;
            
            if (tcp_client_send_message(input)) {
                char response[1024];
                ssize_t bytes = tcp_client_receive_message(response, sizeof(response));
                if (bytes > 0) {
                    printf("Server response: %s\n", response);
                } else {
                    printf("Failed to receive response\n");
                    break;
                }
            } else {
                printf("Failed to send message\n");
                break;
            }
        }
        
        tcp_client_cleanup();
        printf("Client disconnected.\n");
        
    } else {
        printf("Invalid mode. Use 'server' or 'client'\n");
        return 1;
    }
    
    return 0;
}
