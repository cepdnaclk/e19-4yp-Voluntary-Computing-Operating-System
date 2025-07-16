// employee_list.h
#ifndef EMPLOYEE_LIST_H
#define EMPLOYEE_LIST_H

#include <time.h>

typedef struct {
    char ip[32];
    double free_mem_mb;
    double mem_usage_percent;
    double cpu_usage_percent;
    char cpu_model[256];
    int logical_cores;
    time_t last_seen;
} EmployeeNode;

typedef struct EmployeeNodeWrapper {
    EmployeeNode data;
    struct EmployeeNodeWrapper *next;
} EmployeeNodeWrapper;

void add_or_update_employee(EmployeeNode new_node);
EmployeeNodeWrapper* get_candidate_list();

void cleanup_old_employees(int threshold_seconds);


#endif
