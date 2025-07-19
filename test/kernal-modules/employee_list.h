#ifndef EMPLOYEE_LIST_H
#define EMPLOYEE_LIST_H

#include <linux/time.h>  // For time64_t

typedef struct {
    char ip[32];
    unsigned long free_mem_mb;        // changed from double to unsigned long
    unsigned long mem_usage_percent;  // changed from double to unsigned long
    unsigned long cpu_usage_percent;  // changed from double to unsigned long
    char cpu_model[256];
    int logical_cores;
    time64_t last_seen;               // already kernel-safe
} EmployeeNode;

typedef struct EmployeeNodeWrapper {
    EmployeeNode data;
    struct EmployeeNodeWrapper *next;
} EmployeeNodeWrapper;

EmployeeNodeWrapper* get_candidate_list(void);  // Good practice to use void

#endif



