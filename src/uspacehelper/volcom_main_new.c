#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "volcom_agents/volcom_agents.h"
#include "volcom_utils/volcom_utils.h"
#include "volcom_sysinfo/volcom_sysinfo.h"
#include "volcom_scheduler/volcom_scheduler.h"

static volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    printf("\nReceived signal, shutting down...\n");
    running = 0;
}

void show_menu() {
    printf("\n=== Voluntary Computing Operating System ===\n");
    printf("1. Act as Employer (Offload Tasks)\n");
    printf("2. Act as Employee (Volunteer Resources)\n");
    printf("3. Show System Information\n");
    printf("4. Exit\n");
    printf("Enter choice (1-4): ");
}

// Dummy start_node function for compilation
int start_node(const char* config_path) {
    printf("[Node] start_node called with config: %s. This is a dummy function.\n", config_path);
    // In a real scenario, this would initialize a rendering engine or similar.
    return 0;
}

int main(int argc, char *argv[]) {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Initializing Voluntary Computing System...\n");

    // Initialize utilities
    if (init_volcom_utils() != 0) {
        fprintf(stderr, "Failed to initialize system utilities\n");
        return 1;
    }

    while (running) {
        show_menu();
        int choice;
        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "Invalid input. Please enter a number.\n");
            // Clear input buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        switch (choice) {
            case 1:
                printf("Launching in Employer Mode...\n");
                init_agent(AGENT_MODE_EMPLOYER);
                if (run_employer_mode(NULL) != 0) {
                    fprintf(stderr, "Employer mode failed\n");
                }
                cleanup_agent();
                break;
            case 2:
                printf("Launching in Employee Mode...\n");
                init_agent(AGENT_MODE_EMPLOYEE);
                run_employee_mode();
                cleanup_agent();
                break;
            case 3:
                show_system_info();
                break;
            case 4:
                printf("Exiting...\n");
                running = 0;
                break;
            default:
                fprintf(stderr, "Invalid choice. Please enter 1-4.\n");
                break;
        }
        if (choice != 4 && running) {
            printf("\nPress Enter to continue...");
            getchar(); // consume newline
            getchar(); // wait for user input
        }
    }
    cleanup_volcom_utils();
    printf("Voluntary Computing System shutdown complete.\n");
    return 0;
}
