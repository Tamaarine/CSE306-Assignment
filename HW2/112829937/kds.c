#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/hashtable.h>
#include <linux/radix-tree.h>
#include <linux/xarray.h>
#include <linux/bitmap.h>
#include <asm/bitops.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "CSE 306 Testing Module"
#define MAX_SIZE (1UL << 16) 
#define MAX_BITS 1001 /* 0 to 1000 inclusive */

static char * int_str = "";

module_param(int_str, charp, 0000);
MODULE_PARM_DESC(mystring, "A string of list of integers separated by spaces");

/* mystruct for linked list */
struct my_link_list_struct {
    int data; /* The data that stores the integer parsed */
    struct list_head list; /* Kernel's embedded linked list struct use this to traverse */
};

/* mystruct for red-black tree */
struct my_rb_tree_struct {
    int data; /* Stores integer */
    struct rb_node node; /* Kernel's embedded rb_node struct */
};

/* mystruct for hash table */
struct my_hash_table_struct {
    int data; /* Stores integer */
    struct hlist_node hash_list; /* Kernel's embedded linked list node for bucket */
};

/* Put a hash table into a struct, so I can dynamically allocate it on heap */
struct hash_table_wrapper {
    /* Declare a hashtable here with size 2^10 */
    DECLARE_HASHTABLE(myhashtable, 10);
};

/* Insert function for rb-tree */
static void my_rb_insert(struct rb_root * root, struct my_rb_tree_struct * new) {
    /* Pointer to a pointer of rb_node used to traverse the tree */
    struct rb_node ** link = &root->rb_node;
    struct rb_node * parent = NULL; /* Used to store the last node visited */
    int value = new->data; /* The value that we are inserting */
    struct my_rb_tree_struct * current_struct; /* Used to store the entry struct */
    
    /* Descend down the tree until the place to insert is found, link == NULL */
    while (*link) {
        parent = *link; /* Store the node pointer that link is pointing to. The current node basically */
        
        /* Use rb_entry to find the encapsulating struct */
        current_struct = rb_entry(parent, struct my_rb_tree_struct, node);
        
        /* make link point to left sub-tree if value is less than current node */
        if (current_struct->data > value) 
            link = &(*link)->rb_left;
        else
            link = &(*link)->rb_right;
    }
    
    /* Insert the new node at parent */
    rb_link_node(&new->node, parent, link);
    
    /* Rebalance if needed */
    rb_insert_color(&new->node, root);
}

/* 
 * Play with linked list, return -EINVAL if memory allocation failed
 * return 0 if succesfully played with the data structure.
 * 
 * nums: The array of numbers
 * length: The number of numbers from initializing module
*/
static int play_linked_list(int * nums, int length) {
    /* All the variables that are required */
    LIST_HEAD(mylinkedlist); /* macro that defines the sentinel node (The head) */
    struct my_link_list_struct * position, * next; /* Temp variable used in iteration */
    struct my_link_list_struct * to_add; /* Variable used to add to list */
    
    int * ptr; /* Used for nums iteration */
    
    /* Pointer arthimetic for iteration */
    for (ptr = nums; ptr < nums + length; ptr++) {
        to_add = kmalloc(sizeof(struct my_link_list_struct), GFP_KERNEL);
        /* Memory allocation failed */
        if (!to_add) {
            return -EINVAL;
        }
        
        to_add->data = *ptr; /* Store integer into struct */
        INIT_LIST_HEAD(&to_add->list); /* Set the node to point to itself first */
        
        list_add(&to_add->list, &mylinkedlist);
    }
    
    /* Print out the integers inside linked list and then free */
    list_for_each_entry_safe_reverse(position, next, &mylinkedlist, list) {
        /* Position contain the struct data */
        printk(KERN_INFO "Linked List data: %d\n", position->data);
        
        /* Delete the list_head node */
        list_del(&position->list);
        
        /* Free it after printing out the data */
        kfree(position);
    }
    
    return 0;
}

static int play_rb_tree(int * nums, int length) {
    /* Variable required */
    struct rb_root mytree = RB_ROOT; /* Root node of the rb-tree, contains the rb_node pointer */
    struct my_rb_tree_struct * to_add; /* Variable used to add to rb-tree */
    struct my_rb_tree_struct * my_struct_entry; /* Used for storing the entry in iteration */
    struct rb_node * position, * temp; /* Used for iteration */
    
    int * ptr; /* Used for nums iteration */
    
    /* Pointer arthimetic for iteration */
    for (ptr = nums; ptr < nums + length; ptr++) {
        to_add = kmalloc(sizeof(struct my_rb_tree_struct), GFP_KERNEL);
        /* Memory allocation failed */
        if (!to_add) {
            return -EINVAL;
        }
        
        to_add->data = *ptr;
        to_add->node.rb_left = NULL;
        to_add->node.rb_right = NULL;
        
        my_rb_insert(&mytree, to_add); /* Insert it by calling mb_rb_insert */
    }
    
    /* Print out the integers inside the rb_tree then free */
    position = rb_first(&mytree);
    /* While rb_first is not NULL, print then remove and free */
    while (position) {
        my_struct_entry = rb_entry(position, struct my_rb_tree_struct, node);
        
        printk(KERN_INFO "Rb-tree data: %d\n", my_struct_entry->data);
        
        temp = rb_next(position); /* Get next entry */
        
        rb_erase(position, &mytree); /* Delete the node that was printed */
        kfree(my_struct_entry); /* Free the struct that was deleted */
        
        position = temp; /* Point to the next node */
    }
    
    return 0;
}

static int play_hash_table(int * nums, int length) {
    /* Declaration for hash table */
    struct my_hash_table_struct * position; /* Used for iteration */
    struct my_hash_table_struct * to_add; /* Variable used to add to hashtable */
    int bkt; /* Used for iteration */
    
    /* Allocate the hashtable on the stack */
    struct hash_table_wrapper * ht_wrapper = kmalloc(sizeof(struct hash_table_wrapper), GFP_KERNEL);
        
    int * ptr; /* Used for nums iteration */
    
    /* Pointer arthimetic for iteration */
    for (ptr = nums; ptr < nums + length; ptr++) {
        to_add = kmalloc(sizeof(struct my_hash_table_struct), GFP_KERNEL);
        /* Memory allocation failed */
        if (!to_add) {
            return -EINVAL;
        }
        
        to_add->data = *ptr; /* Add the data */
        /* Add current node to hash table, using the data as key */
        hash_add(ht_wrapper->myhashtable, &to_add->hash_list, to_add->data);
    }
    
    /* Print out the integers inside hash table */
    hash_for_each(ht_wrapper->myhashtable, bkt, position, hash_list) {
        printk(KERN_INFO "Hash table data (for_each): %d\n", position->data);
    }
    
    /* Used this to iterate over each of the keys */
    for (ptr = nums; ptr < nums + length; ptr++) {
        /* Should only be few entry */
        hash_for_each_possible(ht_wrapper->myhashtable, position, hash_list, *ptr) {
            printk(KERN_INFO "Hash table data (for_each_possible): %d\n", position->data);
        }
    }
    
    /* Then we free and delete each entry */
    hash_for_each(ht_wrapper->myhashtable, bkt, position, hash_list) {
        hash_del(&position->hash_list); /* Delete the hash_list object in the struct */
        kfree(position); /* Then free the struct */
    }
    
    /* Finally free the hash table itself */
    kfree(ht_wrapper);
    
    return 0;
}

static int play_radix_tree(int * nums, int length) {
    RADIX_TREE(myradixtree, GFP_KERNEL); /* Declare the radix tree and initialize */
    int * ret; /* Used to store the radix_tree_lookup return pointer */
    int * allocated_num; /* Used to store the allocated number on the heap */
    
    int ** results; /* Use to store the first elements of the array of returned gang lookup */
    int results_length; /* Used track how many results are stored */
    int ** results_ptr; /* Used to iterate over the array of pointers */
    
    int * ptr; /* Used for iterating over the nums array */
    for (ptr = nums; ptr < nums + length; ptr++) {
        /* Indexed by the key, and the number as the data as well */
        radix_tree_preload(GFP_KERNEL);
        allocated_num = kmalloc(sizeof(int), GFP_KERNEL);
        if (!allocated_num) {
            return -EINVAL;
        }
        *allocated_num = *ptr;
        
        /* The key is just the number. The data we storing is a pointer to the number we allocated */
        radix_tree_insert(&myradixtree, *ptr, (void *)allocated_num);
        radix_tree_preload_end();        
    }
    
    /* Look up all of the numbers by radix_tree_lookup */
    for (ptr = nums; ptr < nums + length; ptr++) {
        ret = radix_tree_lookup(&myradixtree, *ptr);
        
        /* If the number is odd tag it */
        if (*ptr % 2 == 1)
            radix_tree_tag_set(&myradixtree, *ptr, 1);
        printk(KERN_INFO "Radix tree data: %d\n", *ret);
    }
    
    /* results need to be an array of pointers, because internally
     * radix_tree_gang_lookup_tag stores the tagged results into the pointers
     */
    results = kmalloc(sizeof(int *) * length, GFP_KERNEL);
    
    if (!results) {
        return -EINVAL;
    }
    
    /* Find all odd number tags */
    results_length = radix_tree_gang_lookup_tag(&myradixtree, (void **)results, 0, length, 1);
    
    /* Display them */
    for (results_ptr = results; results_ptr < results + results_length; results_ptr++) {
        printk(KERN_INFO "Radix tree data (from gang_lookup_tag on odd number): %d\n", **results_ptr);
    }
    
    /* Time for freeing */
    kfree(results); /* First the array of pointers needs to be freed */
    
    /* Then each of the allocated data */
    for (ptr = nums; ptr < nums + length; ptr++) {
        ret = radix_tree_lookup(&myradixtree, *ptr);
        radix_tree_delete(&myradixtree, *ptr); /* Delete the entry from the radix tree */
        
        /* Then free the integer that was allocated on the heap */
        kfree(ret);
    }
    
    return 0;
}

static int play_xarray(int * nums, int length) {
    DEFINE_XARRAY(myxarray); /* Declare the xarray and initialize it */
    
    int * allocated_num; /* Used for storing the pointer to the number on heap */
    int * ret; /* Used for storing the result from xa_load() */
    unsigned long temp;
    
    int * ptr; /* Used for iteration over nums */
    for (ptr = nums; ptr < nums + length; ptr++) {
        allocated_num = kmalloc(sizeof(int), GFP_KERNEL);
        if (!allocated_num) {
            return -EINVAL;
        }
        
        /* Store the number into the memory we allocated just like in radix tree */
        *allocated_num = *ptr;
        
        /* Store into the xarray data structure */
        xa_store(&myxarray, *ptr, allocated_num, GFP_KERNEL);
    }
    
    /* Then iterate over the numbers and look up in xarray using xa_load */
    for (ptr = nums; ptr < nums + length; ptr++) {
        ret = xa_load(&myxarray, *ptr);
        
        /* If the number is odd tag it */
        if (*ptr % 2 == 1)
            xa_set_mark(&myxarray, *ptr, 1);
        
        printk(KERN_INFO "Xarray data: %d\n", *ret);
    }
    
    temp = 0;
    
    /* Use xa_for_each_marked to print out all odd numbers */
    xa_for_each_marked(&myxarray, temp, ret, 1) {
        printk(KERN_INFO "Xarray data from xa_for_each_marked: %d\n",*ret);
    }
    
    /* Then delete each entry from xarray */
    for (ptr = nums; ptr < nums + length; ptr++) {
        ret = xa_load(&myxarray, *ptr);
        xa_erase(&myxarray, *ptr); /* Delete entry from the xarray */
        
        /* Then free the integer */
        kfree(ret);
    }
    
    return 0;    
}

static int play_bitmap(int * nums, int length) {
    DECLARE_BITMAP(mybitmap, MAX_BITS); /* Declare an array of long that's long enough to store [0, 1000] */
    int bit_num; /* Used to store which number the bit was set */
    
    int * ptr; /* Used for iteration over nums */
    
    bitmap_zero(mybitmap, MAX_BITS); /* Clear the bitmap to initialize */
    
    /* Set each number's bit */
    for (ptr = nums; ptr < nums + length; ptr++) {
        set_bit(*ptr, mybitmap);
    }
    
    /* Go through each set bit and print out the number */
    for_each_set_bit(bit_num, mybitmap, MAX_BITS) {
        printk(KERN_INFO "Bitmap data: %d\n", bit_num);
    }
    
    bitmap_zero(mybitmap, MAX_BITS); /* Finally clear out the bitmap last time before exiting */
    
    return 0;
}

static int __init mymodule_init(void) {
    /* Used for parsing the argument */
    char * temp;
    char current_char;
    long parsed_res;
    long ret;
    
    /* Used for storing the numbers parsed */
    int * nums;
    int size;
    
    /* Allocate MAX_SIZE of integers */
    nums = kmalloc(MAX_SIZE * sizeof(int), GFP_KERNEL);
    size = 0;
    
    printk(KERN_INFO "My module entered!\n");
    
    /* Set temp to int_str initially */
    temp = int_str;
    
    /* Need to convert each of the string number to int */
    while (true) {
        current_char = *temp;
        
        /* if current is ' ' or '\0' need to parse everything prior */
        if (current_char == ' ' || current_char == '\0') {
            /* only need to replace ' ' with '\0' */
            if (current_char == ' ') {
                *temp = '\0';
            }
            
            if (strcmp(int_str, "") != 0) {
                /* Only parse if the empty isn't empty */
                /* parse and store integer into parsed_res */
                ret = kstrtol(int_str, 10, &parsed_res);
                
                if (ret) {
                    printk(KERN_INFO "Somehow parsing the integer failed error code: %ld\n", ret);
                    return ret;
                }
                
                /* Insert into array check if it has space
                 * If not then free the array and return -EINVAL
                 */
                if (size >= MAX_SIZE) {
                    kfree(nums);
                    printk(KERN_INFO "Too many integer entered\n");
                    return -EINVAL;
                }
                /* Otherwise insert */
                *(nums + size) = parsed_res;
                size++;
                
                /* Parsed successfully */
                printk(KERN_INFO "Number from insmod: %d\n", (int)parsed_res);
            }
        }
        
        /* only increment if current originally was not \0. */
        if (current_char != '\0') {
            temp++;
        }
        else {
            /* If it is. Then done parsing, reached end of string */
            break;
        }
        /* only update the slow pointer if it was a space */
        if (current_char == ' ') {
            int_str = temp;
        }
    }
    
    play_linked_list(nums, size);
    play_rb_tree(nums, size);
    play_hash_table(nums, size);
    play_radix_tree(nums, size);
    play_xarray(nums, size);
    play_bitmap(nums, size);
    
    /* Free the number we allocated in the beginning */
    kfree(nums);
    
    return 0;
}

static void __exit mymodule_exit(void) {
    printk(KERN_INFO "My module exited!\n");
}

module_init(mymodule_init);
module_exit(mymodule_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
