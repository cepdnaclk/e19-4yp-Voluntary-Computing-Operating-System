#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <asm/errno.h>

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write( struct file *, const char __user *, size_t, loff_t *);

#define DEVICE_NAME "chardev"
#define BUF_LEN 80

static int major;

enum {
	CDEV_NOT_USED,
	CDEV_EXCLUSIVE_OPEN,
};

static atomic_t already_open = ATOMIC_INIT(CDEV_EXCLUSIVE_OPEN);

static char msg[BUF_LEN + 1];

static struct class *cls;

static struct file_operations chardev_fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

static int __init chardev_init(void){
	major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

	if (major < 0){
		pr_alert("Registering char device failed with %d\n", major);
		return major;
	}

	pr_info("I was assigned major number %d\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
	cls = class_create(DEVICE_NAME);
#else
	cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif

	device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

	pr_info("Device created on /dev/%s\n", DEVICE_NAME);
	return 0;
}

static void __exit chardev_exit(void){
	device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);

	unregister_chrdev(major, DEVICE_NAME);
}


static int device_open(struct inode *, struct file *){
	 static int counter = 0;

	 if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)){
		return -EBUSY;
	 }

	sprintf(msg, "I already told you %d times Hello Fox!\n", counter++);
	try_module_get(THIS_MODULE);
	return 0;
}

static int device_release(struct inode *, struct file *){
	atomic_set(&already_open, CDEV_NOT_USED);

	module_put(THIS_MODULE);

	return 0;
}

static ssize_t device_read(struct file *, char __user *buffer, size_t length, loff_t *offset){
	int bytes_read = 0;
	const char *msg_ptr = msg;

	if (!*(msg_ptr + *offset)){
		*offset = 0;
		return 0;
	}

	msg_ptr += *offset;

	while(length && *msg_ptr){
		put_user(*(msg_ptr++), buffer++);
		length--;
		bytes_read++;
	}

	*offset += bytes_read;

	return bytes_read;

}

static ssize_t device_write( struct file *, const char __user * buffer, size_t len, loff_t *off){
	pr_alert("Sorry, this operation is not supported\n");

	return -EINVAL;
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");