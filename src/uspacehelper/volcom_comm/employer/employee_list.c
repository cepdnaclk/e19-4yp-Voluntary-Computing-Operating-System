// employee_list.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "employee_list.h"

static EmployeeNodeWrapper *head = NULL;

void cleanup_old_employees(int threshold_seconds) {
    time_t now = time(NULL);
    EmployeeNodeWrapper *curr = head;
    EmployeeNodeWrapper *prev = NULL;

    while (curr != NULL) {
        double elapsed = difftime(now, curr->data.last_seen);
        if (elapsed > threshold_seconds) {
            // Remove this node
            if (prev == NULL) {
                head = curr->next;
                free(curr);
                curr = head;
            } else {
                prev->next = curr->next;
                free(curr);
                curr = prev->next;
            }
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void add_or_update_employee(EmployeeNode new_node) {
    EmployeeNodeWrapper *curr = head;

    // Check if already exists
    while (curr) {
        if (strcmp(curr->data.ip, new_node.ip) == 0) {
            // Update existing
            curr->data = new_node;
            curr->data.last_seen = time(NULL);
            return;
        }
        curr = curr->next;
    }

    // Add new
    EmployeeNodeWrapper *new_entry = (EmployeeNodeWrapper *)malloc(sizeof(EmployeeNodeWrapper));
    if (!new_entry) return;

    new_entry->data = new_node;
    new_entry->data.last_seen = time(NULL);
    new_entry->next = head;
    head = new_entry;
}

EmployeeNodeWrapper* get_candidate_list() {
    return head;
}
