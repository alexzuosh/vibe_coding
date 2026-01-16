#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Zuo");
MODULE_DESCRIPTION("A simple Lite GPU kernel module");
MODULE_VERSION("0.1");

static int __init lite_gpu_init(void) {
    printk(KERN_INFO "lite_gpu: Module initialized\n");
    return 0;
}

static void __exit lite_gpu_exit(void) {
    printk(KERN_INFO "lite_gpu: Module unloaded\n");
}

module_init(lite_gpu_init);
module_exit(lite_gpu_exit);
