#include "volcom_net.h"

int main() {
    printf("=== Testing JavaScript Integration ===\n");
    
    struct unix_socket_config_s client_config = {
        .socket_path = "/tmp/volcom_unix_socket",
        .buffer_size = 1024,
        .timeout_sec = 5
    };
    
    printf("Connecting to JavaScript server...\n");
    if (!unix_socket_client_init(&client_config)) {
        printf("Failed to initialize client\n");
        return 1;
    }
    
    if (!unix_socket_client_connect()) {
        printf("Failed to connect to JavaScript server\n");
        unix_socket_client_cleanup();
        return 1;
    }
    
    printf("Connected! Sending success message...\n");
    
    // Send a success message
    char success_message[] = "{"
        "\"status\": \"success\","
        "\"message\": \"Data saved successfully from C client\","
        "\"timestamp\": \"2025-07-18T12:00:00Z\","
        "\"file\": \"img_1721304000.bin\","
        "\"size\": 1024"
    "}";
    
    if (unix_socket_client_send_message(success_message)) {
        printf("Success message sent!\n");
        
        // Receive response
        char response[1024];
        ssize_t bytes = unix_socket_client_receive_message(response, sizeof(response));
        if (bytes > 0) {
            printf("JavaScript server response: %s\n", response);
        }
    } else {
        printf("Failed to send success message\n");
    }
    
    unix_socket_client_cleanup();
    printf("Test completed!\n");
    
    return 0;
}
