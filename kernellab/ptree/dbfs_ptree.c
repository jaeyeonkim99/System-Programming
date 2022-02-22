#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

typedef struct process{
        char name[16];
        pid_t pid;
} Process;

static char* result = NULL;

static struct task_struct** list;
static int curr_index;
static int curr_maxsize;

static void trace(struct task_struct * now){
        list[curr_index] = now;
        if(now->pid==1||now->pid==0) return;
        curr_index++;
        if(curr_index+1>=curr_maxsize){
                list = (struct task_struct**)krealloc(list, 4*(curr_maxsize+1)*2, GFP_KERNEL);
                curr_maxsize = curr_maxsize*2;
        }
        printk("curr_index = %d", curr_index);
        trace(now->parent);
        return;
}       

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
        pid_t input_pid;
        int i;
        char* result_entry;

        sscanf(user_buffer, "%u", &input_pid);
        printk("input pid: %d", input_pid);

        list = (struct task_struct**)kmalloc(40, GFP_KERNEL);
        curr_index = 0;
        curr_maxsize = 9;

        printk("made list\n");

        // Find task_struct using input_pid. Hint: pid_task  
        curr = pid_task(find_vpid(input_pid), PIDTYPE_PID); 
        printk("read task_struct: %x\n", curr);

        // Tracing process tree from input_pid to init(1) process
        trace(curr);
        printk("finish trace\n");

        for(i=curr_index; i>-1; i--){
                printk("name of process: %s, pid: %d\n", list[i]->comm, list[i]->pid);
        }

        // Make Output Format string: process_command (process_id)    
        /*   
        result = "";
        for(i = curr_index; i>-1; i--){
                char tmp[10];
                sprintf(tmp, "%d", list[i]->pid);
                result_entry = "";
                result_entry = strcat(result_entry, list[i]->comm);
                result_entry = strcat(result_entry, " (");
                result_entry = strcat(result_entry, tmp);
                result_entry = strcat(result_entry, ")\n"); 
                result = strcat(result, result_entry);
        }
        */
        kfree(list);

        printk("%s\n", result);

        return length;
}


static const struct file_operations dbfs_fops = {        
        .write = write_pid_to_input,
};



static int __init dbfs_module_init(void)
{
        // Implement init module code

        dir = debugfs_create_dir("ptree", NULL);
        
        if (!dir) {
                printk("Cannot create ptree dir\n");
                return -1;
        }
        
        inputdir = debugfs_create_file("input", S_IWUGO, dir, NULL, &dbfs_fops);
        if(!inputdir){
                printk("Cannot creat input file\n");
                return -1;
        }
        
        ptreedir = debugfs_create_file("ptree", S_IRUGO, dir, NULL, &dbfs_fops); // Find suitable debugfs API
         if(!ptreedir){
                printk("Cannot creat ptree file\n");
                return -1;
        }
	

	printk("dbfs_ptree module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module code
        if(list!=NULL) kfree(list);
	debugfs_remove_recursive(dir);
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);

