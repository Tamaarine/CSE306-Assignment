#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "CSE 306 Testing Module"

static char * int_str = "";

module_param(int_str, charp, 0000);
MODULE_PARM_DESC(mystring, "A string of list of integers separated by spaces");

static int __init mymodule_init(void) {
    char * temp;
    char current_char;
    long parsed_res;
    long ret;
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
                printk(KERN_INFO "Number: %d\n", (int)parsed_res);
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
