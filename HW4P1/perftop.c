#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kprobes.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "Homework 4 - CPU Profiler"

static char func_name[NAME_MAX] = "pick_next_task_fair";    /* String that host the function to probe */

/*
 * Callback for when func_name is called
 */
static int entry_pick_next_fair(struct kretprobe_instance * ri, struct pt_regs * regs) {
    
}
NOKPROBE_SYMBOL(entry_pick_next_fair);    /* Don't probe this function */

/*
 * Callback for when func_name is returned
 */
static int ret_pick_next_fair(struct kretprobe_instance * ri, struct pt_regs * regs) {
    
}
NOKPROBE_SYMBOL(ret_pick_next_fair);    /* Don't probe this function */

static struct kretprobe my_kretprobe = {
    .handler = ret_pick_next_fair,          /* The callback used when the probing function is returned */
    .entry_handler = entry_pick_next_fair,  /* The callback used when the probing function is entered */
    .maxactive = 8     /* How many concurrent instances of probes. At least 8 to not miss any */
};

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
    int ret;
    
    printk(KERN_INFO "My module entered\n");
    
    /* Creates the /proc/perftop file.
     * 444 file permission, everyone can only read /proc/perftop
     * NULL for storing perftop under /proc
     * Finally the struct that describes all the callbacks
     */
    proc_create("perftop", 0, NULL, &perftop_proc_ops);
    
    my_kretprobe.kp.symbol_name = func_name;    /* Register the function */
    ret = register_kretprobe(&my_kretprobe);
    if (ret < 0) {
        printk(KERN_ERR "register_kretprobe failed, returned %d\n", ret);
        return -1;
    }
    printk(KERN_INFO "Planted return probe at %s: %p\n",
            my_kretprobe.kp.symbol_name, my_kretprobe.kp.addr);
    return 0;
}

static void __exit perftop_exit(void) {
    printk(KERN_INFO "My module exited\n");
    
    /* Null to signal /proc dir */
    remove_proc_entry("perftop", NULL);
    
    unregister_kretprobe(&my_kretprobe);
    printk(KERN_INFO "kretprobe at %p unregistered\n", my_kretprobe.kp.addr);
    
    printk("Missed probing %d instances of %s\n",
            my_kretprobe.nmissed, my_kretprobe.kp.symbol_name);
}

module_init(perftop_init);
module_exit(perftop_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
