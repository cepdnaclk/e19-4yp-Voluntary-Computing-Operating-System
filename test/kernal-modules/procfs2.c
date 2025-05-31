#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
#define HAVE_PROC_OPS
#endif

#define procfs_max_size 1024
#define procfs_name "hellofox"

static struct proc_dir_entry *our_proc_file;

static char procfs_buffer[procfs_max_size];

static unsigned long procfs_buffer_size = 0;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t buffer_length, loff_t *offset){
	char s[12] = "Hello Fox!\n";
	int len = sizeof(s);
	ssize_t ret = len;
	
	if (*offset >= len || copy_to_user(buffer, s, len)){
		pr_info("End of file reached or copy_to_user failed.\n");
		ret = 0;
	} else{
		pr_info("procfile read %s\n", file_pointer->f_path.dentry->d_name.name);
		*offset += len;
	}

	return ret;
}

static ssize_t procfile_write(struct file *file, const char __user *buffer, size_t len, loff_t *off){
	procfs_buffer_size = len;
	if(procfs_buffer_size >= procfs_max_size){
		procfs_buffer_size = procfs_max_size - 1;
	}

	if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size)){
		return -EFAULT;
	}

	procfs_buffer[procfs_buffer_size] = '\0';
	*off += procfs_buffer_size;
	pr_info("procfile write %s\n", procfs_buffer);
	// sprintf(procfs_buffer, "procfile write\n");

	return procfs_buffer_size;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops proc_file_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
};
#else
static const struct file_operations proc_file_fops = {
	.read = procfile_read,
	.write = procfile_write,
};
#endif

static int __init procfs2_init(void){
	our_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops);
	if(NULL == our_proc_file){
		pr_alert("Error: Could not initialize /proc/%s\n", procfs_name);
		return -ENOMEM;
	}

	pr_info("/proc/%s created\n", procfs_name);
	return 0;
}

static void __exit procfs2_exit(void){
	proc_remove(our_proc_file);
	pr_info("/proc/%s removed\n", procfs_name);
}

module_init(procfs2_init);
module_exit(procfs2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");