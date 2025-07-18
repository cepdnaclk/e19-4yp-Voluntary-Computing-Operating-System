#include "volcom_agents/employer/volcom_employer.h"
#include "volcom_agents/employee/volcom_employee.h"
#include "volcom_sysinfo/volcom_sysinfo.h"
#include "volcom_rcsmngr/volcom_rcsmngr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static struct volcom_rcsmngr_s manager;

// Function declarations
void open_file(char *filename);
void configure_by_cmd(struct config_s *config);
int run_node_in_cgroup(struct volcom_rcsmngr_s *manager, const char *task_name, const char *script_path, const char *data_point);
void process_data_stream(struct volcom_rcsmngr_s *manager, const char *script_path);

void open_file(char *filename) {

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }

    fclose(file);
}

void configure_by_cmd(struct config_s *config) {
    
    // TODO: Implement memory configuration logic
    while (1) {
            char *input = malloc(256);
            printf("Enter configuration (or 'q' to quit or 'menu' for menu): ");
            fgets(input, 256, stdin);
            input[strcspn(input, "\n")] = 0; 

            if (strcmp(input, "q") == 0) {
                free(input);
                break;
            } 

            // TODO: Implement configuration logic
            if (strncmp(input, "config ", 7) == 0) {
                char *arg = input + 7;
                char key[32];
                int value;
                int matched = sscanf(arg, "%31s %d", key, &value);
                printf("%s\n", arg);
                if (matched == 2) {
                    if (strcmp(key, "memory") == 0) {
                        config->mem_config.allocated_memory_size_max = value * 1024; 
                        config->mem_config.allocated_memory_size_high = (value * 1024) / 2; 
                        printf("%d MB of memory allocated for volcom (max: %lu KB, high: %lu KB)\n", 
                               value, config->mem_config.allocated_memory_size_max, 
                               config->mem_config.allocated_memory_size_high);
                    } else if (strcmp(key, "cpu") == 0) {
                        config->cpu_config.allocated_logical_processors = value;
                        config->cpu_config.allocated_cpu_share = 1024; // Default CPU share
                        printf("%d cores allocated for volcom (CPU share: %d)\n", 
                               value, config->cpu_config.allocated_cpu_share);
                    } else if (strcmp(key, "gpu") == 0) {
                        // GPU configuration would go here
                        printf("%d MB of GPU memory allocated for volcom\n", value);
                    } else {
                        printf("Invalid configuration. Use `help` for usage information.\n");
                    }
                } else if (matched == 1) {
                    if (strcmp(key, "memory") == 0 || strcmp(key, "cpu") == 0 || strcmp(key, "gpu") == 0) {
                        printf("Please provide a value for %s.\n", key);
                    } else {
                        printf("Invalid configuration. Use `help` for usage information.\n");
                    }
                } else {
                    printf("Invalid configuration. Use `help` for usage information.\n");
                }

            } else if (strncmp(input, "config", 6) == 0 && strlen(input) == 6) {
                
                printf("Configuration mode. Use 'config <key> <value>' to set configurations.\n");
                printf("Available keys: memory, cpu, gpu.\n");  
            } else if (strncmp(input, "get ", 4) == 0) {
                char *arg = input + 4;
                if (strcmp(arg, "memory") == 0) {
                    struct memory_info_s mem_info = get_memory_info();
                    print_memory_info(mem_info);
                } else if (strcmp(arg, "cpu") == 0) {
                    struct cpu_info_s cpu_info = get_cpu_info();
                    print_cpu_info(cpu_info);
                } else if (strcmp(arg, "cpu_usage") == 0) {
                    struct cpu_info_s cpu_usage = get_cpu_usage(get_cpu_info());
                    print_cpu_usage(cpu_usage);
                } else if (strcmp(arg, "gpu") == 0) {
                    struct gpu_info_s gpu_info = get_gpu_info();
                    print_gpu_info(gpu_info);
                } else {
                    printf("Invalid argument for get. Use 'memory', 'cpu', 'cpu_usage', or 'gpu'.\n");
                }
            } else if (strcmp(input, "get") == 0) {
                printf("Please specify what to get: memory, cpu, cpu_usage, or gpu.\n");
            } else if (strcmp(input, "process") == 0) {
                printf("Starting data stream processing demo...\n");
                
                // Initialize resource manager for interactive processing
                struct volcom_rcsmngr_s interactive_manager;
                if (volcom_rcsmngr_init(&interactive_manager, "volcom_interactive") != 0) {
                    printf("Failed to initialize resource manager for processing\n");
                } else {
                    if (volcom_create_main_cgroup(&interactive_manager, *config) != 0) {
                        printf("Failed to create main cgroup for processing\n");
                    } else {
                        // Process the data stream
                        process_data_stream(&interactive_manager, "./scripts/data_processor.js");
                        
                        // Cleanup
                        volcom_delete_main_cgroup(&interactive_manager);
                    }
                    volcom_rcsmngr_cleanup(&interactive_manager);
                }
            } else if (strncmp(input, "process ", 8) == 0) {
                char *data_point = input + 8;
                printf("Processing single data point: %s\n", data_point);
                
                // Initialize resource manager for single processing
                struct volcom_rcsmngr_s single_manager;
                if (volcom_rcsmngr_init(&single_manager, "volcom_single") != 0) {
                    printf("Failed to initialize resource manager for processing\n");
                } else {
                    if (volcom_create_main_cgroup(&single_manager, *config) != 0) {
                        printf("Failed to create main cgroup for processing\n");
                    } else {
                        // Process single data point
                        int result = run_node_in_cgroup(&single_manager, "SingleProcessor", 
                                                       "./scripts/data_processor.js", data_point);
                        if (result == 0) {
                            printf("✓ Data point processed successfully\n");
                        } else {
                            printf("✗ Data point processing failed\n");
                        }
                        
                        // Cleanup
                        volcom_delete_main_cgroup(&single_manager);
                    }
                    volcom_rcsmngr_cleanup(&single_manager);
                }
            } else if (strcmp(input, "show_config") == 0) {
                printf("Current Configuration:\n");
                print_config(*config);
            } else if (strncmp(input, "save_config ", 12) == 0) {
                char *filename = input + 12;
                if (save_config(filename, *config) == 0) {
                    printf("Configuration saved to %s\n", filename);
                } else {
                    printf("Failed to save configuration to %s\n", filename);
                }
            } else if (strcmp(input, "help") == 0) {

                open_file("./other/help.txt");
            } else if (strcmp(input, "menu") == 0) {

                open_file("./other/menu.txt");
            } else {
                printf("Invalid arguments. Use `help` for usage information.\n");
            }

            free(input);
        }
}

int run_node_in_cgroup(struct volcom_rcsmngr_s *manager, const char *task_name, const char *script_path, const char *data_point) {

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return -1;
    }

    if (pid == 0) {
        // Child process - execute Node.js script with data point as argument
        printf("Child process %d starting Node.js task: %s with data: %s\n", getpid(), task_name, data_point);
        execlp("node", "node", script_path, data_point, NULL);
        perror("execlp failed - Node.js not found or script error");
        exit(1);
    } else {
        // Parent process - add child to cgroup
        printf("Created child process %d for task '%s'\n", pid, task_name);
        
        // Add the process to the main cgroup
        if (volcom_add_pid_to_cgroup(manager, manager->main_cgroup.name, pid) == 0) {
            printf("Added process %d to main cgroup '%s'\n", pid, manager->main_cgroup.name);
        } else {
            printf("Failed to add process %d to cgroup\n", pid);
        }

        // Wait for the child process to complete
        int status;
        printf("Waiting for Node.js task to complete...\n");
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("Task '%s' completed with exit code %d\n", task_name, exit_code);
            return exit_code;
        } else if (WIFSIGNALED(status)) {
            printf("Task '%s' terminated by signal %d\n", task_name, WTERMSIG(status));
            return -1;
        }
        
        return 0;
    }
}

// Function to process streaming data points
void process_data_stream(struct volcom_rcsmngr_s *manager, const char *script_path) {
    
    const char *data_points[] = {
        "{\"id\":\"sensor_001\", \"type\":\"sensor\", \"value\":75}",
        "{\"id\":\"calc_001\", \"type\":\"calculation\", \"value\":8}",
        "{\"id\":\"sensor_002\", \"type\":\"sensor\", \"value\":120}",
        "{\"id\":\"calc_002\", \"type\":\"calculation\", \"value\":5}",
        "{\"id\":\"sensor_003\", \"type\":\"sensor\", \"value\":45}",
        "{\"id\":\"calc_003\", \"type\":\"calculation\", \"value\":10}",
        NULL  // End marker
    };
    
    printf("\n=== Starting Data Stream Processing ===\n");
    printf("Script: %s\n", script_path);
    printf("Processing data points in cgroup: %s\n\n", manager->main_cgroup.name);
    
    int total_processed = 0;
    int successful = 0;
    int failed = 0;
    
    for (int i = 0; data_points[i] != NULL; i++) {
        printf("--- Processing Data Point %d ---\n", i + 1);
        printf("Data: %s\n", data_points[i]);
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "DataProcessor_%d", i + 1);
        
        int result = run_node_in_cgroup(manager, task_name, script_path, data_points[i]);
        
        if (result == 0) {
            successful++;
            printf("Data point %d processed successfully\n", i + 1);
        } else {
            failed++;
            printf("Data point %d processing failed (exit code: %d)\n", i + 1, result);
        }
        
        total_processed++;
        
        printf("Waiting before next data point...\n");
        sleep(1);
        printf("\n");
    }
    
    printf("=== Data Stream Processing Summary ===\n");
    printf("Total data points: %d\n", total_processed);
    printf("Successful: %d\n", successful);
    printf("Failed: %d\n", failed);
    printf("Success rate: %.1f%%\n", total_processed > 0 ? (successful * 100.0 / total_processed) : 0.0);
    printf("======================================\n");
}

int main(int argc, char *argv[]) {

    struct config_s config = {0};
    char *filename = NULL;

     if (getuid() != 0) {
        printf("[ERRROR][MAIN] This programme requires root privileges. Please run with sudo.\n");
        open_file("./other/help.txt");
        return -1;
    }


    if (argc < 3) {
        printf("[ERRROR][MAIN] Run with the --mode.\n");
        open_file("./other/help.txt");
        return -1;
    }

    // TODO: Add the main programme to the cgroup.
    if (argc == 5 && strcmp(argv[3], "--file") == 0) {
        config = load_config(argv[4]);
        print_config(config);
    } else {
        //TODO: Handle this
        configure_by_cmd(&config);
    }

    if (volcom_rcsmngr_init(&manager, "volcom") != 0) {
        fprintf(stderr, "[ERRROR][MAIN] Failed to initialize resource manager\n");
        return -1;
    }

    if (volcom_create_main_cgroup(&manager, config) != 0) {
        fprintf(stderr, "[ERRROR][MAIN] Failed to create main cgroup\n");
        volcom_rcsmngr_cleanup(&manager);
        return -1;
    }

    if (strcmp(argv[1], "--mode") == 0) {
        if (strcmp(argv[2], "employer") == 0){
            printf("[EMPLOYER] Launching in Employer Mode...\n");
            if (run_employer_mode() != 0) {
                    fprintf(stderr, "[ERRPR][EMPLOYER] Employer mode failed\n");
            }
        } else{
            printf("[EMPLOYEE] Launching in Employee Mode...\n");
            if (run_employee_mode(&manager) != 0) {
                fprintf(stderr, "[ERRPR][EMPLOYEE] Employee mode failed\n");
            }
        }
    }
    
    // TODO: 
    //Cleanup
    volcom_delete_main_cgroup(&manager);
    volcom_rcsmngr_cleanup(&manager);
}