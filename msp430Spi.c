#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mm.h>

// #defines:
#define DEVICE_NAME ("msp430Spi")
#define FIRST_MINOR_NUMBER (0)
#define TOTAL_MINOR_NUMBERS (1)
#define DEVICE_IN_BUFFER_SIZE (50)
#define DEVICE_OUT_BUFFER_SIZE (50)

// IRQ Handlers:
irq_handler_t testHandler(int irqNumber, void* deviceId)
{
    
    return (IRQ_HANDLED);
}


// Define in global memory space to avoid using stack:
struct cdev *p_device;
dev_t g_deviceNumber;
int g_deviceMajorNumber;

// The device memory:
struct msp430Spi
{
    char out_buffer[DEVICE_OUT_BUFFER_SIZE];
    char in_buffer[DEVICE_IN_BUFFER_SIZE];
    struct semaphore deviceSem;
};

struct msp430Spi* p_msp430Spi;


int device_open(struct inode* deviceFile, struct file* file)
{
    int retVal = 0;

    // Sem Pend
    if(down_interruptible(&p_msp430Spi->deviceSem))
        retVal = -ERESTARTSYS;

    printk(KERN_INFO "Device Open called\n");
    return retVal;
}

int device_release(struct inode* deviceInode, struct file* deviceFileInstance)
{
    int retVal = 0;
   
    printk(KERN_INFO "Device Release called\n");
    // Release the semaphore
    up(&p_msp430Spi->deviceSem);

    return retVal;
}

int device_write(struct file* deviceFileInstance, const char __user* userBuffer, 
        size_t size, loff_t* fileOffset)
{
    printk(KERN_INFO "Device write called\n");
    return 0;
}

int device_read(struct file* deviceFileInstance, char __user* userBuffer, 
        size_t size, loff_t* fileOffset)
{
    printk(KERN_INFO "Device read called\n");
    return 0;
}

struct file_operations fops=
{
   .owner = THIS_MODULE,
   .open = device_open,
   .release = device_release,
   .write = device_write,
   .read = device_read,

};

static int driver_entry(void)
{                                  
//    request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
//	    const char *name, void *dev)
    int retVal = 0;
    p_msp430Spi = kmalloc(sizeof(struct msp430Spi), GFP_KERNEL);
    printk(KERN_INFO "Driver Entry called!\n");

    sema_init(&p_msp430Spi->deviceSem,1);  
    
    retVal = alloc_chrdev_region(&g_deviceNumber, FIRST_MINOR_NUMBER, 
            TOTAL_MINOR_NUMBERS, DEVICE_NAME);
    if (retVal < 0)
        goto out_major_number_bad;

    g_deviceMajorNumber = MAJOR(g_deviceNumber);
    p_device = cdev_alloc(); 
    p_device->owner = THIS_MODULE;
    p_device->ops = &fops;

    retVal = cdev_add(p_device, g_deviceNumber, TOTAL_MINOR_NUMBERS);
    if (retVal < 0)
        goto out_cdev_bad;

    // Return 0 if all is good:
    return 0; 

out_major_number_bad:
    printk(KERN_ALERT "Failure to allocate major number!\n");
    return retVal;
out_cdev_bad:
    printk(KERN_ALERT "Failure to add cdev\n");
    return retVal;
}

static void driver_exit (void)
{
    printk(KERN_INFO "Driver Exit called\n");
    
    // Cleaning up:
    unregister_chrdev_region(g_deviceNumber, TOTAL_MINOR_NUMBERS);
    cdev_del(p_device);
    kfree(p_msp430Spi); 
}



module_init(driver_entry);
module_exit(driver_exit);
