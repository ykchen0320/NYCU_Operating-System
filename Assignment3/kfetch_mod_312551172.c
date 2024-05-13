#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/utsname.h>
#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE (1 << 0)
#define KFETCH_NUM_CPUS (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM (1 << 3)
#define KFETCH_UPTIME (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)
#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1);
int major, minor, mask1 = 63, count = 0;
dev_t devid;
struct cdev dev;
static struct class *cls;
static atomic_t already_open = ATOMIC_INIT(0);
char kfetch_buf[1024];
static void penguin(void) {
  switch (count) {
    case 0:
      strncat(kfetch_buf, "                   ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
    case 1:
      strncat(kfetch_buf, "\n        .-.        ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
    case 2:
      strncat(kfetch_buf, "\n       (.. |       ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
    case 3:
      strncat(kfetch_buf, "\n       <>  |       ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
    case 4:
      strncat(kfetch_buf, "\n      / --- \\      ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
    case 5:
      strncat(kfetch_buf, "\n     ( |   | |     ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
    case 6:
      strncat(kfetch_buf, "\n   |\\\\_)___/\\)/\\   ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
    case 7:
      strncat(kfetch_buf, "\n  <__)------(__/   ",
              sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
      break;
  }
  count++;
}
static void get_hostname(void) {
  char temp_c[sizeof(utsname()->nodename)];
  strncpy(temp_c, utsname()->nodename, sizeof(utsname()->nodename));
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
  penguin();
  for (int i = 0; i < sizeof(temp_c) - 1; i++) {
    if (temp_c[i] == '\x00') break;
    temp_c[i] = '-';
  }
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
  penguin();
}
static void get_release(void) {
  char temp_c[] = "Kernel:   ";
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
  strncat(kfetch_buf, utsname()->release,
          sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
}
static void get_num_cpus(void) {
  strncat(kfetch_buf,
          "CPUs:     ", sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
  int temp = cpumask_weight(cpu_present_mask);
  char temp_c[3];
  snprintf(temp_c, sizeof(temp_c), "%d", temp);
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
  strncat(kfetch_buf, " / ", sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
  temp = cpumask_weight(cpu_possible_mask);
  snprintf(temp_c, sizeof(temp_c), "%d", temp);
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
}
static void get_cpu_model(void) {
  char temp_c[] = "CPU:      ";
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
}
static void get_mem(void) {
  char temp_c[] = "Mem:      ";
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
}
static void get_uptime(void) {
  char temp_c[] = "Uptime:   ";
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
}
static void get_num_proc(void) {
  char temp_c[] = "Procs:    ";
  strncat(kfetch_buf, temp_c, sizeof(kfetch_buf) - strlen(kfetch_buf) - 1);
}
static ssize_t kfetch_read(struct file *filp, char __user *buffer,
                           size_t length, loff_t *offset) {
  // fetching the information
  penguin();
  get_hostname();
  if (mask1 & 1) {
    get_release();
    penguin();
  }
  if (mask1 & 4) {
    get_cpu_model();
    penguin();
  }
  if (mask1 & 2) {
    get_num_cpus();
    penguin();
  }
  if (mask1 & 8) {
    get_mem();
    penguin();
  }
  if (mask1 & 32) {
    get_num_proc();
    penguin();
  }
  if (mask1 & 16) get_uptime();
  while (count < 8) penguin();
  int len = sizeof(kfetch_buf);
  if (copy_to_user(buffer, kfetch_buf, len)) {
    pr_alert("Failed to copy data to user");
    return 0;
  }
  // cleaning up
  memset(kfetch_buf, '\0', len);
  count = 0;
  return len;
}
static ssize_t kfetch_write(struct file *filp, const char __user *buffer,
                            size_t length, loff_t *offset) {
  int mask_info;
  if (copy_from_user(&mask_info, buffer, length)) {
    pr_alert("Failed to copy data from user");
    return 0;
  }
  // setting the information mask
  mask1 = mask_info;
  return 0;
}
int kfetch_open(struct inode *inode, struct file *filp) {
  if (atomic_cmpxchg(&already_open, 0, 1)) return -EBUSY;
  return 0;
}
int kfetch_release(struct inode *inode, struct file *filp) {
  atomic_set(&already_open, 0);
  return 0;
}
const static struct file_operations kfetch_ops = {
    .owner = THIS_MODULE,
    .read = kfetch_read,
    .write = kfetch_write,
    .open = kfetch_open,
    .release = kfetch_release,
};
static int __init kfetch_init(void) {
  major = register_chrdev(0, "kfetch", &kfetch_ops);
  cls = class_create(THIS_MODULE, "kfetch");
  device_create(cls, NULL, MKDEV(major, 0), NULL, "kfetch");
  return 0;
}
static void __exit kfetch_exit(void) {
  device_destroy(cls, MKDEV(major, 0));
  class_destroy(cls);
  unregister_chrdev(major, "kfetch");
}
module_init(kfetch_init);
module_exit(kfetch_exit);
MODULE_LICENSE("GPL");