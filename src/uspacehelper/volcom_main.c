#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "volcom_sysinfo/volcom_sysinfo.h"
#include "volcom_rcsmngr/volcom_rcsmngr.h"

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
                        printf("%d Mb of memory allocated for volcom\n", value);
                    } else if (strcmp(key, "cpu") == 0) {
                        printf("%d cores allocated for volcom\n", value);
                    } else if (strcmp(key, "gpu") == 0) {
                        printf("%d Mb of gpu memory allocated for volcom\n", value);
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

int main(int argc, char *argv[]) {

    struct volcom_rcsmngr_s manager;
    struct config_s config = {0};

     if (getuid() != 0) {
        printf("This programme requires root privileges. Please run with sudo.\n");
        return 1;
    }

    if (argc == 3 && strcmp(argv[1], "--file") == 0) {

        config = load_config(argv[2]);
        print_config(config);

        if (volcom_rcsmngr_init(&manager, "volcom") != 0) {
            fprintf(stderr, "Failed to initialize resource manager\n");
            return 1;
        }

        if (volcom_create_main_cgroup(&manager, config) != 0) {
            fprintf(stderr, "Failed to create main cgroup\n");
            volcom_rcsmngr_cleanup(&manager);
            return 1;
        }

        volcom_print_cgroup_info(&manager);

        //Cleanup
        volcom_delete_main_cgroup(&manager);
        volcom_rcsmngr_cleanup(&manager);

        return 0;
    } else {

        configure_by_cmd(&config);
        return 0;
    }
}