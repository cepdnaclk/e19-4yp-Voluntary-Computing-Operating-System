#include <linux/module.h>
#include <linux/export.h>      // Required for EXPORT_SYMBOL
#include "dummy_tasks.h"

task_info_s global_task_list[] = {
    {0, "Task A", 1, 512, 20, "NEW", {"192.168.1.10"}},
    {1, "Task B", 2, 1024, 30, "NEW", {"192.168.1.11"}}
};

int global_task_count = 2;

EXPORT_SYMBOL(global_task_list);   // <-- Add this
EXPORT_SYMBOL(global_task_count);  // <-- And this

MODULE_LICENSE("GPL");

