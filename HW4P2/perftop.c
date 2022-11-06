#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kprobes.h>
#include <linux/hashtable.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <asm/msr.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "Homework 4 - CPU Profiler"
#define UNSIGN_LONG_NULL (unsigned long)NULL

static char func_name[NAME_MAX] = "pick_next_task_fair";    /* String that host the function to probe */
static int pre_count;   /* Counting entry */
static int post_count;  /* Counting return */
static int context_switch_counter;  /* Counting number of context switches*/
static struct hash_table_wrapper * ht_wrapper;  /* Used for storing the global hashtable */
static struct rb_root mytree = RB_ROOT;         /* Used for storing the task ordered by total tsc */

DEFINE_SPINLOCK(pre_count_lock);
DEFINE_SPINLOCK(post_count_lock);
DEFINE_SPINLOCK(post_timing_lock);      /* One lock to incorporate all of prev != next */

/* Data for storing prev to be carried into ret_handler */
struct my_data {
    unsigned long prev;
};

/* Each individual hash table entries.
 * For this homework, the key for the hash table is the PID of
 * each task_struct. The value would be this entire struct.
 * I'm going to additionally embed the pid in the entry so
 * when we iterate over it, we can further verify that this
 * entry indeed for that task_struct in case of collision.
 */
struct my_hash_table_struct {
    unsigned long long tsc;         /* Stores the timestamp counter */
    pid_t pid;                      /* Keep the pid of the entry */
    struct rb_node * node;          /* Reference to the rb_node for the task_struct */
    struct hlist_node hash_list;    /* Kernel embedded linked list node for bucket */
};

/* Dynamically allocate a hash table */
struct hash_table_wrapper {
    DECLARE_HASHTABLE(myhashtable, 10);
};

/* mystruct for red-black tree.
 * The rb-tree in this homework will be used to keep track
 * of the Total accumulated time that the task_struct spend in CPU.
 * The TAT is calculated by using the elapsed time (time in hash table - rdtsc())
 * and added to the initial time, which starts at 0.
 */
struct my_rb_tree_struct {
    unsigned long long ttsc;        /* Stores the total accumlative tsc time */
    pid_t pid;                      /* Stores the pid associated with the top-10 scheduled task */
    struct rb_node node;            /* Embedded rb_node struct */
};

/* Insertion function for rb-tree */
static void my_rb_insert(struct rb_root * root, struct my_rb_tree_struct * new) {
    /* Two level of indirection to prevent null deref */
    struct rb_node ** link = &root->rb_node;
    struct rb_node * parent = NULL; /* Points to last node visited*/
    unsigned long long value = new->ttsc;       /* The value we are inserting */
    struct my_rb_tree_struct * current_struct;  /* Used to store entry struct */
    
    /* Go down the tree until insertion place is found, link == NULL basically */
    while (*link) {
        parent = *link;     /* Put the current pointer pointed by link into parent */
        
        /* Get encapsulating struct around parent by calling rb_entry */
        current_struct = rb_entry(parent, struct my_rb_tree_struct, node);
        
        /* Belongs in the left sub-tree */
        if (current_struct->ttsc > value)
            link = &(*link)->rb_left;
        else
            link = &(*link)->rb_right;
    }
    
    /* Insert the node at parent */
    rb_link_node(&new->node, parent, link);
    
    /* Rebalance if needed */
    rb_insert_color(&new->node, root);
}

/*
 * Callback for when func_name is called
 */
static int entry_pick_next_fair(struct kretprobe_instance * ri, struct pt_regs * regs) {
    struct my_data * data;
    
    if (!current->mm)
        return 1;   /* Skip kernel threads*/
    /*
     * %rdi contain struct rq * rq
     * %rsi contain struct task_struct * prev
     * %rdx contain struct rq_flags * rf
     * https://wiki.osdev.org/System_V_ABI
     * https://aaronbloomfield.github.io/pdr/book/x86-64bit-ccc-chapter.pdf
     */
    data = (struct my_data *)ri->data;  /* Get the data from instance, typecast to my_data */
    data->prev = regs->si;              /* %rsi is the second parameter that contain prev */
    spin_lock(&pre_count_lock);
    pre_count++;
    spin_unlock(&pre_count_lock);
    return 0;
}
NOKPROBE_SYMBOL(entry_pick_next_fair);    /* Don't probe this function */

/*
 * Callback for when func_name is returned
 */
static int ret_pick_next_fair(struct kretprobe_instance * ri, struct pt_regs * regs) {
    struct my_data * data;
    unsigned long next;
    
    struct my_hash_table_struct * hash_to_add;   /* temp var used for adding to hashtable */
    struct my_hash_table_struct * position; /* Used for bucket iteration */
    pid_t pid;                              /* Storing prev|next's PID */
    
    struct my_rb_tree_struct * node_to_add;      /* temp var for adding to rb-tree */
    struct my_rb_tree_struct * my_struct_entry;  /* Holds the unwrapped struct */
    
    unsigned long long start_tsc;   /* Obtained from hash table. The last tsc timestamp */
    unsigned long long current_tsc; /* rdtsc() */
    unsigned long long elapsed;     /* start_tsc - current_tsc */
    
    next = regs_return_value(regs);     /* Get return value of func_name. In our case the next task_struct **/

    data = (struct my_data *)ri->data;  /* Retriveing my_data from instance */
    if (data->prev != next && next != UNSIGN_LONG_NULL && data->prev != UNSIGN_LONG_NULL) {
        /*
         * Only increment if next != null, prev != null, prev != next.
         * No race condition will occur, because next, prev are all unique
         * per instance. If two threads needs to increment context_switch
         * the spin_lock will force the others to wait. If it doesn't need to
         * increment context_switch then no need to lock it.
         */
        spin_lock(&post_timing_lock);
        context_switch_counter++;
        
        /* Work for prev */
        pid = ((struct task_struct *)data->prev)->pid;
        current_tsc = rdtsc();
        
        hash_for_each_possible(ht_wrapper->myhashtable, position, hash_list, pid) {
            if (position->pid == pid) {
                /* pid match, we found the entry, get the start_tsc from this entry */
                break;
            }
        }
        
        /*
         * position is null = didn't find entry in hash table.
         * We will initialize one for prev. And add in a new
         * rb_node into the tree.
         */
        if (!position) {
            hash_to_add = kmalloc(sizeof(struct my_hash_table_struct), GFP_ATOMIC);
            hash_to_add->pid = pid;
            hash_to_add->node = NULL;
            hash_to_add->tsc = current_tsc;
            
            hash_add(ht_wrapper->myhashtable, &hash_to_add->hash_list, pid);
            position = hash_to_add;
        }
        
        start_tsc = position->tsc;
        elapsed = current_tsc - start_tsc;
        node_to_add = kmalloc(sizeof(struct my_rb_tree_struct), GFP_ATOMIC);
        node_to_add->pid = pid;     /* Can be updated outside */
        
        /*
         * If the hash entry already have a old node, must add to it by retrieving
         * the old entry ttsc. Then erase the old entry & free it.
         *
         * If is a brand new hash entry, just set time to be 0
         * for node_to_add struct.
         */
        if (position->node) {
            my_struct_entry = rb_entry(position->node, struct my_rb_tree_struct, node);
            node_to_add->ttsc = my_struct_entry->ttsc + elapsed;
            
            rb_erase(&my_struct_entry->node, &mytree);
            kfree(my_struct_entry);
        }
        else {
            node_to_add->ttsc = 0;
        }
        position->node = &node_to_add->node;     /* Update hash entry with new node */
        my_rb_insert(&mytree, node_to_add);      /* Add to rb-tree */
        
        /* Work for next */
        pid = ((struct task_struct *)(next))->pid;
        
        hash_for_each_possible(ht_wrapper->myhashtable, position, hash_list, pid) {
            if (position->pid == pid)
                break; /* Break if entry found */
        }
        
        if (!position) {
            /* Next doesn't have entry yet either */
            hash_to_add = kmalloc(sizeof(struct my_hash_table_struct), GFP_ATOMIC);
            hash_to_add->node = NULL;
            hash_to_add->tsc = current_tsc;
            hash_to_add->pid = pid;
            
            hash_add(ht_wrapper->myhashtable, &hash_to_add->hash_list, pid);
        }
        else {
            /* If it has then just update tsc with current_tsc */
            position->tsc = current_tsc;
        }
        spin_unlock(&post_timing_lock);
    }
    spin_lock(&post_count_lock);
    post_count++;
    spin_unlock(&post_count_lock);
    return 0;
}
NOKPROBE_SYMBOL(ret_pick_next_fair);    /* Don't probe this function */

static struct kretprobe my_kretprobe = {
    .handler = ret_pick_next_fair,          /* The callback used when the probing function is returned */
    .entry_handler = entry_pick_next_fair,  /* The callback used when the probing function is entered */
    .data_size = sizeof(struct my_data),    /* No screwing around with the size, provide it! */
    .maxactive = 8     /* How many concurrent instances of probes. At least 8 to not miss any */
};

/* Function that actually writes to the proc file  */
static int perftop_proc_show(struct seq_file * m, void * v) {
    seq_printf(m, "Pre count: %d Post count: %d Context switch: %d\n",
            pre_count, post_count, context_switch_counter);
    
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
    /* Initialize the global hashtable */
    ht_wrapper = kmalloc(sizeof(struct hash_table_wrapper), GFP_KERNEL);
    
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
