#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/hashtable.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "CSE 306 Testing Module"
#define MAX_SIZE (1UL << 16) 

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

/* Insert function for rb-tree */
void my_rb_insert(struct rb_root * root, struct my_rb_tree_struct * new) {
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

/* Search function */
struct my_rb_tree_struct * my_rb_search(struct rb_root * root, int value) {
    struct rb_node * node = root->rb_node;
    
    while (node) {
        struct my_rb_tree_struct * data = container_of(node, struct my_rb_tree_struct, node);
        
        /* The value we are looking for is smaller than current node */
        if (data->data > value) 
            node = node->rb_left;
        else if (data->data < value)
            node = node->rb_right;
        else
            return data;
    }
    /* Unable to find the node we are looking for */
    return NULL;
}

/* 
 * Play with linked list, return -EINVAL if memory allocation failed
 * return 0 if succesfully played with the data structure.
 * 
 * nums: The array of numbers
 * length: The number of numbers from initializing module
*/
int play_linked_list(int * nums, int length) {
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

int play_rb_tree(int * nums, int length) {
    /* Variable required */
    struct rb_root mytree = RB_ROOT; /* Root node of the rb-tree, contains the rb_node pointer */
    struct my_rb_tree_struct * to_add; /* Variable used to add to rb-tree */
    struct my_rb_tree_struct * my_struct_entry; /* Used for storing the entry in iteration */
    
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
    
    for (ptr = nums; ptr < nums + length; ptr++) {
        /* Used search function */
        my_struct_entry = my_rb_search(&mytree, *ptr);
        if (my_struct_entry != NULL) {
            printk(KERN_INFO "Rb-tree data: %d\n", my_struct_entry->data);
            /* Delete and free from rb_tree */
            rb_erase(&my_struct_entry->node, &mytree);
            kfree(my_struct_entry);
        }
    }
    
    return 0;
}

int play_hash_table(int * nums, int length) {
    /* Declaration for hash table */
    struct my_hash_table_struct * position; /* Used for iteration */
    struct my_hash_table_struct * to_add; /* Variable used to add to hashtable */
    int bkt; /* Used for iteration */
    DEFINE_HASHTABLE(myhashtable, 10); /* Will have 2^10 buckets for hashtable */
    
    int * ptr; /* Used for nums iteration */
    
    hash_init(myhashtable); /* Initialize the hash table */
    
    /* Pointer arthimetic for iteration */
    for (ptr = nums; ptr < nums + length; ptr++) {
        to_add = kmalloc(sizeof(struct my_hash_table_struct), GFP_KERNEL);
        /* Memory allocation failed */
        if (!to_add) {
            return -EINVAL;
        }
        
        to_add->data = *ptr; /* Add the data */
        /* Add current node to hash table, using the data as key */
        hash_add(myhashtable, &to_add->hash_list, to_add->data);
    }
    
    /* Print out the integers inside hash table */
    hash_for_each(myhashtable, bkt, position, hash_list) {
        printk(KERN_INFO "Hash table data (for_each): %d\n", position->data);
        // hash_del(&position->hash_list); /* Delete the hash_list object in the struct */
        // kfree(position); /* Then free the struct */
    }
    
    /* Used this to iterate over each of the keys */
    for (ptr = nums; ptr < nums + length; ptr++) {
        /* Should only be few entry */
        hash_for_each_possible(myhashtable, position, hash_list, *ptr) {
            printk(KERN_INFO "Hash table data (for_each_possible): %d\n", position->data);
        }
    }
    
    /* Then we free and delete each entry */
    hash_for_each(myhashtable, bkt, position, hash_list) {
        hash_del(&position->hash_list); /* Delete the hash_list object in the struct */
        kfree(position); /* Then free the struct */
    }
    
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
