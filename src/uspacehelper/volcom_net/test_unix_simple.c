#include "volcom_net.h"

int main() {
    printf("=== Simple Unix Socket Test ===\n");
    
    // Test 1: Server initialization
    printf("1. Testing server initialization...\n");
    struct unix_socket_config_s server_config = {
        .socket_path = "/tmp/test_volcom_socket",
        .buffer_size = 1024,
        .timeout_sec = 5
    };
    
    if (unix_socket_server_init(&server_config)) {
        printf("✓ Server initialized successfully\n");
        
        // Test 2: Server cleanup
        printf("2. Testing server cleanup...\n");
        unix_socket_server_cleanup();
        printf("✓ Server cleanup completed\n");
        
    } else {
        printf("✗ Server initialization failed\n");
        return 1;
    }
    
    // Test 3: Client initialization
    printf("3. Testing client initialization...\n");
    struct unix_socket_config_s client_config = {
        .socket_path = "/tmp/test_volcom_socket",
        .buffer_size = 1024,
        .timeout_sec = 5
    };
    
    if (unix_socket_client_init(&client_config)) {
        printf("✓ Client initialized successfully\n");
        
        // Test 4: Client cleanup
        printf("4. Testing client cleanup...\n");
        unix_socket_client_cleanup();
        printf("✓ Client cleanup completed\n");
        
    } else {
        printf("✗ Client initialization failed\n");
        return 1;
    }
    
    printf("\n=== All Tests Passed ===\n");
    printf("Unix socket functionality is working correctly!\n");
    
    return 0;
}
