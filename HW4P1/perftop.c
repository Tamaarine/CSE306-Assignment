#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "Homework 4 - CPU Profiler"

/* Function that actually writes to the proc file  */
static int perftop_proc_show(struct seq_file * m, void * v) {
    seq_printf(m, "Hello World\n");
    
    return 0;
}

/*
 * Open callback, gets called when proc file is opened.
 * Uses single_open to output all the data at once.
 */
static int perftop_proc_open(struct inode * inode, struct file * file) {
    return single_open(file, perftop_proc_show, NULL);
}

/* struct that define the callbacks for different operations for /proc/perftop */
static const struct proc_ops perftop_proc_ops = {
    .proc_open = perftop_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release
};

static int __init perftop_init(void) {
    printk(KERN_INFO "My module entered\n");
    
    /* Creates the /proc/perftop file.
     * 444 file permission, everyone can only read /proc/perftop
     * NULL for storing perftop under /proc
     * Finally the struct that describes all the callbacks
     */
    proc_create("perftop", 0, NULL, &perftop_proc_ops);
    
    return 0;
}

static void __exit perftop_exit(void) {
    printk(KERN_INFO "My module exited\n");
    
    /* Null to signal /proc dir */
    remove_proc_entry("perftop", NULL);
}

module_init(perftop_init);
module_exit(perftop_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
