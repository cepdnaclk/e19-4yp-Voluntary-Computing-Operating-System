#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
#define HAVE_PROC_OPS
#endif

#define procfs_name "hellofox"

static void *my_seq_start(struct seq_file *s, loff_t *pos){
	static unsigned long counter = 0;

	if (*pos == 0){
		return &counter;
	}

	*pos = 0;
	return NULL;
}

static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos){
	unsigned long *tmp_v = (unsigned long *)v;
	(*tmp_v)++;
	(*pos)++;
	return NULL;
}

static void my_seq_stop(struct seq_file *s, void *v){

}

static int my_seq_show(struct seq_file *s, void *v){
	loff_t *spos = (loff_t *)v;
	seq_printf(s, "%Ld\n", spos);
	return 0;
}

static struct seq_operations my_seq_ops = {
	.start = my_seq_start,
	.next = my_seq_next,
	.stop = my_seq_stop,
	.show = my_seq_show,
}

static int procfs_open(struct inode *inode, struct file *file){
	return seq_open(file, &my_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops proc_file_fops = {
	.proc_open = procfs_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,	
	.proc_release = seq_release,
};
#else
static const struct file_operations proc_file_fops = {
	.open = procfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

static int __init procfs4_init(void){
	struct proc_dir_entry *entry;
	entry = proc_create(procfs_name, 0, NULL, &proc_file_fops);
	if(NULL == entry){
		pr_alert("Error: Could not initialize /proc/%s\n", procfs_name);
		return -ENOMEM;
	}

	pr_info("/proc/%s created\n", procfs_name);
	return 0;
}

static void __exit procfs4_exit(void){
	remove_proc_entry(procfs_name, NULL);
	pr_info("/proc/%s removed\n", procfs_name);
}

module_init(procfs4_init);
module_exit(procfs4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");