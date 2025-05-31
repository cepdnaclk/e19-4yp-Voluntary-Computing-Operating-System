#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include <linux/minmax.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
#define HAVE_PROC_OPS
#endif

#define procfs_max_size 2048UL
#define procfs_name "hellofox"

static struct proc_dir_entry *our_proc_file;

static char procfs_buffer[procfs_max_size];

static unsigned long procfs_buffer_size = 0;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t buffer_length, loff_t *offset){
	if (*offset || procfs_buffer_size == 0){
		pr_debug("procfs_read: END\n");
		*offset = 0;
		return 0;
	}

	procfs_buffer_size = min(procfs_buffer_size, buffer_length);
	if (copy_to_user(buffer, procfs_buffer, procfs_buffer_size)){
		return -EFAULT;
	}

	*offset += procfs_buffer_size;

	pr_debug("procfs_read: read %lu bytes\n", procfs_buffer_size);
	return procfs_buffer_size;
}

static ssize_t procfile_write(struct file *file, const char __user *buffer, size_t len, loff_t *off){
	procfs_buffer_size = min(procfs_max_size, len);

	if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size)){
		return -EFAULT;
	}

	*off += procfs_buffer_size;

	pr_debug("procfile_write: write %lu bytes\n", procfs_buffer_size);

	return procfs_buffer_size;
}

static int procfs_open(struct inode *inode, struct file *file){
	try_module_get(THIS_MODULE);
	return 0;
}

static int procfs_close(struct inode *inode, struct file *file){
	module_put(THIS_MODULE);
	return 0;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops proc_file_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
	.proc_open = procfs_open,
	.proc_release = procfs_close,
};
#else
static const struct file_operations proc_file_fops = {
	.read = procfile_read,
	.write = procfile_write,
	.open = procfs_open,
	.release = procfs_close,
};
#endif

static int __init procfs3_init(void){
	our_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops);
	if(NULL == our_proc_file){
		pr_alert("Error: Could not initialize /proc/%s\n", procfs_name);
		return -ENOMEM;
	}

	proc_set_size(our_proc_file, 80);
	proc_set_user(our_proc_file, GLOBAL_ROOT_UID, GLOBAL_ROOT_GID);

	pr_info("/proc/%s created\n", procfs_name);
	return 0;
}

static void __exit procfs3_exit(void){
	remove_proc_entry(procfs_name, NULL);
	pr_info("/proc/%s removed\n", procfs_name);
}

module_init(procfs3_init);
module_exit(procfs3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");