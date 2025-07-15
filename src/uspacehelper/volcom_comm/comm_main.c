#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "employee/employee_mode.h"
#include "employer/employer_mode.h"

void show_menu() {
    printf("\n=== Voluntary Computing Node ===\n");
    printf("1. Act as Employer (Offload Tasks)\n");
    printf("2. Act as Employee (Volunteer Resources)\n");
    printf("Enter choice (1 or 2): ");
}

int main() {
    int choice;
    show_menu();

    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Invalid input. Exiting.\n");
        return 1;
    }

    switch (choice) {
        case 1:
            printf("Launching in Employer Mode...\n");
            run_employer_mode();  // defined in employer_mode.c
            break;
        case 2:
            printf("Launching in Employee Mode...\n");
            run_employee_mode();  // defined in employee_mode.c
            break;
        default:
            fprintf(stderr, "Invalid choice. Please enter 1 or 2.\n");
            return 1;
    }

    return 0;
}
