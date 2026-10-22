#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "asgn1_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hoang Minh Nhat - SE204273");
MODULE_DESCRIPTION("ASSIGNMENT-01 Task 1: Character Driver with ioctl interface");
MODULE_VERSION("1.0");

#define DEVICE_NAME     "asgn1"
#define CLASS_NAME      "asgn1_class"
#define BUFFER_SIZE     1024
#define DRIVER_MAJOR    245

static char   device_buffer[BUFFER_SIZE];
static char   tmp_buf[BUFFER_SIZE];
static int    buffer_len = 0;
static int    open_count = 0;
static int    write_count = 0;
static int    read_count = 0;
static int    last_write_size = 0;
static int    current_mode = 1; /* Default Mode 1: Echo mode */

static struct class  *asgn1_class  = NULL;
static struct device *asgn1_device = NULL;
static DEFINE_MUTEX(asgn1_mutex);

/*
 * asgn1_open - Open device and increment open_count
 * @inode: Pointer to inode structure
 * @file: Pointer to file structure
 * Return: 0 on success
 */
static int asgn1_open(struct inode *inode, struct file *file) {
    mutex_lock(&asgn1_mutex);
    open_count++;
    mutex_unlock(&asgn1_mutex);
    pr_info("asgn1_driver: device opened (open_count=%d)\n", open_count);
    return 0;
}

/*
 * asgn1_release - Close device
 * @inode: Pointer to inode structure
 * @file: Pointer to file structure
 * Return: 0 on success
 */
static int asgn1_release(struct inode *inode, struct file *file) {
    pr_info("asgn1_driver: device closed\n");
    return 0;
}

/*
 * asgn1_read - Read data from internal buffer to userspace
 * @file: Pointer to file structure
 * @buf: Userspace buffer
 * @len: Requested read length
 * @offset: Current file offset
 * Return: Number of bytes read, 0 on EOF, or negative error code
 */
static ssize_t asgn1_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    int bytes_read = 0;

    mutex_lock(&asgn1_mutex);
    if (*offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;
    }
    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }
    *offset += len;
    bytes_read = len;
    read_count++;
    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: read %d bytes (read_count=%d)\n", bytes_read, read_count);
    return bytes_read;
}

/*
 * asgn1_write - Write data from userspace to internal buffer based on mode
 * @file: Pointer to file structure
 * @buf: Userspace buffer
 * @len: Data length
 * @offset: Current file offset
 * Return: Number of bytes written or negative error code
 */
static ssize_t asgn1_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    size_t copy_len = (len > BUFFER_SIZE - 16) ? (BUFFER_SIZE - 16) : len;

    mutex_lock(&asgn1_mutex);
    if (copy_from_user(tmp_buf, buf, copy_len)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }
    tmp_buf[copy_len] = '\0';

    if (current_mode == 0) {
        buffer_len = snprintf(device_buffer, BUFFER_SIZE, "[KERNEL]%s", tmp_buf);
    } else {
        memcpy(device_buffer, tmp_buf, copy_len);
        device_buffer[copy_len] = '\0';
        buffer_len = copy_len;
    }
    write_count++;
    last_write_size = buffer_len;
    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: wrote %d bytes in mode %d\n", buffer_len, current_mode);
    return len;
}

/*
 * asgn1_unlocked_ioctl - Handle 4 ioctl commands from userspace
 * @file: Pointer to file structure
 * @cmd: ioctl command code
 * @arg: Userspace argument
 * Return: 0 on success, negative error code on failure
 */
static long asgn1_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case ASGN1_RESET_BUFFER:
            mutex_lock(&asgn1_mutex);
            memset(device_buffer, 0, BUFFER_SIZE);
            buffer_len = 0;
            write_count = 0;
            read_count = 0;
            last_write_size = 0;
            mutex_unlock(&asgn1_mutex);
            pr_info("asgn1_driver: ioctl ASGN1_RESET_BUFFER executed\n");
            break;

        case ASGN1_GET_STATS: {
            struct asgn1_stats st;
            mutex_lock(&asgn1_mutex);
            st.open_count      = open_count;
            st.write_count     = write_count;
            st.read_count      = read_count;
            st.buffer_len      = buffer_len;
            st.last_write_size = last_write_size;
            mutex_unlock(&asgn1_mutex);

            if (copy_to_user((struct asgn1_stats __user *)arg, &st, sizeof(st)))
                return -EFAULT;
            pr_info("asgn1_driver: ioctl ASGN1_GET_STATS executed\n");
            break;
        }

        case ASGN1_SET_MODE: {
            int mode;
            if (copy_from_user(&mode, (int __user *)arg, sizeof(int)))
                return -EFAULT;
            if (mode != 0 && mode != 1)
                return -EINVAL;

            mutex_lock(&asgn1_mutex);
            current_mode = mode;
            mutex_unlock(&asgn1_mutex);
            pr_info("asgn1_driver: ioctl ASGN1_SET_MODE set to %d\n", mode);
            break;
        }

        case ASGN1_GET_VERSION: {
            char ver[64] = {0};
            snprintf(ver, sizeof(ver), "asgn1_driver v1.0 - SE204273");
            if (copy_to_user((char __user *)arg, ver, sizeof(ver)))
                return -EFAULT;
            pr_info("asgn1_driver: ioctl ASGN1_GET_VERSION executed\n");
            break;
        }

        default:
            pr_warn("asgn1_driver: invalid ioctl command 0x%x\n", cmd);
            return -ENOTTY;
    }
    return 0;
}

static struct file_operations asgn1_fops = {
    .owner          = THIS_MODULE,
    .open           = asgn1_open,
    .release        = asgn1_release,
    .read           = asgn1_read,
    .write          = asgn1_write,
    .unlocked_ioctl = asgn1_unlocked_ioctl,
};

/*
 * asgn1_init - Module initialization
 * Return: 0 on success, negative error code on failure
 */
static int __init asgn1_init(void) {
    int ret = register_chrdev(DRIVER_MAJOR, DEVICE_NAME, &asgn1_fops);
    if (ret < 0)
        return ret;

    asgn1_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(asgn1_class)) {
        unregister_chrdev(DRIVER_MAJOR, DEVICE_NAME);
        return PTR_ERR(asgn1_class);
    }

    asgn1_device = device_create(asgn1_class, NULL, MKDEV(DRIVER_MAJOR, 0), NULL, DEVICE_NAME);
    if (IS_ERR(asgn1_device)) {
        class_destroy(asgn1_class);
        unregister_chrdev(DRIVER_MAJOR, DEVICE_NAME);
        return PTR_ERR(asgn1_device);
    }

    pr_info("asgn1_driver: module loaded (major=%d)\n", DRIVER_MAJOR);
    return 0;
}

/*
 * asgn1_exit - Module cleanup
 */
static void __exit asgn1_exit(void) {
    device_destroy(asgn1_class, MKDEV(DRIVER_MAJOR, 0));
    class_destroy(asgn1_class);
    unregister_chrdev(DRIVER_MAJOR, DEVICE_NAME);
    pr_info("asgn1_driver: module unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
