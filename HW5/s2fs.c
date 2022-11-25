#include <linux/module.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "Homework 5 - Super Simple File System"

static int __init s2fs_init(void) {
    printk(KERN_INFO "My module entered\n");
    return 0;
}

static void __exit s2fs_exit(void) {
    printk(KERN_INFO "My module exited\n");
}

module_init(s2fs_init);
module_exit(s2fs_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);