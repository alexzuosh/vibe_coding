#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Zuo");
MODULE_DESCRIPTION("A simple Lite GPU kernel module");
MODULE_VERSION("0.1");

#define LITE_GPU_VENDOR_ID 0x1ED5  // Example Vendor ID
#define LITE_GPU_DEVICE_ID 0x1000  // Example Device ID

static const struct pci_device_id lite_gpu_ids[] = {
    { PCI_DEVICE(LITE_GPU_VENDOR_ID, LITE_GPU_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, lite_gpu_ids);

static int lite_gpu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int ret;

    printk(KERN_INFO "lite_gpu: Probing device\n");

    ret = pci_enable_device(pdev);
    if (ret) {
        printk(KERN_ERR "lite_gpu: Failed to enable PCI device\n");
        return ret;
    }

    pci_set_master(pdev);

    /* Runtime PM Initialization */
    pm_runtime_set_autosuspend_delay(&pdev->dev, 1000); /* 1 second delay */
    pm_runtime_use_autosuspend(&pdev->dev);
    pm_runtime_set_active(&pdev->dev);
    pm_runtime_allow(&pdev->dev);
    pm_runtime_enable(&pdev->dev);

    printk(KERN_INFO "lite_gpu: Runtime PM enabled\n");

    return 0;
}

static void lite_gpu_remove(struct pci_dev *pdev)
{
    /* Runtime PM Teardown */
    pm_runtime_disable(&pdev->dev);
    pm_runtime_forbid(&pdev->dev);
    
    pci_disable_device(pdev);
    printk(KERN_INFO "lite_gpu: Device removed\n");
}

static struct pci_driver lite_gpu_driver = {
    .name = "lite_gpu",
    .id_table = lite_gpu_ids,
    .probe = lite_gpu_probe,
    .remove = lite_gpu_remove,
};

static int __init lite_gpu_init(void) {
    printk(KERN_INFO "lite_gpu: Module initialized\n");
    return pci_register_driver(&lite_gpu_driver);
}

static void __exit lite_gpu_exit(void) {
    pci_unregister_driver(&lite_gpu_driver);
    printk(KERN_INFO "lite_gpu: Module unloaded\n");
}

module_init(lite_gpu_init);
module_exit(lite_gpu_exit);
