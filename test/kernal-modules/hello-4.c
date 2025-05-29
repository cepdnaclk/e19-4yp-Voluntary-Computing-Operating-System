#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/stat.h>

static short int myshort = 1;
static int myint = 420;
static long int mylong = 9999;
static char *mystring = "Hello Fox";
static int myintarray[3] = {1, 2, 3};
static int arr_argc = 0;

module_param(myshort, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(myshort, "A short integer parameter");
module_param(myint, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(myint, "An integer parameter");
module_param(mylong, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mylong, "A long integer parameter");
module_param(mystring, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mystring, "A string parameter");

module_param_array(myintarray, int, &arr_argc, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(myintarray, "An array of integers");

static int __init hello_init(void){
	int i;

	pr_info("Hello Fox. \n");
	pr_info("Short: %hd\n", myshort);
	pr_info("Int: %d\n", myint);
	pr_info("Long: %ld\n", mylong);
	pr_info("String: %s\n", mystring);

	for (i = 0; i < ARRAY_SIZE(myintarray); i++) {
		pr_info("Array[%d]: %d\n", i, myintarray[i]);
	}

	pr_info("got %d arguments for array. \n", arr_argc);

	return 0;
}

static void __exit hello_exit(void){
	pr_info("Goodbye Fox. \n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MrDFox");