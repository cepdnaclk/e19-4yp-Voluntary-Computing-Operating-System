# volcom_net - Network Communication Library

This library provides reusable UDP and TCP networking functionality for the Voluntary Computing Operating System.

## Features

-   **UDP Broadcasting**: Send broadcast messages to discover services
-   **TCP Server**: Accept incoming connections and handle bidirectional communication
-   **TCP Client**: Connect to servers and exchange messages
-   **Reusable Sockets**: Efficient socket management with proper cleanup
-   **Thread-Safe**: Designed for multi-threaded applications

## API Reference

### UDP Functions

```c
struct udp_config_s {
    int port;
    const char *ip;
    int interval_sec;
};

bool udp_broadcaster_init(struct udp_config_s *config);
bool send_udp_broadcast(const char* message);
void udp_broadcaster_cleanup(void);
```

### TCP Server Functions

```c
struct tcp_config_s {
    int port;
    const char *ip;
    int max_connections;
    int timeout_sec;
};

bool tcp_server_init(struct tcp_config_s *config);
int tcp_server_accept_connection(void);
bool tcp_server_send_message(int client_fd, const char* message);
ssize_t tcp_server_receive_message(int client_fd, char* buffer, size_t buffer_size);
void tcp_server_cleanup(void);
```

### TCP Client Functions

```c
bool tcp_client_init(struct tcp_config_s *config);
bool tcp_client_connect(void);
bool tcp_client_send_message(const char* message);
ssize_t tcp_client_receive_message(char* buffer, size_t buffer_size);
void tcp_client_cleanup(void);
```

## Usage Examples

### UDP Broadcasting

```c
#include "volcom_net.h"

struct udp_config_s udp_config = {
    .port = 9876,
    .ip = "255.255.255.255",
    .interval_sec = 5
};

// Initialize UDP broadcaster
if (udp_broadcaster_init(&udp_config)) {
    // Send broadcast message
    send_udp_broadcast("Hello, network!");

    // Cleanup
    udp_broadcaster_cleanup();
}
```

### TCP Server

```c
#include "volcom_net.h"

struct tcp_config_s server_config = {
    .port = 12345,
    .ip = "0.0.0.0",  // Listen on all interfaces
    .max_connections = 5,
    .timeout_sec = 10
};

// Initialize TCP server
if (tcp_server_init(&server_config)) {
    // Accept client connection
    int client_fd = tcp_server_accept_connection();
    if (client_fd >= 0) {
        // Receive message
        char buffer[1024];
        ssize_t bytes = tcp_server_receive_message(client_fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            printf("Received: %s\n", buffer);

            // Send response
            tcp_server_send_message(client_fd, "Message received!");
        }
        close(client_fd);
    }

    // Cleanup
    tcp_server_cleanup();
}
```

### TCP Client

```c
#include "volcom_net.h"

struct tcp_config_s client_config = {
    .port = 12345,
    .ip = "127.0.0.1",
    .max_connections = 0,
    .timeout_sec = 5
};

// Initialize and connect TCP client
if (tcp_client_init(&client_config) && tcp_client_connect()) {
    // Send message
    tcp_client_send_message("Hello, server!");

    // Receive response
    char buffer[1024];
    ssize_t bytes = tcp_client_receive_message(buffer, sizeof(buffer));
    if (bytes > 0) {
        printf("Response: %s\n", buffer);
    }

    // Cleanup
    tcp_client_cleanup();
}
```

## Integration with Employee/Employer Architecture

### Employee Node (Server Mode)

```c
// Employee can use both UDP and TCP
void* employee_main() {
    // 1. Setup UDP broadcasting for availability
    struct udp_config_s udp_config = {
        .port = BROADCAST_PORT,
        .ip = "255.255.255.255",
        .interval_sec = 5
    };

    // 2. Setup TCP server for task reception
    struct tcp_config_s tcp_config = {
        .port = TCP_PORT,
        .ip = "0.0.0.0",
        .max_connections = 10,
        .timeout_sec = 30
    };

    if (udp_broadcaster_init(&udp_config) && tcp_server_init(&tcp_config)) {
        // Broadcasting thread
        pthread_create(&broadcast_thread, NULL, broadcast_worker, NULL);

        // Main server loop
        while (running) {
            int client_fd = tcp_server_accept_connection();
            if (client_fd >= 0) {
                // Handle task from employer
                handle_task_request(client_fd);
            }
        }

        // Cleanup
        udp_broadcaster_cleanup();
        tcp_server_cleanup();
    }
}
```

### Employer Node (Client Mode)

```c
// Employer can use TCP client to send tasks
void send_task_to_employee(const char* employee_ip, const char* task_data) {
    struct tcp_config_s client_config = {
        .port = TCP_PORT,
        .ip = employee_ip,
        .max_connections = 0,
        .timeout_sec = 10
    };

    if (tcp_client_init(&client_config) && tcp_client_connect()) {
        // Send task
        tcp_client_send_message(task_data);

        // Receive result
        char result[4096];
        ssize_t bytes = tcp_client_receive_message(result, sizeof(result));
        if (bytes > 0) {
            printf("Task result: %s\n", result);
        }

        tcp_client_cleanup();
    }
}
```

## Building

```bash
# Build library and test
make all

# Build library only
make libvolcom_net.a

# Run tests
make test

# Clean build artifacts
make clean
```

## Thread Safety

-   Each socket (UDP/TCP server/TCP client) maintains its own state
-   Multiple TCP clients can be used simultaneously
-   UDP broadcaster is reentrant for message sending
-   Proper cleanup prevents resource leaks

## Error Handling

-   All functions return success/failure status
-   Detailed error messages via perror()
-   Graceful degradation on network failures
-   Resource cleanup on errors

## Performance Considerations

-   Socket reuse with SO_REUSEADDR
-   Efficient buffer management
-   Minimal system calls
-   Proper timeout handling

## Integration Notes

This library is designed to replace the scattered networking code in the employee/employer architecture with a unified, reusable interface. It provides the foundation for:

1. **Employee availability broadcasting**
2. **Task distribution via TCP**
3. **Result collection from employees**
4. **Scalable client-server communication**

The library handles the low-level socket operations while exposing a clean API for the application logic.
