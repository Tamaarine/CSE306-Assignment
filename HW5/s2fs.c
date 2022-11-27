#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>

#define DRIVER_AUTHOR "Ricky Lu ricky.lu@stonybrook.edu"
#define DRIVER_DESC   "Homework 5 - Super Simple File System"
#define LFS_MAGIC 0x19920342 /* # Used to identify the filesystem */

/*
 * Function for defining a inode whenever a file/directory is created.
 * The mode parameter tells if this file is a directory or a file,
 * it also contains permissions.
*/
static struct inode * s2fs_make_inode(struct super_block * sb, int mode) {
    struct inode * ret = new_inode(sb);
    
    if (ret) {
        ret->i_mode = mode;
        ret->i_blocks = 0;
        ret->i_atime = ret->i_mtime = ret->i_ctime = current_time(ret);
        ret->i_ino = get_next_ino();
    }
    return ret;
}

/*
 * This function creates a directory in the filesystem.
 * Also calls s2fs_make_inode except the mode is for a directory.
*/
static struct dentry * s2fs_create_dir(struct super_block * sb, struct dentry * parent, const char * dirname) {
    struct dentry * dentry;
    struct inode * inode;
    
    dentry = d_alloc_name(parent, dirname);
    if (!dentry) {
        return 0;
    }
    
    inode = s2fs_make_inode(sb, S_IFDIR | 0755);
    if (!inode) {
        dput(dentry);
        return 0;
    }
    /*
     * Add the operation for inode, and directory ops to be the pre-defined ones
    */
    inode->i_op = &simple_dir_inode_operations;
    inode->i_fop = &simple_dir_operations;
    
    d_add(dentry, inode);
    return dentry;
}

/* Superblock's operations */
static struct super_operations s2fs_s_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_delete_inode
};

/* What happens when the file is opened */
static int s2fs_open(struct inode * inode, struct file * filp) {
    return 0;
}

/* What happens when you write to a file */
static ssize_t s2fs_write_file(struct file * filp, const char * buf,
                        size_t count, loff_t * offset)
{
    return 0;
}

static ssize_t s2fs_read_file(struct file * filp, char * buf,
                        size_t count, loff_t * offset)
{
    /* Just writes the message Hello World! to the buffer */
    char * msg = "Hello World!";
    int len = strlen(msg) + 1; /* Plus one to account for null terminator */
    
    /* If it has already printed enough don't go on */
    if (*offset > len) {
        return 0;
    }
    if (count > len - *offset) {
        count = len - *offset;
    }
    if (copy_to_user(buf, msg, count))
        return -EFAULT;
    *offset += count;
    return count;
}

/* Put all of the operations defined previously together into a struct */
static struct file_operations s2fs_fops = {
    .open = s2fs_open,
    .write = s2fs_write_file,
    .read = s2fs_read_file
};

/*
 * Function used to fill the superblock's metadata
*/
static int s2fs_fill_super(struct super_block * sb, void * data, int silent) {
    /*
     * Two args below are used to create the root directory
     * of this filesystem. All ops comes from libfs so we don't have to
     * write anything from scratch.
    */
    struct inode * root;
    struct dentry * root_dentry;
    
    sb->s_blocksize = PAGE_SIZE;        /* Blocksize for the filesystem */
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_magic = LFS_MAGIC;
    sb->s_op = &s2fs_s_ops;
    
    /* Create the inode for root directory */
    root = s2fs_make_inode(sb, S_IFDIR | 0755);
    if (!root)
        return -ENOMEM;
    root->i_op = &simple_dir_inode_operations;
    root->i_fop = &simple_dir_operations;
    
    /* Give a dentry to represent the root directory */
    root_dentry = d_make_root(root);
    if (!root_dentry) {
        iput(root);
        return -ENOMEM;
    }
    sb->s_root = root_dentry;
    
    return 0;
};

static struct dentry * s2fs_get_super(struct file_system_type *fst, int flags, const char * devname, void * data) {
    return mount_nodev(fst, flags, data, s2fs_fill_super);
}

static struct file_system_type s2fs_type = {
    .owner = THIS_MODULE,
    .name  = "s2fs",
    .mount = s2fs_get_super,
    .kill_sb = kill_litter_super,
};

static int __init s2fs_init(void) {
    printk(KERN_INFO "My module entered\n");
    return register_filesystem(&s2fs_type);
}

static void __exit s2fs_exit(void) {
    printk(KERN_INFO "My module exited\n");
    unregister_filesystem(&s2fs_type);
}

module_init(s2fs_init);
module_exit(s2fs_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);