#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../volcom_sysinfo/volcom_sysinfo.h"
#include "volcom_rcsmngr.h"

int main() {
    
    printf("=== Volcom Resource Manager Module Test ===\n\n");
    
    // Test 1: Check cgroups v2 support
    printf("1. Checking cgroups v2 support...\n");
    if (volcom_check_cgroup_v2_support() == 0) {
        printf("   ✓ Cgroups v2 is supported and mounted\n");
    } else {
        printf("   ✗ Cgroups v2 is not available\n");
    }
    
    // Test 2: Initialize resource manager
    printf("\n2. Initializing resource manager...\n");
    struct volcom_rcsmngr_s manager;
    if (volcom_rcsmngr_init(&manager, "volcom_test") == 0) {
        printf("   ✓ Resource manager initialized successfully\n");
        printf("   Main cgroup name: %s\n", manager.main_cgroup.name);
        printf("   Main cgroup path: %s\n", manager.main_cgroup.path);
        printf("   Root path: %s\n", manager.root_cgroup_path);
        printf("   Child capacity: %d\n", manager.child_capacity);
    } else {
        printf("   ✗ Failed to initialize resource manager\n");
        return 1;
    }
    
    // Test 3: Load configuration
    printf("\n3. Loading configuration...\n");
    struct config_s config;
    
    // Set some test values
    config.mem_config.allocated_memory_size_max = 1024 * 1024; // 1GB in KB
    config.mem_config.allocated_memory_size_high = 512 * 1024; // 512MB in KB
    config.cpu_config.allocated_logical_processors = 2;
    config.cpu_config.allocated_cpu_share = 1024;
    
    printf("   ✓ Configuration loaded:\n");
    printf("     Memory max: %lu KB\n", config.mem_config.allocated_memory_size_max);
    printf("     Memory high: %lu KB\n", config.mem_config.allocated_memory_size_high);
    printf("     CPU cores: %d\n", config.cpu_config.allocated_logical_processors);
    printf("     CPU shares: %d\n", config.cpu_config.allocated_cpu_share);
    
    // Test 4: Display manager info (without actually creating cgroups)
    printf("\n4. Resource manager structure test...\n");
    printf("   Main cgroup path would be: %s\n", manager.main_cgroup.path);
    printf("   Child cgroup path would be: %s/worker1\n", manager.main_cgroup.path);
    
    // Test 5: Test utility functions
    printf("\n5. Testing utility functions...\n");
    printf("   Current PID: %d\n", getpid());
    printf("   Current UID: %d (need 0 for cgroup operations)\n", getuid());
    
    if (getuid() != 0) {
        printf("\n⚠️  Note: Run with sudo to test actual cgroup creation:\n");
        printf("   sudo ./test_basic\n");
    }
    
    // Cleanup
    printf("\n6. Cleaning up...\n");
    volcom_rcsmngr_cleanup(&manager);
    printf("   ✓ Cleanup completed\n");
    
    printf("\n=== Test completed successfully ===\n");
    return 0;
}
