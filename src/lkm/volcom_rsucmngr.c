/*
 * Kernel Module with Cgroup Creation and Limit Setting
 * - Allocates memory/cpu to tasks based on ioctl input
 * - Creates and configures cgroups under /sys/fs/cgroup
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
#include <linux/kernfs.h>
#include <linux/cred.h>
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
    int cpu_share; // optional for now
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
    mm_segment_t old_fs;
    char fullpath[256];
    ssize_t written;

    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    file = filp_open(fullpath, O_WRONLY | O_CREAT, 0644);
    if (IS_ERR(file)) {
        set_fs(old_fs);
        pr_err("[resman] Failed to open file: %s\n", fullpath);
        return PTR_ERR(file);
    }
    written = kernel_write(file, value, strlen(value), &file->f_pos);
    filp_close(file, NULL);
    set_fs(old_fs);
    return (written > 0) ? 0 : -EIO;
}

static long resman_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct task_config config;
    char task_path[256];
    char mem_limit[32];
    int ret;

    if (cmd != IOCTL_ALLOCATE_TASK)
        return -EINVAL;

    if (copy_from_user(&config, (void __user *)arg, sizeof(config)))
        return -EFAULT;

    snprintf(task_path, sizeof(task_path), "%s/%s", cgroup_base_path, config.task_name);

    // Create directory
    ret = sys_mkdir(task_path, 0755);
    if (ret < 0 && ret != -EEXIST) {
        pr_err("[resman] Failed to create cgroup directory: %s\n", task_path);
        return ret;
    }

    // Set memory limit
    snprintf(mem_limit, sizeof(mem_limit), "%dM", config.memory_limit_mb);
    ret = write_cgroup_file(task_path, "memory.max", mem_limit);
    if (ret < 0) {
        pr_err("[resman] Failed to write memory limit\n");
        return ret;
    }

    pr_info("[resman] Task %s allocated %s memory\n", config.task_name, mem_limit);
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
