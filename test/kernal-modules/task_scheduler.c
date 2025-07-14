#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>

#define MAX_DEVICES 3

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SPM");
MODULE_DESCRIPTION("Basic Kernel Task Scheduler - Init");
MODULE_VERSION("0.1");

struct device_info {
    int id;
    int load_percent;
};

static struct device_info devices[MAX_DEVICES];

static void simulate_single_device(int i) {
    devices[i].id = i;
    get_random_bytes(&devices[i].load_percent, sizeof(int));
    devices[i].load_percent %= 101;
}

static void simulate_device_status(void) {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        simulate_single_device(i);
        printk(KERN_INFO "Device %d: Load = %d%%\n", i, devices[i].load_percent);
    }
}

static int __init scheduler_init(void) {
    printk(KERN_INFO "Scheduler module init\n");
    simulate_device_status();
    return 0;
}

static void __exit scheduler_exit(void) {
    printk(KERN_INFO "Scheduler module exit\n");
}

module_init(scheduler_init);
module_exit(scheduler_exit);
