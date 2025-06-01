#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/random.h>

#define MAX_DEVICES 3
#define MAX_TASKS 5

struct device_info {
    int id;
    int load_percent; // Simulated load (0-100)
};

struct task {
    int task_id;
    int assigned_device;
    struct list_head list;
};

static struct task *task_queue_head;
static LIST_HEAD(task_list);

static struct device_info devices[MAX_DEVICES];

// Simulate load for a single device
static void simulate_single_device(int i) {
    devices[i].id = i;
    get_random_bytes(&devices[i].load_percent, sizeof(int));
    devices[i].load_percent = devices[i].load_percent % 101; 
}

// Simulate random load for each device
static void simulate_device_status(void) {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        simulate_single_device(i);
        printk(KERN_INFO "Device %d: Load = %d%%\n", i, devices[i].load_percent);
    }
}

static int find_least_loaded_device(void) {
    int i, min_load = 101, selected = -1;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].load_percent < min_load) {
            min_load = devices[i].load_percent;
            selected = i;
        }
    }
    return selected;
}

static void enqueue_tasks(void) {
    int i;
    for (i = 0; i < MAX_TASKS; i++) {
        struct task *new_task = kmalloc(sizeof(struct task), GFP_KERNEL);
        new_task->task_id = i;
        new_task->assigned_device = find_least_loaded_device();
        list_add_tail(&new_task->list, &task_list);
        printk(KERN_INFO "Task %d assigned to Device %d\n", new_task->task_id, new_task->assigned_device);
    }
}

static void clear_task_queue(void) {
    struct task *t, *tmp;
    list_for_each_entry_safe(t, tmp, &task_list, list) {
        printk(KERN_INFO "Removing Task %d\n", t->task_id);
        list_del(&t->list);
        kfree(t);
    }
}

static int __init scheduler_init(void) {
    printk(KERN_INFO "Initializing Task Scheduler Module...\n");
    simulate_device_status();
    enqueue_tasks();
    return 0;
}

static void __exit scheduler_exit(void) {
    printk(KERN_INFO "Exiting Task Scheduler Module...\n");
    clear_task_queue();
}

module_init(scheduler_init);
module_exit(scheduler_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SPM");
MODULE_DESCRIPTION("Kernel-Level Task Scheduler Prototype");
MODULE_VERSION("0.1");

