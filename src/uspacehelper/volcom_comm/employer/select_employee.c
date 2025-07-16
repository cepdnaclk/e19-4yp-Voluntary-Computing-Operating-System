#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "select_employee.h"

// Check if input IP exists in employee list
int is_valid_ip(const char *ip, EmployeeNodeWrapper *list) {
    while (list) {
        if (strcmp(ip, list->data.ip) == 0) {
            return 1;
        }
        list = list->next;
    }
    return 0;
}

char *select_employee_ip(EmployeeNodeWrapper *list) {
    char input[64];

    printf("\n=== Select an Employee ===\n");
    int count = 0;
    EmployeeNodeWrapper *curr = list;
    while (curr) {
        printf("  [%d] %s | CPU: %.2f%% | Free Mem: %.2f MB | Cores: %d\n",
               count + 1,
               curr->data.ip,
               curr->data.cpu_usage_percent,
               curr->data.free_mem_mb,
               curr->data.logical_cores);
        count++;
        curr = curr->next;
    }

    if (count == 0) {
        printf("No employees to select from.\n");
        return NULL;
    }

    while (1) {
        printf("Enter IP of the employee to select: ");
        if (!fgets(input, sizeof(input), stdin)) continue;

        // Strip newline
        input[strcspn(input, "\n")] = 0;

        if (is_valid_ip(input, list)) {
            return strdup(input); // Caller must free
        } else {
            printf("Invalid IP. Please enter one from the list above.\n");
        }
    }
}
