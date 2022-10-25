#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "Homework 4 - CPU Profiler"

static int __init perftop_init(void) {
    printk(KERN_INFO "My module entered\n");
    
    return 0;
}

static void __exit perftop_exit(void) {
    printk(KERN_INFO "My module exited\n");
}

module_init(perftop_init);
module_exit(perftop_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
