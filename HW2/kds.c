#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rbtree.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "CSE 306 Testing Module"

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

static int __init mymodule_init(void) {
    char * temp;
    char current_char;
    long parsed_res;
    long ret;

    /* Declaration for link list */
    LIST_HEAD(mylinkedlist); /* macro that defines the sentinel node (The head) */
    struct my_link_list_struct * position, * next; /* Temp variable used in iteration */
    struct my_link_list_struct * to_add_link_list; /* Variable used to add to list */
    
    /* Declaration for red-black tree */
    struct rb_root mytree = RB_ROOT; /* Root node of the rb-tree, contains the rb_node pointer */
    struct my_rb_tree_struct * to_add_rb_node; /* Variable used to add to rb-tree */
    struct rb_node * rb_node_position, * rb_node_temp; /* Used for iteration */

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
                /* Parsed successfully */
                printk(KERN_INFO "Number from insmod: %d\n", (int)parsed_res);
                
                /* Insert into the corresponding data structure here */
                /* First, linked list */
                to_add_link_list = kmalloc(sizeof(struct my_link_list_struct), GFP_KERNEL);
                /* Memory allocation failed */
                if (!to_add_link_list) {
                    return -EINVAL;
                }
                
                /* Add integer to the struct */
                to_add_link_list->data = parsed_res;
                INIT_LIST_HEAD(&to_add_link_list->list); /* set the node to point to itself first */
                
                /* Then we add it to the head */
                list_add(&to_add_link_list->list, &mylinkedlist);
                
                
                /* Second, red-black tree */
                to_add_rb_node = kmalloc(sizeof(struct my_rb_tree_struct), GFP_KERNEL);
                /* Memory allocation failed */
                if (!to_add_rb_node) {
                    return -EINVAL;
                }
                
                /* Set the data of the inserted node */
                to_add_rb_node->data = parsed_res;
                to_add_rb_node->node.rb_left = NULL;
                to_add_rb_node->node.rb_right = NULL;
                my_rb_insert(&mytree, to_add_rb_node); /* Insert it by calling mb_rb_insert */
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
    
    /* Print out the integers inside linked list and then free */
    list_for_each_entry_safe_reverse(position, next, &mylinkedlist, list) {
        /*
         * First parameter is the ptr used to iterate over the link list
         * is just position.
         * Second parameter is the struct that the linked list is embedded in
         * Third parameter is the list_head name the embedded struct
         */
        printk(KERN_INFO "Linked List data: %d\n", position->data);
        
        /* Delete the list_head node */
        list_del(&position->list);
        
        /* Free it after printing out the data */
        kfree(position);
    }
    
    /* Print out the integers inside the rb_tree then free */
    rb_node_position = rb_first(&mytree);
    /* While rb_first is not NULL, print then remove and free */
    while (rb_node_position) {
        /* Reuse to_add_rb_node for storing the struct pointer */
        to_add_rb_node = rb_entry(rb_node_position, struct my_rb_tree_struct, node);
        
        printk(KERN_INFO "rb-tree data: %d\n", to_add_rb_node->data);
        
        rb_node_temp = rb_next(rb_node_position); /* Get next entry */
        
        rb_erase(rb_node_position, &mytree); /* Delete the node that was printed */
        kfree(rb_node_position); /* Free the node that was deleted */
        
        rb_node_position = rb_node_temp; /* Point to the next node */
    }
    
    
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
