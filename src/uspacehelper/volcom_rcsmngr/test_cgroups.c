#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../volcom_sysinfo/volcom_sysinfo.h"
#include "volcom_rcsmngr.h"

int main() {
    
    printf("=== Volcom Resource Manager - Cgroup Creation Test ===\n\n");
    
    // Check if running as root
    if (getuid() != 0) {
        printf("⚠️  This test requires root privileges. Please run with sudo.\n");
        printf("   sudo ./test_cgroups\n");
        return 1;
    }
    
    printf("✓ Running with root privileges\n\n");
    
    // Initialize resource manager
    printf("1. Initializing resource manager...\n");
    struct volcom_rcsmngr_s manager;
    if (volcom_rcsmngr_init(&manager, "volcom_test") != 0) {
        printf("✗ Failed to initialize resource manager\n");
        return 1;
    }
    printf("   ✓ Resource manager initialized\n");
    
    // Set up configuration
    printf("\n2. Setting up configuration...\n");
    struct config_s config;
    config.mem_config.allocated_memory_size_max = 512 * 1024; // 512MB in KB
    config.mem_config.allocated_memory_size_high = 256 * 1024; // 256MB in KB
    config.cpu_config.allocated_logical_processors = 2;
    config.cpu_config.allocated_cpu_share = 1024;
    printf("   ✓ Configuration set: 512MB memory, 2 CPU cores, 1024 shares\n");
    
    // Create main cgroup
    printf("\n3. Creating main cgroup...\n");
    if (volcom_create_main_cgroup(&manager, config) != 0) {
        printf("✗ Failed to create main cgroup\n");
        volcom_rcsmngr_cleanup(&manager);
        return 1;
    }
    printf("   ✓ Main cgroup created: %s\n", manager.main_cgroup.path);
    
    // Create a child cgroup
    printf("\n4. Creating child cgroup...\n");
    if (volcom_create_child_cgroup(&manager, "worker1", config) != 0) {
        printf("✗ Failed to create child cgroup\n");
        volcom_delete_main_cgroup(&manager);
        volcom_rcsmngr_cleanup(&manager);
        return 1;
    }
    printf("   ✓ Child cgroup created: worker1\n");
    
    // Add current process to main cgroup
    printf("\n5. Testing PID management...\n");
    pid_t current_pid = getpid();
    if (volcom_add_pid_to_cgroup(&manager, "volcom_test", current_pid) != 0) {
        printf("✗ Failed to add PID to main cgroup\n");
    } else {
        printf("   ✓ Added PID %d to main cgroup\n", current_pid);
    }
    
    // Create a simple child process and add it to child cgroup
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process - just sleep for a bit
        printf("   Child process %d starting...\n", getpid());
        sleep(2);
        printf("   Child process %d finishing...\n", getpid());
        exit(0);
    } else if (child_pid > 0) {
        // Parent process
        if (volcom_add_pid_to_cgroup(&manager, "worker1", child_pid) != 0) {
            printf("✗ Failed to add child PID to worker1 cgroup\n");
        } else {
            printf("   ✓ Added child PID %d to worker1 cgroup\n", child_pid);
        }
        
        // Wait for child to complete
        int status;
        waitpid(child_pid, &status, 0);
        printf("   ✓ Child process completed\n");
    } else {
        printf("✗ Failed to fork child process\n");
    }
    
    // Display cgroup information
    printf("\n6. Current cgroup state...\n");
    volcom_print_cgroup_info(&manager);
    
    // Remove PID from main cgroup
    printf("\n7. Removing PID from main cgroup...\n");
    if (volcom_remove_pid_from_cgroup(&manager, "volcom_test", current_pid) != 0) {
        printf("✗ Failed to remove PID from main cgroup\n");
    } else {
        printf("   ✓ Removed PID %d from main cgroup\n", current_pid);
    }
    
    // Cleanup
    printf("\n8. Cleaning up cgroups...\n");
    if (volcom_delete_child_cgroup(&manager, "worker1") != 0) {
        printf("✗ Failed to delete child cgroup\n");
    } else {
        printf("   ✓ Child cgroup deleted\n");
    }
    
    if (volcom_delete_main_cgroup(&manager) != 0) {
        printf("✗ Failed to delete main cgroup\n");
    } else {
        printf("   ✓ Main cgroup deleted\n");
    }
    
    volcom_rcsmngr_cleanup(&manager);
    printf("   ✓ Resource manager cleanup completed\n");
    
    printf("\n=== Cgroup creation test completed successfully ===\n");
    return 0;
}
