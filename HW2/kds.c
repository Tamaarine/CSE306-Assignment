#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>

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

static int __init mymodule_init(void) {
    char * temp;
    char current_char;
    long parsed_res;
    long ret;

    /* Declaration for link list */
    LIST_HEAD(mylinkedlist); /* macro that defines the sentinel node (The head) */
    struct my_link_list_struct * position, * next; /* Temp variable used in iteration */
    struct my_link_list_struct * to_add; /* Variable used to add to list */

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
                /* First linked list */
                to_add = kmalloc(sizeof(struct my_link_list_struct), GFP_KERNEL);
                
                /* Memory allocation failed */
                if (!to_add) {
                    return -EINVAL;
                }
                
                /* Add integer to the struct */
                to_add->data = parsed_res;
                INIT_LIST_HEAD(&to_add->list); /* set the node to point to itself first */
                
                /* Then we add it to the head */
                list_add(&to_add->list, &mylinkedlist);
                
                /*  */
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
