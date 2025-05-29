#include <linux/module.h>
#include <linux/printk.h>

int init_module(void){
	pr_info("Hello Fox. \n");

	return 0;
}

void cleanup_module(void){
	pr_info("Goodbye Fox. \n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");