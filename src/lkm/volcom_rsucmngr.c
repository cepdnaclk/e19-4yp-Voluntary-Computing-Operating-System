/*
 * Kernel Module with Cgroup v2 Creation and Limit Setting
 * - Allocates memory and CPU to tasks based on ioctl input
 * - Attaches processes to new cgroups under /sys/fs/cgroup/resgroup
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/fs_context.h>

#define DEVICE_NAME "resalloc"
#define CLASS_NAME  "resman"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");
MODULE_DESCRIPTION("Kernel module for resource allocation using cgroups");

#define TASK_NAME_LEN 64

struct task_config {
    char task_name[TASK_NAME_LEN];
    int memory_limit_mb;
    int cpu_share; // In microseconds per 100ms (e.g. 20000 = 20%)
    pid_t pid; // Process ID to assign
};

#define IOCTL_ALLOCATE_TASK _IOW('k', 1, struct task_config)

static struct kobject *resman_kobj;
static char cgroup_base_path[128] = "/sys/fs/cgroup/resgroup";

static ssize_t path_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%s\n", cgroup_base_path);
}

static struct kobj_attribute path_attr = __ATTR_RO(path);

// Helper to write to cgroup files
static int write_cgroup_file(const char *path, const char *filename, const char *value) {
    struct file *file;
    char fullpath[256];
    ssize_t written;
    loff_t pos = 0;

    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    file = filp_open(fullpath, O_WRONLY | O_CREAT, 0644);
    if (IS_ERR(file)) {
        pr_err("[resman] Failed to open file: %s\n", fullpath);
        return PTR_ERR(file);
    }

    written = kernel_write(file, value, strlen(value), &pos);
    filp_close(file, NULL);
    return (written > 0) ? 0 : -EIO;
}

static long resman_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct task_config config;
    char task_path[256];
    char mem_limit[32];
    char cpu_limit[32];
    char pid_str[16];
    int ret;

    if (cmd != IOCTL_ALLOCATE_TASK)
        return -EINVAL;

    if (copy_from_user(&config, (void __user *)arg, sizeof(config)))
        return -EFAULT;

    snprintf(task_path, sizeof(task_path), "%s/%s", cgroup_base_path, config.task_name);

    // Check if the directory exists
    ret = kern_path(task_path, LOOKUP_DIRECTORY, NULL);
    if (ret != 0) {
        pr_err("[resman] Cgroup directory does not exist: %s\n", task_path);
        return -ENOENT;
    }

    // Set memory limit in megabytes
    snprintf(mem_limit, sizeof(mem_limit), "%dM", config.memory_limit_mb);
    ret = write_cgroup_file(task_path, "memory.max", mem_limit);
    if (ret < 0) {
        pr_err("[resman] Failed to write memory limit\n");
        return ret;
    }

    // Set CPU limit if provided
    if (config.cpu_share > 0) {
        snprintf(cpu_limit, sizeof(cpu_limit), "%d 100000", config.cpu_share);
        ret = write_cgroup_file(task_path, "cpu.max", cpu_limit);
        if (ret < 0) {
            pr_err("[resman] Failed to write CPU limit\n");
            return ret;
        }
    }

    // Assign process to cgroup
    snprintf(pid_str, sizeof(pid_str), "%d", config.pid);
    ret = write_cgroup_file(task_path, "cgroup.procs", pid_str);
    if (ret < 0) {
        pr_err("[resman] Failed to assign PID to cgroup\n");
        return ret;
    }

    pr_info("[resman] Task %s: memory=%s, cpu=%dus, pid=%d\n",
            config.task_name, mem_limit, config.cpu_share, config.pid);
    return 0;
}

static const struct file_operations resman_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = resman_ioctl,
};

static struct miscdevice resman_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &resman_fops,
    .mode = 0666,
};

static int __init resman_init(void) {
    int ret;

    ret = misc_register(&resman_dev);
    if (ret) {
        pr_err("[resman] Failed to register misc device\n");
        return ret;
    }

    resman_kobj = kobject_create_and_add(CLASS_NAME, kernel_kobj);
    if (!resman_kobj) {
        misc_deregister(&resman_dev);
        return -ENOMEM;
    }

    if (sysfs_create_file(resman_kobj, &path_attr.attr)) {
        kobject_put(resman_kobj);
        misc_deregister(&resman_dev);
        return -ENOMEM;
    }

    pr_info("[resman] Module loaded. Base Cgroup path: %s\n", cgroup_base_path);
    return 0;
}

static void __exit resman_exit(void) {
    sysfs_remove_file(resman_kobj, &path_attr.attr);
    kobject_put(resman_kobj);
    misc_deregister(&resman_dev);
    pr_info("[resman] Module unloaded\n");
}

module_init(resman_init);
module_exit(resman_exit);
