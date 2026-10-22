#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hoang Minh Nhat - SE204273");
MODULE_DESCRIPTION("ASSIGNMENT-01 Task 2: Misc Device Counter Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME "counter"

static atomic_t counter_val  = ATOMIC_INIT(0);
static atomic_t total_reads  = ATOMIC_INIT(0);
static atomic_t total_resets = ATOMIC_INIT(0);
static struct proc_dir_entry *proc_entry = NULL;

static ssize_t counter_read(struct file *file, char __user *buf, size_t len, loff_t *ppos) {
    char kbuf[32];
    int val, slen;
    if (*ppos > 0) return 0;
    val = atomic_read(&counter_val);
    atomic_inc(&counter_val);
    atomic_inc(&total_reads);
    slen = snprintf(kbuf, sizeof(kbuf), "%d\n", val);
    if (len < slen) slen = len;
    if (copy_to_user(buf, kbuf, slen)) return -EFAULT;
    *ppos += slen;
    pr_info("counter_driver: read value=%d (next=%d)\n", val, atomic_read(&counter_val));
    return slen;
}

static ssize_t counter_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos) {
    char kbuf[32];
    int new_val, ret;
    if (len == 0 || len >= sizeof(kbuf)) return -EINVAL;
    if (copy_from_user(kbuf, buf, len)) return -EFAULT;
    kbuf[len] = '\0';
    ret = kstrtoint(strim(kbuf), 10, &new_val);
    if (ret < 0 || new_val < 0) return -EINVAL;
    atomic_set(&counter_val, new_val);
    atomic_inc(&total_resets);
    pr_info("counter_driver: counter set to %d\n", new_val);
    return len;
}

static const struct file_operations counter_fops = {
    .owner = THIS_MODULE, .read = counter_read, .write = counter_write,
};

static struct miscdevice counter_misc = {
    .minor = MISC_DYNAMIC_MINOR, .name = DEVICE_NAME, .fops = &counter_fops,
};

static int counter_proc_show(struct seq_file *m, void *v) {
    seq_printf(m, "=== Counter Device Info ===\n");
    seq_printf(m, "Current Counter : %d\n", atomic_read(&counter_val));
    seq_printf(m, "Total Reads     : %d\n", atomic_read(&total_reads));
    seq_printf(m, "Total Resets    : %d\n", atomic_read(&total_resets));
    return 0;
}

static int counter_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, counter_proc_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
    .proc_open = counter_proc_open, .proc_read = seq_read,
    .proc_lseek = seq_lseek, .proc_release = single_release,
};

static int __init counter_init(void) {
    int ret = misc_register(&counter_misc);
    if (ret) return ret;
    proc_entry = proc_create("counter_info", 0444, NULL, &counter_proc_ops);
    pr_info("counter_driver: registered /dev/counter (minor=%d)\n", counter_misc.minor);
    return 0;
}

static void __exit counter_exit(void) {
    if (proc_entry) proc_remove(proc_entry);
    misc_deregister(&counter_misc);
    pr_info("counter_driver: unloaded\n");
}
module_init(counter_init);
module_exit(counter_exit);
