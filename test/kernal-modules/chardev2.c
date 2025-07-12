#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include<asm/errno.h>

#include "../headers/chardev.h"

#define BUF_LEN 80
#define DEVICE_NAME "char_dev"

enum {
	CDEV_NOT_USED,
	CDEV_EXCLUSIVE_OPEN,
};

static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static char msg[BUF_LEN + 1];

static struct class *cls;

static int device_open(struct inode *inode, struct file *file){
	
	pr_info("device_open(%p)\n", file);
	try_module_get(THIS_MODULE);
	return 0;
}

static int device_release(struct inode *inode, struct file *file){
	pr_info("device_release(%p, %p)\n", inode, file);
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

	pr_info("Read %d bytes, %ld, left\n", bytes_read, length);

	*offset += bytes_read;

	return bytes_read;

}

static ssize_t device_write( struct file *file, const char __user * buffer, size_t length, loff_t *off){
	int i;

	pr_info("device_write(%p, %p, %ld)", file, buffer, length);

	for (i = 0; i < length && i < BUF_LEN; i++){
		get_user(msg[i], buffer + i);
	}

	return i;
}


static long device_ioctl(struct file *file, unsigned int ioct_num, unsigned long ioctl_param){
	int i;
	long ret = 0;

	if(atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)){
		return -EBUSY;
	}

	switch(ioct_num){
		case IOCTL_SET_MSG:
			char __user *tmp = (char __user *)ioctl_param;
			char ch;

			get_user(ch, tmp);
			for(i = 0; ch && i < BUF_LEN; i++, tmp++){
				get_user(ch, tmp);
			}
			device_write(file, (char __user *)ioctl_param, i, NULL);

			break;

		case IOCTL_GET_MSG:
			loff_t offset = 0;

			i = device_read(file, (char __user *) ioctl_param, BUF_LEN - 1, &offset);

			put_user('\0', (char __user *)ioctl_param + 1);

			break;

		case IOCTL_GET_NTH_BYTE:
			ret = (long)msg[ioctl_param];
			break;
		
	}

	atomic_set(&already_open, CDEV_NOT_USED);
	return ret;
}

static struct file_operations fpos = {
	.read = device_read,
	.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release,
};

static int __init chardev2_init(void){
	int ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fpos);

	if (ret_val < 0){
		pr_alert("%s failed with %d\n",
		"Sorry, registering the chaacter device ", ret_val);
		return ret_val;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
	cls = class_create(DEVICE_FILE_NAME);
#else
	cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
#endif

	device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);

	pr_info("Device created on /dev/%s\n", DEVICE_FILE_NAME);
	return 0;
}

static void __exit chardev2_exit(void){
	device_destroy(cls, MKDEV(MAJOR_NUM, 0));
	class_destroy(cls);

	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

module_init(chardev2_init);
module_exit(chardev2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");