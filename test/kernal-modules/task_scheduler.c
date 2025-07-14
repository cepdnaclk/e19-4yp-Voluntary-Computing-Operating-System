#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/list.h>

#define MAX_DEVICES 3

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SPM");
MODULE_DESCRIPTION("Task Scheduler - Basic Scheduling Logic");
MODULE_VERSION("0.2");

struct device_info {
    int id;
    int load_percent;
};

struct task {
    int task_id;
    int assigned_device;
    struct list_head list;
};

static LIST_HEAD(task_list);
static struct device_info devices[MAX_DEVICES];
static int task_counter = 0;

static void simulate_single_device(int i) {
    devices[i].id = i;
    get_random_bytes(&devices[i].load_percent, sizeof(int));
    devices[i].load_percent %= 101;
}

static void simulate_device_status(void) {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        simulate_single_device(i);
        printk(KERN_INFO "Device %d: Load = %d%%\n", i, devices[i].load_percent);
    }
}

static int find_least_loaded_device(void) {
    int i, min = 101, chosen = -1;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].load_percent < min) {
            min = devices[i].load_percent;
            chosen = i;
        }
    }
    return chosen;
}

static void schedule_task(void) {
    struct task *new_task = kmalloc(sizeof(struct task), GFP_KERNEL);
    if (!new_task) return;

    new_task->task_id = task_counter++;
    new_task->assigned_device = find_least_loaded_device();
    list_add_tail(&new_task->list, &task_list);

    printk(KERN_INFO "Task %d assigned to device %d\n", new_task->task_id, new_task->assigned_device);
}

static int __init scheduler_init(void) {
    printk(KERN_INFO "Scheduler module init\n");
    simulate_device_status();
    schedule_task();
    return 0;
}

static void __exit scheduler_exit(void) {
    struct task *t, *tmp;
    list_for_each_entry_safe(t, tmp, &task_list, list) {
        printk(KERN_INFO "Freeing task %d\n", t->task_id);
        list_del(&t->list);
        kfree(t);
    }
    printk(KERN_INFO "Scheduler module exit\n");
}

module_init(scheduler_init);
module_exit(scheduler_exit);
