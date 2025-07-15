#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define MAX_DEVICES 3
#define PROCFS_NAME "volcom_scheduler"
#define MAX_INPUT 128

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SPM");
MODULE_DESCRIPTION("Task Scheduler with procfs command handling");
MODULE_VERSION("0.5");

enum task_state { QUEUED, RUNNING, COMPLETED };

struct device_info {
    int id;
    int load_percent;
};

struct task {
    int task_id;
    int assigned_device;
    enum task_state state;
    struct list_head list;
};

static struct device_info devices[MAX_DEVICES];
static LIST_HEAD(task_list);
static struct proc_dir_entry *proc_file;
static int task_counter = 0;

/* ==== Device Simulation ==== */
static void simulate_single_device(int i) {
    devices[i].id = i;
    get_random_bytes(&devices[i].load_percent, sizeof(int));
    devices[i].load_percent %= 101;
}

static void simulate_device_status(void) {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        simulate_single_device(i);
    }
}

static int find_least_loaded_device(void) {
    int i, min = 101, selected = -1;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].load_percent < min) {
            min = devices[i].load_percent;
            selected = i;
        }
    }
    return selected;
}

/* ==== Task Handling ==== */
static void schedule_task(void) {
    struct task *new_task = kmalloc(sizeof(struct task), GFP_KERNEL);
    if (!new_task) return;

    new_task->task_id = task_counter++;
    new_task->assigned_device = find_least_loaded_device();
    new_task->state = QUEUED;
    list_add_tail(&new_task->list, &task_list);

    printk(KERN_INFO "Task %d assigned to device %d [QUEUED]\n",
           new_task->task_id, new_task->assigned_device);
}

/* ==== procfs write handler ==== */
static ssize_t scheduler_write(struct file *file, const char __user *buffer,
                               size_t count, loff_t *ppos) {
    char input[MAX_INPUT];

    if (count >= MAX_INPUT)
        return -EINVAL;

    if (copy_from_user(input, buffer, count))
        return -EFAULT;

    input[count] = '\0';

    if (strncmp(input, "submit", 6) == 0) {
        schedule_task();
    } else if (strncmp(input, "list", 4) == 0) {
        struct task *t;
        list_for_each_entry(t, &task_list, list) {
            printk(KERN_INFO "Task %d on Device %d [%s]\n",
                   t->task_id, t->assigned_device,
                   (t->state == QUEUED ? "QUEUED" :
                    t->state == RUNNING ? "RUNNING" : "COMPLETED"));
        }
    } else if (strncmp(input, "clear", 5) == 0) {
        struct task *t, *tmp;
        list_for_each_entry_safe(t, tmp, &task_list, list) {
            printk(KERN_INFO "Clearing Task %d\n", t->task_id);
            list_del(&t->list);
            kfree(t);
        }
        task_counter = 0;
    } else {
        printk(KERN_INFO "Invalid command. Use 'submit', 'list', or 'clear'\n");
    }

    return count;
}

static const struct proc_ops proc_file_ops = {
    .proc_write = scheduler_write,
};

/* ==== Module Init/Exit ==== */
static int __init scheduler_init(void) {
    printk(KERN_INFO "Task Scheduler with procfs command interface\n");
    simulate_device_status();

    proc_file = proc_create(PROCFS_NAME, 0666, NULL, &proc_file_ops);
    if (!proc_file) {
        printk(KERN_ERR "Failed to create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    return 0;
}

static void __exit scheduler_exit(void) {
    struct task *t, *tmp;
    list_for_each_entry_safe(t, tmp, &task_list, list) {
        list_del(&t->list);
        kfree(t);
    }

    if (proc_file)
        proc_remove(proc_file);

    printk(KERN_INFO "Task Scheduler cleaned up\n");
}

module_init(scheduler_init);
module_exit(scheduler_exit);

