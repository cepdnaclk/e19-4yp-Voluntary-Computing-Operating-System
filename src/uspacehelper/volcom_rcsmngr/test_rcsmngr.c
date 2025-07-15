#include "volcom_rcsmngr.h"
#include <sys/wait.h>

void print_usage() {

    printf("Volcom Resource Manager Test Program\n");
    printf("====================================\n");
    printf("This program demonstrates cgroup v2 management functionality\n");
    printf("Usage: ./test_rcsmngr [config_file]\n");
    printf("If no config file is provided, default values will be used\n\n");
}

int create_test_process() {

    pid_t pid = fork();
    if (pid == 0) {
        // Child process - sleep for a while to allow testing
        printf("Test process %d started (will sleep for 30 seconds)\n", getpid());
        sleep(30);
        exit(0);
    } else if (pid > 0) {
        return pid;
    } else {
        perror("fork failed");
        return -1;
    }
}

int main(int argc, char *argv[]) {
    
    print_usage();

    // Load configuration
    struct config_s config = {0};
    if (argc > 1) {
        printf("Loading configuration from: %s\n", argv[1]);
        config = load_config(argv[1]);
    } else {
        printf("Using default configuration\n");
        // Set default values
        config.mem_config.allocated_memory_size_max = 1024 * 1024; // 1GB
        config.mem_config.allocated_memory_size_high = 512 * 1024; // 512MB
        config.cpu_config.allocated_logical_processors = 2;
        config.cpu_config.allocated_cpu_share = 1024;
    }

    print_config(config);

    // Initialize resource manager
    struct volcom_rcsmngr_s manager;
    if (volcom_rcsmngr_init(&manager, "volcom_test") != 0) {
        fprintf(stderr, "Failed to initialize resource manager\n");
        return 1;
    }

    // Create main cgroup
    printf("\n=== Creating Main Cgroup ===\n");
    if (volcom_create_main_cgroup(&manager, config) != 0) {
        fprintf(stderr, "Failed to create main cgroup\n");
        volcom_rcsmngr_cleanup(&manager);
        return 1;
    }

    // Create child cgroups
    printf("\n=== Creating Child Cgroups ===\n");
    if (volcom_create_child_cgroup(&manager, "worker1", config) != 0) {
        fprintf(stderr, "Failed to create worker1 cgroup\n");
    }

    if (volcom_create_child_cgroup(&manager, "worker2", config) != 0) {
        fprintf(stderr, "Failed to create worker2 cgroup\n");
    }

    // Print current state
    volcom_print_cgroup_info(&manager);

    // Create test processes and add them to cgroups
    printf("=== Adding Test Processes ===\n");
    
    pid_t main_pid = create_test_process();
    if (main_pid > 0) {
        if (volcom_add_pid_to_cgroup(&manager, "volcom_test", main_pid) == 0) {
            printf("Added process %d to main cgroup\n", main_pid);
        }
    }

    pid_t worker1_pid = create_test_process();
    if (worker1_pid > 0) {
        if (volcom_add_pid_to_cgroup(&manager, "worker1", worker1_pid) == 0) {
            printf("Added process %d to worker1 cgroup\n", worker1_pid);
        }
    }

    pid_t worker2_pid = create_test_process();
    if (worker2_pid > 0) {
        if (volcom_add_pid_to_cgroup(&manager, "worker2", worker2_pid) == 0) {
            printf("Added process %d to worker2 cgroup\n", worker2_pid);
        }
    }

    // Print updated state
    printf("\n=== After Adding Processes ===\n");
    volcom_print_cgroup_info(&manager);

    // Test getting PID list
    printf("=== Testing PID Retrieval ===\n");
    pid_t *pids;
    int count;
    if (volcom_get_cgroup_pids(&manager, "volcom_test", &pids, &count) == 0) {
        printf("Main cgroup has %d processes: ", count);
        for (int i = 0; i < count; i++) {
            printf("%d ", pids[i]);
        }
        printf("\n");
        if (pids) free(pids);
    }

    // Wait a moment to see processes in action
    printf("\nWaiting 5 seconds to observe processes...\n");
    sleep(5);

    // Remove a process from cgroup
    printf("\n=== Removing Process from Cgroup ===\n");
    if (volcom_remove_pid_from_cgroup(&manager, "worker1", worker1_pid) == 0) {
        printf("Removed process %d from worker1 cgroup\n", worker1_pid);
    }

    // Print updated state
    volcom_print_cgroup_info(&manager);

    // Clean up test processes
    printf("\n=== Cleaning Up Test Processes ===\n");
    if (main_pid > 0) {
        kill(main_pid, SIGTERM);
        waitpid(main_pid, NULL, 0);
        printf("Terminated process %d\n", main_pid);
    }
    if (worker1_pid > 0) {
        kill(worker1_pid, SIGTERM);
        waitpid(worker1_pid, NULL, 0);
        printf("Terminated process %d\n", worker1_pid);
    }
    if (worker2_pid > 0) {
        kill(worker2_pid, SIGTERM);
        waitpid(worker2_pid, NULL, 0);
        printf("Terminated process %d\n", worker2_pid);
    }

    // Delete child cgroups
    printf("\n=== Deleting Child Cgroups ===\n");
    volcom_delete_child_cgroup(&manager, "worker1");
    volcom_delete_child_cgroup(&manager, "worker2");

    // Delete main cgroup
    printf("\n=== Deleting Main Cgroup ===\n");
    volcom_delete_main_cgroup(&manager);

    // Final cleanup
    volcom_rcsmngr_cleanup(&manager);

    printf("\n=== Test Completed Successfully ===\n");
    return 0;
}
