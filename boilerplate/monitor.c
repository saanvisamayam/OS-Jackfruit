#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/device.h>

#define DEVICE_NAME "container_monitor"
#define CLASS_NAME  "container_class"

// ================= STRUCT =================
struct container_entry {
    pid_t pid;
    unsigned long soft_limit;
    unsigned long hard_limit;
    int warned;

    struct list_head list;
};

// ================= GLOBALS =================
static LIST_HEAD(container_list);
static DEFINE_MUTEX(container_lock);
static struct timer_list monitor_timer;

static int major;
static struct class *monitor_class = NULL;
static struct device *monitor_device = NULL;

// ================= MEMORY =================
#include <linux/mm.h>
#include <linux/mm_types.h>

static unsigned long get_rss_pages(struct task_struct *task) {
    struct mm_struct *mm = task->mm;

    if (!mm)
        return 0;

    return get_mm_rss(mm);
}
// ================= TIMER =================
static void monitor_fn(struct timer_list *t)
{
    struct container_entry *entry, *tmp;

    mutex_lock(&container_lock);

    list_for_each_entry_safe(entry, tmp, &container_list, list) {

        struct task_struct *task =
            pid_task(find_vpid(entry->pid), PIDTYPE_PID);

        if (!task) {
            list_del(&entry->list);
            kfree(entry);
            continue;
        }

        unsigned long rss = get_rss_pages(task);

        if (rss > entry->soft_limit && !entry->warned) {
            printk(KERN_INFO "[monitor] PID %d exceeded soft limit\n", entry->pid);
            entry->warned = 1;
        }

        if (rss > entry->hard_limit) {
            printk(KERN_INFO "[monitor] PID %d killed (hard limit)\n", entry->pid);
            send_sig(SIGKILL, task, 0);

            list_del(&entry->list);
            kfree(entry);
        }
    }

    mutex_unlock(&container_lock);

    mod_timer(&monitor_timer, jiffies + msecs_to_jiffies(2000));
}

// ================= IOCTL =================
struct monitor_request {
    pid_t pid;
    unsigned long soft;
    unsigned long hard;
};

#define IOCTL_ADD _IOW('a', 'a', struct monitor_request *)

static long monitor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct monitor_request req;

    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    struct container_entry *entry =
        kmalloc(sizeof(*entry), GFP_KERNEL);

    if (!entry)
        return -ENOMEM;

    entry->pid = req.pid;
entry->soft_limit = (req.soft * 1024 * 1024) / PAGE_SIZE;
entry->hard_limit = (req.hard * 1024 * 1024) / PAGE_SIZE;
    entry->warned = 0;

    mutex_lock(&container_lock);
    list_add(&entry->list, &container_list);
    mutex_unlock(&container_lock);

    printk(KERN_INFO "[monitor] Registered PID %d\n", req.pid);

    return 0;
}

// ================= FILE OPS =================
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = monitor_ioctl,
};

// ================= INIT =================
static int __init monitor_init(void)
{
    printk(KERN_INFO "[monitor] module loaded\n");

    // device setup
    major = register_chrdev(0, DEVICE_NAME, &fops);

    monitor_class = class_create(CLASS_NAME);
    monitor_device = device_create(monitor_class, NULL,
                                   MKDEV(major, 0),
                                   NULL,
                                   DEVICE_NAME);

    // timer
    timer_setup(&monitor_timer, monitor_fn, 0);
    mod_timer(&monitor_timer, jiffies + msecs_to_jiffies(2000));

    return 0;
}

// ================= EXIT =================
static void __exit monitor_exit(void)
{
    struct container_entry *entry, *tmp;

    timer_shutdown_sync(&monitor_timer);

    mutex_lock(&container_lock);

    list_for_each_entry_safe(entry, tmp, &container_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }

    mutex_unlock(&container_lock);

    device_destroy(monitor_class, MKDEV(major, 0));
    class_destroy(monitor_class);
    unregister_chrdev(major, DEVICE_NAME);

    printk(KERN_INFO "[monitor] module unloaded\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");
