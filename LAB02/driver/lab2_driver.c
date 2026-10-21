/*
 * lab2_driver.c - LAB-02: Simple Character Device Driver
 * Gồm: char device (/dev/lab2), procfs (/proc/lab2_info),
 *      sysfs (/sys/class/lab2_class/lab2/<attr>)
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Simple Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME  "lab2"
#define CLASS_NAME   "lab2_class"
#define BUFFER_SIZE  1024
#define DRIVER_MAJOR 240   /* 240-254: dải dành cho local/experimental */

static int major_number = DRIVER_MAJOR;
static char device_buffer[BUFFER_SIZE];   /* bộ đệm nội bộ của driver */
static int buffer_len = 0;                /* số byte hợp lệ trong buffer */
static int open_count = 0;                /* số lần thiết bị được open */
static struct class *lab2_class = NULL;
static struct device *lab2_device = NULL;

/*
 * Mutex bảo vệ device_buffer, buffer_len, open_count.
 * Nhiều process có thể mở /dev/lab2 cùng lúc và chạy song song trên
 * nhiều CPU (hoặc bị preempt giữa chừng). Không có mutex thì hai lần
 * write đồng thời có thể ghi chồng lên buffer, làm buffer_len không
 * khớp nội dung (race condition). Dùng mutex (không phải spinlock) vì
 * copy_from_user()/copy_to_user() có thể ngủ (page fault).
 */
static DEFINE_MUTEX(lab2_mutex);

/* ===== PROCFS: /proc/lab2_info ===== */
static struct proc_dir_entry *proc_entry;

/*
 * seq_file API: kernel lo việc phân trang, offset và cấp phát buffer.
 * Driver chỉ cần gọi seq_printf() trong hàm show, không phải tự xử lý
 * offset như hàm read thông thường.
 */
static int lab2_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "LAB-02 Driver Statistics\n");
    seq_printf(m, "======================\n");
    seq_printf(m, "Driver name  : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number : %d\n", major_number);
    seq_printf(m, "Buffer size  : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length  : %d bytes\n", buffer_len);
    seq_printf(m, "Open count   : %d\n", open_count);
    if (buffer_len > 0)
        seq_printf(m, "Last data    : [%s]\n", device_buffer);
    return 0;
}

/*
 * single_open(): gắn hàm show vào file, dùng cho entry chỉ in ra một
 * khối dữ liệu duy nhất (không phải danh sách nhiều phần tử).
 */
static int lab2_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ===== SYSFS: /sys/class/lab2_class/lab2/<attr> ===== */
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    if (buffer_len > 0)
        return sprintf(buf, "%s\n", device_buffer);
    return sprintf(buf, "(empty)\n");
}

/*
 * DEVICE_ATTR_RO(name) sinh ra biến dev_attr_<name> (mode 0444) và gắn
 * với hàm <name>_show. Mỗi attribute là một file trong thư mục sysfs của
 * device; mỗi thư mục sysfs tương ứng một kobject (đối tượng kernel có
 * đếm tham chiếu, cấu thành cây phân cấp /sys).
 */
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};

/* ===== file_operations ===== */

/* open: được gọi khi userspace open("/dev/lab2"). Tăng bộ đếm. */
static int lab2_open(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);
    open_count++;
    mutex_unlock(&lab2_mutex);
    pr_info("lab2_driver: device opened (count=%d)\n", open_count);
    return 0;
}

/* release: được gọi khi file descriptor cuối cùng được close. */
static int lab2_release(struct inode *inode, struct file *file)
{
    pr_info("lab2_driver: device closed\n");
    return 0;
}

/*
 * read: chuyển dữ liệu từ device_buffer ra userspace.
 * Phải dùng copy_to_user() thay vì memcpy() vì con trỏ buf là địa chỉ ảo
 * của process user, có thể chưa được map (page fault), sai quyền, hoặc
 * là địa chỉ trỏ vào vùng kernel do user cố tình truyền. copy_to_user()
 * kiểm tra tính hợp lệ của địa chỉ và xử lý page fault an toàn, trả về
 * số byte chưa chép được (khác 0 nghĩa là lỗi -> trả -EFAULT).
 * memcpy() không kiểm tra gì cả: địa chỉ xấu sẽ gây kernel oops/panic.
 */
static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    int bytes_read = 0;

    mutex_lock(&lab2_mutex);
    if (*offset >= buffer_len) {
        mutex_unlock(&lab2_mutex);
        return 0;                       /* EOF */
    }
    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&lab2_mutex);
        pr_err("lab2_driver: copy_to_user failed\n");
        return -EFAULT;
    }
    *offset += len;
    bytes_read = len;
    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: sent %d bytes to user\n", bytes_read);
    return bytes_read;
}

/*
 * write: nhận dữ liệu từ userspace vào device_buffer.
 * Dùng copy_from_user() vì cùng lý do như copy_to_user() ở trên.
 */
static ssize_t lab2_write(struct file *file, const char __user *buf,
                          size_t len, loff_t *offset)
{
    if (len > BUFFER_SIZE - 1) {
        pr_warn("lab2_driver: write too large, truncating\n");
        len = BUFFER_SIZE - 1;
    }

    mutex_lock(&lab2_mutex);
    if (copy_from_user(device_buffer, buf, len)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }
    buffer_len = len;
    device_buffer[buffer_len] = '\0';
    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: received %zu bytes: [%s]\n", len, device_buffer);
    return len;
}

static struct file_operations lab2_fops = {
    .owner   = THIS_MODULE,
    .open    = lab2_open,
    .release = lab2_release,
    .read    = lab2_read,
    .write   = lab2_write,
};

/* init: chạy khi insmod. Đăng ký chrdev, tạo class/device, sysfs, procfs. */
static int __init lab2_init(void)
{
    int ret;

    pr_info("lab2_driver: initializing module\n");

    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }

    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_class);
    }

    lab2_device = device_create(lab2_class, NULL,
                                MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(lab2_device)) {
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_device);
    }

    ret = sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
    if (ret)
        pr_warn("lab2_driver: sysfs_create_group failed\n");

    proc_entry = proc_create("lab2_info", 0444, NULL, &lab2_proc_fops);
    if (!proc_entry)
        pr_warn("lab2_driver: failed to create /proc/lab2_info\n");
    else
        pr_info("lab2_driver: /proc/lab2_info created\n");

    pr_info("lab2_driver: loaded, major=%d\n", major_number);
    return 0;
}

/* exit: chạy khi rmmod. Gỡ mọi thứ theo thứ tự ngược với init. */
static void __exit lab2_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);
    sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
