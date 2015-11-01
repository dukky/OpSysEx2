/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <charDeviceDriver.h>
#include <linux/kfifo.h>
#include "ioctl.h"

MODULE_LICENSE("GPL");

/*
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */


DEFINE_MUTEX  (devLock);
static int counter = 0;

static struct kfifo message_buffer;
static struct kfifo message_size_buffer;

static long device_ioctl(struct file *file,	/* see include/linux/fs.h */
		 unsigned int ioctl_num,	/* number and param for ioctl */
		 unsigned long ioctl_param)
{

	/*
	 * Switch according to the ioctl called
	 */
	if (ioctl_num == RESET_COUNTER) {
	    counter = 0;
	    /* 	    return 0; */
	    return 5; /* can pass integer as return value */
	}

	else {
	    /* no operation defined - return failure */
	    return -EINVAL;

	}
}


/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
	int message_buffer_result;
	int message_size_buffer_result;
  Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	message_buffer_result = kfifo_alloc(&message_buffer, INITIAL_KFIFO_SIZE, GFP_KERNEL);
	message_size_buffer_result = kfifo_alloc(&message_size_buffer, INITIAL_KFIFO_SIZE, GFP_KERNEL);

	// Check if allocating buffers worked
	if(message_buffer_result !=0) {
		printk(KERN_ERR "Error allocating message buffer\n");
		return -EAGAIN;
	}

	if(message_size_buffer_result !=0) {
		printk(KERN_ERR "Error allocating message size buffer\n");
		return -EAGAIN;
	}

	return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
	/*  Unregister the device */
	unregister_chrdev(Major, DEVICE_NAME);
	kfifo_free(&message_buffer);
	kfifo_free(&message_size_buffer);
}

/*
 * Methods
 */

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{

		printk(KERN_INFO "Device open:%d\n", Device_Open);
    mutex_lock (&devLock);
    if (Device_Open) {
			mutex_unlock (&devLock);
			return -EBUSY;
    }
    Device_Open++;
    mutex_unlock (&devLock);
    // sprintf(msg, "I already told you %d times Hello world!\n", counter++);
    // msg_Ptr = msg;
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
	mutex_lock (&devLock);
	Device_Open--;		/* We're now ready for our next caller */
	mutex_unlock (&devLock);
	/*
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module.
	 */
	module_put(THIS_MODULE);

	return 0;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{

	/*
	 * Number of bytes actually written to the buffer
	 */
	int bytes_read = 0;
	int message_size;

	if(kfifo_is_empty(&message_buffer)) {
		return -EAGAIN;
	} else {
		if(kfifo_is_empty(&message_size_buffer)) {
			printk(KERN_ERR "Message size buffer unexpectly empty\n");
			return -EAGAIN;
		}
		kfifo_out(&message_size_buffer, &message_size, 4);
		kfifo_to_user(&message_buffer, buffer, message_size, &bytes_read);
	}


	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	int bytes_read;
	int ret;
	if(kfifo_avail(&message_buffer) < len) {
		printk(KERN_INFO "Not enough space in message buffer");
		return -EAGAIN;
	} else if(len > BUF_LEN) {
		printk(KERN_INFO "Message too long");
		return -EINVAL;
	} else {
		ret = kfifo_from_user(&message_buffer, buff, len, &bytes_read);
		if (ret !=0 || len != bytes_read) {
			printk(KERN_INFO "Error in kfifo_from_user");
			return -EAGAIN;
		} else {
			// Success reading
			kfifo_in(&message_size_buffer, &bytes_read, 4);
			printk(KERN_INFO "Successfull");
			return bytes_read;
		}

	}
}
