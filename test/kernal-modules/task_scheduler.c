#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include "employee_list.h"
#include "dummy_tasks.h"

#define PROCFS_NAME "volcom_scheduler"

static struct proc_dir_entry *proc_file;

#define MAX_PROC_SIZE 1024
static char procfs_buffer[MAX_PROC_SIZE];
static unsigned long procfs_buffer_size = 0;

static ssize_t proc_write(struct file *file, const char __user *buffer,
                          size_t count, loff_t *pos) {
    procfs_buffer_size = count;
    if (procfs_buffer_size > MAX_PROC_SIZE)
        procfs_buffer_size = MAX_PROC_SIZE;

    if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size))
        return -EFAULT;

    procfs_buffer[procfs_buffer_size] = '\0';

    if (strncmp(procfs_buffer, "schedule", 8) == 0) {
        printk(KERN_INFO "volcom: Task scheduling started...\n");

        EmployeeNodeWrapper *device = get_candidate_list();

        if (!device) {
            printk(KERN_INFO "volcom: No candidate devices found.\n");
            return count;
        }

        for (int i = 0; i < global_task_count; ++i) {
            task_info_s task = global_task_list[i];

            // Choose best device (least CPU usage and enough memory)
            EmployeeNodeWrapper *best = NULL;
            unsigned int best_score = 999999;  // large number for initial comparison

            EmployeeNodeWrapper *curr = device;
            while (curr) {
                if (curr->data.free_mem_mb >= task.memory_usage) {
                    unsigned int score = (unsigned int)curr->data.cpu_usage_percent;
                    if (score < best_score) {
                        best_score = score;
                        best = curr;
                    }
                }
                curr = curr->next;
            }

            if (best) {
                printk(KERN_INFO "volcom: Assigned Task[%d] '%s' to Device[%s] (CPU %lu%%, MEM %luMB)\n",
                       task.task_id, task.task_name,
                       best->data.ip,
                       best->data.cpu_usage_percent,
                       best->data.free_mem_mb);
            } else {
                printk(KERN_INFO "volcom: Could not assign Task[%d] '%s' — no suitable device.\n",
                       task.task_id, task.task_name);
            }
        }

        // Free device list memory
        while (device) {
            EmployeeNodeWrapper *temp = device;
            device = device->next;
            kfree(temp);
        }

        return count;
    }

    printk(KERN_INFO "volcom: Unknown command in /proc file: %s\n", procfs_buffer);
    return count;
}

static const struct proc_ops proc_file_ops = {
    .proc_write = proc_write
};

static int __init scheduler_init(void) {
    proc_file = proc_create(PROCFS_NAME, 0666, NULL, &proc_file_ops);
    if (!proc_file) {
        printk(KERN_ALERT "volcom: Failed to create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "volcom: Task Scheduler Module Loaded\n");
    return 0;
}

static void __exit scheduler_exit(void) {
    proc_remove(proc_file);
    printk(KERN_INFO "volcom: Task Scheduler Module Removed\n");
}

module_init(scheduler_init);
module_exit(scheduler_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Dummy Task Scheduler Kernel Module");