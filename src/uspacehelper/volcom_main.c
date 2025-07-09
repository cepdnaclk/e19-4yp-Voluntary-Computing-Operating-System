#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "volcom_sysinfo/volcom_sysinfo.h"

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

int main(int argc, char *argv[]) {
    printf("argv %s %d\n", argv[1], argc);

    if (argc == 3 && strcmp(argv[1], "--file") == 0) {
        FILE *config = fopen(argv[2], "r");

        // Extract configurations

        fclose(config);
        return 0;
    } else {
        while (1) {
            char *input = malloc(256);
            printf("Enter configuration (or 'q' to quit or 'menu' for menu): ");
            fgets(input, 256, stdin);
            input[strcspn(input, "\n")] = 0; 

            if (strcmp(input, "q") == 0) {
                free(input);
                break;
            } 

            if (strcmp(input, "config") == 0) {

            } else if (strcmp(input, "help") == 0) {

                open_file("./other/help.txt");
            } else if (strcmp(input, "menu") == 0) {

                open_file("./other/menu.txt");
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
            } else {
                printf("Invalid arguments. Use `help` for usage information.\n");
            }
            free(input);
        }

        return 0;
    }
}