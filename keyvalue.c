//////////////////////////////////////////////////////////////////////
//                             North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng
//
//   Description:
//     Skeleton of KeyValue Pseudo Device
//
////////////////////////////////////////////////////////////////////////




/*
***************************************


PROJECT SUBMITTED BY
Akanksha Singh (asingh27)
Shashank Jha (sjha5)


****************************************
*/

#include "keyvalue.h"
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/semaphore.h>

unsigned transaction_id;

struct myNode {
__u64 key;
__u64 size;
void* data;
struct myNode *next;
};

struct myNode * head = NULL;
struct semaphore sem;



static void free_callback(void *data)
{
}

static long keyvalue_get(struct keyvalue_get __user *ukv)
{
    if(down_interruptible(&sem))
	return -ERESTARTSYS;
 
    bool foundFlag = false;

    //printk(KERN_INFO "\nSTARTING GET\n");
    int bytesNotCopied;
    struct keyvalue_get kv;
    kv.key = ukv->key;
    //printk(KERN_INFO "Received Key: %d \n", kv.key);
 
     if(head == NULL)
    {
	printk(KERN_INFO "Head is NULL. Returning.\n");
	up(&sem);
	return -1;
    }
    struct myNode * temp = head;
    __u64 tempSize;
    while(temp != NULL)
    {

	if(temp->key == kv.key)
	{
		//printk(KERN_INFO "Key found.\n");
		tempSize = temp->size;
		kv.size = &(tempSize);
		kv.data = temp->data;
		//printk(KERN_INFO "Size found: %d\n", *(kv.size));
		//printk(KERN_INFO "Data found: %s\n", (char *)kv.data);
		foundFlag = true;
	}
	temp = temp->next;
    }


   if(!foundFlag)
   {
	printk (KERN_INFO "Key not found for get.\n");
	up(&sem);
	printk (KERN_INFO "Unlocked for get.\n");
	return -1;
   }


    bytesNotCopied = copy_to_user(ukv->data, kv.data, *(kv.size));
    if (bytesNotCopied !=0)
    {
	printk(KERN_INFO "Data not copied properly to user space!\n");
    	up(&sem);	
	return -1;
    }
    //else
    	//printk(KERN_INFO "Data copied to user space!\n");

    bytesNotCopied = copy_to_user(ukv->size, kv.size, sizeof(__u64));
    if (bytesNotCopied !=0)
    {
	printk(KERN_INFO "Size not copied properly to user space!\n");
	up(&sem);	
	return -1;
    }
    //else
    	//printk(KERN_INFO "Size copied to user space!\n");	


    //printk(KERN_INFO "Tranaction id before returning from get is %u \n", transaction_id);
    up(&sem);	
    return transaction_id++;
}

static long keyvalue_set(struct keyvalue_set __user *ukv)
{
    if(down_interruptible(&sem))
	return -ERESTARTSYS;
    //printk(KERN_INFO "\nSTARTING SET\n");

    int bytesNotCopied;
    struct keyvalue_set kv;

    kv.key = ukv->key;
    kv.size = ukv->size;
    if(kv.size > (4*1024))
    {
         up(&sem);	
	 return  -1;	
    }
    //printk(KERN_INFO "Key in kernel: %d \n", kv.key);
    //printk(KERN_INFO "Size in kernel: %d \n", kv.size);

    if((kv.data = kmalloc(kv.size, GFP_KERNEL))== NULL)
    {
    	printk(KERN_INFO "Memory not allocated!\n");
	up(&sem);	
        return -1;
    }
    //else
    	//printk(KERN_INFO "Memory allocated!\n");

    bytesNotCopied = copy_from_user(kv.data, ukv->data, kv.size);
    if (bytesNotCopied !=0)
    {
	printk(KERN_INFO "Data not copied!\n");
	up(&sem);	
	return -1;
    }
    //else
       //printk(KERN_INFO "Data copied!\n");

    struct myNode * checkCopyTemp;
    checkCopyTemp = head;
    int copyFlag = 0;

    while(checkCopyTemp!=NULL)
    {
	if(checkCopyTemp->key == kv.key)
	{
    		
		checkCopyTemp->key = kv.key;
		checkCopyTemp->size = kv.size;
		checkCopyTemp->data = kv.data;
		//printk(KERN_INFO "Key inserted: %d\n", checkCopyTemp->key);
    		//printk(KERN_INFO "Size inserted: %d\n", checkCopyTemp->size);
    		//printk(KERN_INFO "Data inserted: %s\n", (char *)(checkCopyTemp->data));
		//printk(KERN_INFO "Old key replaced\n");
		copyFlag = 1;
	}
	checkCopyTemp = checkCopyTemp->next;
    }


    if(!copyFlag)
    {
    	struct myNode * temp;
    	temp = (struct myNode *)(kmalloc(sizeof(struct myNode), GFP_KERNEL));
    	temp->key = kv.key;
    	temp->size = kv.size;
    	temp->data = kv.data;

    	if (head == NULL)
    	{
		head = temp;
		head->next = NULL;
    	}
    	else
    	{
		temp->next = head;
		head = temp; 
    	}

    	//printk(KERN_INFO "Key inserted: %d\n", head->key);
    	//printk(KERN_INFO "Size inserted: %d\n", head->size);
    	//printk(KERN_INFO "Data inserted: %s\n", (char *)(head->data));
    }

    //printk(KERN_INFO "Transaction id before returning from set is %u \n", transaction_id);
    

    up(&sem);
    return transaction_id++;
}

static long keyvalue_delete(struct keyvalue_delete __user *ukv)
{
   
    bool continueLoop=true;
    bool foundFlag = false;	
    printk (KERN_INFO "\nDELETE started\n");
    if(down_interruptible(&sem))
	return -ERESTARTSYS;
    printk (KERN_INFO "LOCKED\n");
    struct keyvalue_delete kv;
    kv.key = ukv->key;
    printk(KERN_INFO "Key to delete: %d\n", kv.key);

    if(head == NULL)
    {
	printk(KERN_INFO "Head is NULL. Returning.\n");
	up(&sem);
	return -1;
    }
    printk(KERN_INFO "Key in head: %d.\n", head->key);
    printk(KERN_INFO "Size in head: %d.\n", head->size);
    printk(KERN_INFO "Data in head: %s.\n", (char *)(head->data)); 
  
    if(head->key == kv.key)
    {
	printk(KERN_INFO "Deleting key in head: %d\n", head->key);
	printk(KERN_INFO "Deleting size in head: %d\n", head->size);
	printk(KERN_INFO "Deleting data in head: %d\n", (char *)(head->data));
	if(head->next == NULL)
	{   
          	head = NULL;	
	}
	else
	{
                printk (KERN_INFO"Before moving head to next\n");
	        printk(KERN_INFO "Next key :%d\n", head->next->key);
	        printk(KERN_INFO "Next size :%d\n", head->next->size);
	        printk(KERN_INFO "Next data :%s\n", (char *)(head->next->data));
		head = head->next;
                printk (KERN_INFO"After moving head to next\n");
	}
        foundFlag = true;	
    }
    else
    {
	struct myNode * temp;
	struct myNode * swapTemp;
    	temp = head;
	while(continueLoop && temp->next!=NULL)
	{
		if(temp->next->key == kv.key)
		{	
			printk(KERN_INFO "Deleting key in other nodes: %d\n", temp->next->key);
			printk(KERN_INFO "Deleting size in other nodes: %d\n", temp->next->size);
			printk(KERN_INFO "Deleting data in other nodes: %s\n", (char *)(temp->next->data));
			if(temp->next->next == NULL) //Last element in list
			{
				printk(KERN_INFO "Starting to delete last element\n");
				temp->next = NULL;
				printk(KERN_INFO "Finished deleting last element\n");
				continueLoop=false;
			}
			else
			{
				swapTemp = temp->next->next;
				temp->next = swapTemp;

			}
			foundFlag = true;
		}
		temp = temp->next;
	}
    }
    if(!foundFlag)
    {
	printk (KERN_INFO "Key not found for delete.\n");
	up(&sem);
	printk (KERN_INFO "Unlocked for delete.\n");
	return -1;
    }
    up(&sem);
    printk (KERN_INFO "DELETE ended.\n LOCK RELEASED\n");
    return transaction_id++;
}

//Added by Hung-Wei
     
unsigned int keyvalue_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    printk("keyvalue_poll called. Process queued\n");
    return mask;
}

static long keyvalue_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case KEYVALUE_IOCTL_GET:
        return keyvalue_get((void __user *) arg);
    case KEYVALUE_IOCTL_SET:
        return keyvalue_set((void __user *) arg);
    case KEYVALUE_IOCTL_DELETE:
        return keyvalue_delete((void __user *) arg);
    default:
        return -ENOTTY;
    }
}

static int keyvalue_mmap(struct file *filp, struct vm_area_struct *vma)
{
    return 0;
}

static const struct file_operations keyvalue_fops = {
    .owner                = THIS_MODULE,
    .unlocked_ioctl       = keyvalue_ioctl,
    .mmap                 = keyvalue_mmap,
//    .poll		  = keyvalue_poll,
};

static struct miscdevice keyvalue_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "keyvalue",
    .fops = &keyvalue_fops,
};

static int __init keyvalue_init(void)
{
    //Initializing semaphore
    sema_init(&sem,1);
    int ret;
    if ((ret = misc_register(&keyvalue_dev)))
        printk(KERN_ERR "Unable to register \"keyvalue\" misc device\n");
    return ret;
}

static void __exit keyvalue_exit(void)
{
    misc_deregister(&keyvalue_dev);
}

MODULE_AUTHOR("Hung-Wei Tseng <htseng3@ncsu.edu>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(keyvalue_init);
module_exit(keyvalue_exit);
