#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/export.h>   // <-- Needed for EXPORT_SYMBOL

#include "employee_list.h"

EmployeeNodeWrapper* get_candidate_list() {
    EmployeeNodeWrapper *head = NULL, *temp = NULL;

    // Device 1
    temp = kmalloc(sizeof(EmployeeNodeWrapper), GFP_KERNEL);
    if (!temp) return NULL;
    strcpy(temp->data.ip, "192.168.1.10");
    temp->data.free_mem_mb = 4096;
    temp->data.cpu_usage_percent = 25;
    temp->data.logical_cores = 4;
    temp->data.last_seen = ktime_get_real_seconds();
    temp->next = head;
    head = temp;

    // Device 2
    temp = kmalloc(sizeof(EmployeeNodeWrapper), GFP_KERNEL);
    if (!temp) return head;
    strcpy(temp->data.ip, "192.168.1.11");
    temp->data.free_mem_mb = 2048;
    temp->data.cpu_usage_percent = 10;
    temp->data.logical_cores = 2;
    temp->data.last_seen = ktime_get_real_seconds();
    temp->next = head;
    head = temp;

    return head;
}

EXPORT_SYMBOL(get_candidate_list);

MODULE_LICENSE("GPL");