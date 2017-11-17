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
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/kfifo.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include "msp430Spi.h"

// Settings #defines:
#define DEVICE_NAME             ("msp430Spi")
#define AUTHOR_HANDLE           "SmYte"
#define FIRST_MINOR_NUMBER      (0)
#define TOTAL_MINOR_NUMBERS     (1)
#define DEVICE_IN_BUFFER_SIZE   (128)
#define DEVICE_OUT_BUFFER_SIZE  (128)
#define TRANSFER_TIMER_NS_INTERVAL (62000) // Set to 62 us, 62000 ns

// Hardware specific #defines:
#define BCM2836_PERIPHERAL_BASE     (0x3F000000)
#define BCM2836_GPIO_BASE           (BCM2836_PERIPHERAL_BASE + 0x200000)

#define BCM2836_SPI_BASE            (BCM2836_PERIPHERAL_BASE + 0x204000)
#define BCM2836_SPI_LEN             (0x18)
#define BCM2836_SPI_CS_OFFSET       (0x00)
#define BCM2836_SPI_FIFO_OFFSET     (0x04)
#define BCM2836_SPI_CLK_OFFSET      (0x08)
#define BCM2836_SPI_DLEN_OFFSET     (0x0C)
#define BCM2836_SPI_LTOH_OFFSET     (0x10)
#define BCM2836_SPI_DC_OFFSET       (0x14)

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
    struct kfifo in_fifo;
    struct kfifo out_fifo;
    struct semaphore deviceSem;
    struct hrtimer transferTimer;
    ktime_t transferTimerInterval;
    void* virtual_spiBase;
};

struct msp430Spi* p_msp430Spi;

union msp430_spi_cs_register cs_register_buffer;


static void driver_cleanup(void)
{
    // Cleaning up:
    hrtimer_cancel(&p_msp430Spi->transferTimer);
    iounmap(p_msp430Spi->virtual_spiBase);
    release_mem_region(BCM2836_SPI_BASE, BCM2836_SPI_LEN);
    kfifo_free(&p_msp430Spi->in_fifo);
    kfifo_free(&p_msp430Spi->out_fifo);
    kfree(p_msp430Spi);
    unregister_chrdev_region(g_deviceNumber, TOTAL_MINOR_NUMBERS);
    cdev_del(p_device);
}

static int device_open(struct inode* deviceFile, struct file* file)
{
    int retVal = 0;

    //// TODO: Remove, just used to test driver and populate fifo
    //unsigned char data0 = 255;
    //unsigned char data1 = 128;
    //unsigned char data2 = 0;
    //unsigned char data3 = 136;

    //kfifo_put(&p_msp430Spi->in_fifo, data0);
    //kfifo_put(&p_msp430Spi->in_fifo, data1);
    //kfifo_put(&p_msp430Spi->in_fifo, data2);
    //kfifo_put(&p_msp430Spi->in_fifo, data3);
    
    // Sem Pend
    if(down_interruptible(&p_msp430Spi->deviceSem))
        retVal = -ERESTARTSYS;
    printk(KERN_INFO "Device Open called\n");
    return retVal;
}

static int device_release(struct inode* deviceInode, struct file* deviceFileInstance)
{
    int retVal = 0;
   
    printk(KERN_INFO "Device Release called\n");
    // Release the semaphore
    up(&p_msp430Spi->deviceSem);

    return retVal;
}

static int device_write(struct file* deviceFileInstance, 
        const char __user* userBuffer, 
        size_t size, loff_t* fileOffset)
{
    unsigned int numCopied = 0;
    int retVal = 0;
    printk(KERN_INFO "Device write called\n");
    
    retVal = kfifo_from_user(&p_msp430Spi->out_fifo, userBuffer, size, &numCopied);
    if (0 > retVal)
        goto out_copy_failed;

    return numCopied;

out_copy_failed:
    driver_cleanup();
    return retVal;
}

static int device_read(struct file* deviceFileInstance, char __user* userBuffer, 
        size_t size, loff_t* fileOffset)
{
    unsigned int numCopied = 0;
    int retVal = 0;
    printk(KERN_INFO "Device read called\n");
    
    retVal =  kfifo_to_user(&p_msp430Spi->in_fifo, userBuffer, size, &numCopied);

    if (0 > retVal)
        goto out_copy_failed;
    
    // If all succeeds:
    return numCopied;

out_copy_failed:
    driver_cleanup();
    return retVal;
}

struct file_operations fops=
{
   .owner = THIS_MODULE,
   .open = device_open,
   .release = device_release,
   .write = device_write,
   .read = device_read,

};

static int spi_peripheral_setup(void)
{
    int retVal = 0;

    // Attempt to grab control of memory mapped SPI IO:
    if (NULL==request_mem_region(BCM2836_SPI_BASE, BCM2836_SPI_LEN, DEVICE_NAME))
    {
        goto out_iomem_bad;
    }

    p_msp430Spi->virtual_spiBase
        = ioremap(BCM2836_SPI_BASE, BCM2836_SPI_LEN);
    if (NULL == p_msp430Spi->virtual_spiBase)
        goto out_remap_failed;
    // TODO: Add SPI Setup, first we are going to do timers.
    // Reset the buffer before writing:
    cs_register_buffer.full_buffer = 0;

    cs_register_buffer.csbits = 2;
    cs_register_buffer.cpha = 0;
    cs_register_buffer.cpol = 0;
    
    printk(KERN_INFO "CS Register is: %d\n", cs_register_buffer.full_buffer);
    return retVal;

out_remap_failed:
    printk(KERN_ALERT "IOremap failed!\n");
    release_mem_region(BCM2836_SPI_BASE, BCM2836_SPI_LEN);

out_iomem_bad:
    printk(KERN_ALERT "IO memory could not be allocated!\n");
    return -ENOMEM;
}

enum hrtimer_restart transfer_timer_isr(struct hrtimer* dataIn)
{
    struct msp430Spi* p_dev = container_of(dataIn, struct msp430Spi, transferTimer);
    kfifo_put(&p_dev->in_fifo, 255);
    hrtimer_forward_now(dataIn, p_dev->transferTimerInterval);
    return HRTIMER_RESTART;
}

static int setup_hres_abs_timer(struct hrtimer* timer, 
        enum hrtimer_restart (*p_timerFunction)(struct hrtimer* ),
        ktime_t* p_ktime_interval)
{
    hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
    
    if (p_timerFunction == NULL)
        goto out_bad;
    timer->function = p_timerFunction;
    
    if (p_ktime_interval == NULL)
        goto out_bad;

    *p_ktime_interval = ktime_set(0, TRANSFER_TIMER_NS_INTERVAL);
    hrtimer_start(timer, *p_ktime_interval, HRTIMER_MODE_ABS);
    
    return 0;

out_bad:
    return -EINVAL;
}

static int driver_entry(void)
{                                  
//    request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
//	    const char *name, void *dev)
    int retVal = 0;
    p_msp430Spi = kmalloc(sizeof(struct msp430Spi), GFP_KERNEL);
    
    retVal = kfifo_alloc(&(p_msp430Spi->in_fifo), DEVICE_IN_BUFFER_SIZE, GFP_KERNEL);
    if (retVal)
        goto out_fifo_bad;
    retVal = kfifo_alloc (&(p_msp430Spi->out_fifo), DEVICE_OUT_BUFFER_SIZE, GFP_KERNEL);
    if (retVal)
        goto out_fifo_bad;

    printk(KERN_INFO "Driver Entry called!\n");

    sema_init(&p_msp430Spi->deviceSem,1);  
    
    retVal = alloc_chrdev_region(&g_deviceNumber, FIRST_MINOR_NUMBER, 
            TOTAL_MINOR_NUMBERS, DEVICE_NAME);
    if (retVal < 0)
        goto out_major_number_bad;

    // Allocate cdev and add to the system
    g_deviceMajorNumber = MAJOR(g_deviceNumber);
    p_device = cdev_alloc(); 
    p_device->owner = THIS_MODULE;
    p_device->ops = &fops;

    retVal = cdev_add(p_device, g_deviceNumber, TOTAL_MINOR_NUMBERS);
    if (retVal < 0)
        goto out_cdev_bad;

    // Setup BCM2836 SPI peripheral
    if (0 > spi_peripheral_setup())
        goto out_spi_setup_bad;

    // Setup transfer timer
    if (0 > setup_hres_abs_timer(&(p_msp430Spi->transferTimer), &transfer_timer_isr, 
                &(p_msp430Spi->transferTimerInterval)))
        goto out_timer_setup_bad;

    // Return 0 if all is good:
    return 0; 

out_timer_setup_bad:
    printk(KERN_ALERT "Transfer timer setup failed!\n");

out_spi_setup_bad:
    printk(KERN_ALERT "SPI peripheral setup failed!\n");

out_cdev_bad:
    printk(KERN_ALERT "Failure to add cdev\n");
    unregister_chrdev_region(g_deviceNumber, TOTAL_MINOR_NUMBERS);
    
out_fifo_bad:
    printk(KERN_ALERT "Failure to allocate fifo!\n");
    kfifo_free(&p_msp430Spi->in_fifo);
    kfifo_free(&p_msp430Spi->out_fifo);

out_major_number_bad:
    printk(KERN_ALERT "Failure to allocate major number!\n");
    kfree(p_msp430Spi); 

    return retVal; 
}

static void driver_exit (void)
{
    printk(KERN_INFO "Driver Exit called\n");
    driver_cleanup();    
}

module_init(driver_entry);
module_exit(driver_exit);

// Licensing and author:
MODULE_AUTHOR(AUTHOR_HANDLE);
MODULE_LICENSE("GPL");
