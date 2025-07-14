#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROCFS_NAME "volcom_scheduler"
#define MAX_INPUT 128

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SPM");
MODULE_DESCRIPTION("Task Scheduler with procfs interface");
MODULE_VERSION("0.4");

static struct proc_dir_entry *proc_file;

/* procfs write handler */
static ssize_t scheduler_write(struct file *file, const char __user *buffer,
                               size_t count, loff_t *ppos) {
    char input[MAX_INPUT];

    if (count >= MAX_INPUT)
        return -EINVAL;

    if (copy_from_user(input, buffer, count))
        return -EFAULT;

    input[count] = '\0'; 
    printk(KERN_INFO "Received via /proc: %s", input);

    return count;
}

static const struct proc_ops proc_file_ops = {
    .proc_write = scheduler_write,
};

static int __init scheduler_init(void) {
    printk(KERN_INFO "Initializing procfs interface\n");

    proc_file = proc_create(PROCFS_NAME, 0666, NULL, &proc_file_ops);
    if (!proc_file) {
        printk(KERN_ERR "Failed to create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);
    return 0;
}

static void __exit scheduler_exit(void) {
    if (proc_file)
        proc_remove(proc_file);

    printk(KERN_INFO "Removed /proc/%s\n", PROCFS_NAME);
}

module_init(scheduler_init);
module_exit(scheduler_exit);
