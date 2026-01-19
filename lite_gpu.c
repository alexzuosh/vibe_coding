#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>

#include "lite_uapi.h"

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

struct lite_device {
    struct drm_device drm;
    struct pci_dev *pdev;
    struct ttm_device ttm;
};

struct lite_gem_object {
    struct drm_gem_object base;
    struct ttm_buffer_object bo;
};

static struct ttm_tt *lite_ttm_tt_create(struct ttm_buffer_object *bo, uint32_t page_flags)
{
    struct ttm_tt *tt;
    tt = kzalloc(sizeof(*tt), GFP_KERNEL);
    if (!tt)
        return NULL;
    
    if (ttm_tt_init(tt, bo, page_flags, ttm_cached)) {
        kfree(tt);
        return NULL;
    }
    return tt;
}

static void lite_ttm_tt_destroy(struct ttm_device *bdev, struct ttm_tt *tt)
{
    ttm_tt_fini(tt);
    kfree(tt);
}

static struct ttm_device_funcs lite_ttm_funcs = {
    .ttm_tt_create = lite_ttm_tt_create,
    .ttm_tt_destroy = lite_ttm_tt_destroy,
};

static int lite_ttm_init(struct lite_device *ldev)
{
    int ret;
    
    ret = ttm_device_init(&ldev->ttm, &lite_ttm_funcs, ldev->drm.dev,
                          ldev->drm.anon_inode->i_mapping,
                          ldev->drm.vma_offset_manager,
                          false, true);
    return ret;
}


static int lite_ioctl_gem_create(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_gem_create *args = data;
    /* Todo: Implement allocation */
    return 0;
}

static int lite_ioctl_gem_map(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_gem_map *args = data;
    /* Todo: Implement mapping */
    return 0;
}

static int lite_ioctl_get_param(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_get_param *args = data;
    /* Todo: Implement get param */
    return 0;
}

static const struct drm_ioctl_desc lite_ioctls[] = {
    DRM_IOCTL_DEF_DRV(LITE_GEM_CREATE, lite_ioctl_gem_create, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(LITE_GEM_MAP, lite_ioctl_gem_map, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(LITE_GET_PARAM, lite_ioctl_get_param, DRM_RENDER_ALLOW),
};

static const struct file_operations lite_gpu_fops = {
    .owner = THIS_MODULE,
    .open = drm_open,
    .release = drm_release,
    .unlocked_ioctl = drm_ioctl,
    .mmap = drm_gem_mmap,
};

static struct drm_driver lite_drm_driver = {
    .driver_features = DRIVER_GEM | DRIVER_RENDER,
    .fops = &lite_gpu_fops,
    .ioctls = lite_ioctls,
    .num_ioctls = ARRAY_SIZE(lite_ioctls),
    .name = "lite_gpu",
    .desc = "Lite GPU Driver",
    .date = "20240118",
    .major = 0,
    .minor = 1,
};

static int lite_gpu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct lite_device *ldev;
    int ret;

    printk(KERN_INFO "lite_gpu: Probing device\n");

    ret = pci_enable_device(pdev);
    if (ret)
        return ret;

    ldev = devm_drm_dev_alloc(&pdev->dev, &lite_drm_driver, struct lite_device, drm);
    if (IS_ERR(ldev))
        return PTR_ERR(ldev);

    ldev->pdev = pdev;
    pci_set_drvdata(pdev, ldev);

    ret = lite_ttm_init(ldev);
    if (ret)
        return ret;

    ret = drm_dev_register(&ldev->drm, 0);
    if (ret)
        return ret;

    pci_set_master(pdev);

    /* Runtime PM Initialization */
    pm_runtime_set_autosuspend_delay(&pdev->dev, 1000); /* 1 second delay */
    pm_runtime_use_autosuspend(&pdev->dev);
    pm_runtime_set_active(&pdev->dev);
    pm_runtime_allow(&pdev->dev);
    pm_runtime_enable(&pdev->dev);

    printk(KERN_INFO "lite_gpu: DRM device registered\n");

    return 0;
}

static void lite_gpu_remove(struct pci_dev *pdev)
{
    struct lite_device *ldev = pci_get_drvdata(pdev);

    /* Runtime PM Teardown */
    pm_runtime_disable(&pdev->dev);
    pm_runtime_forbid(&pdev->dev);
    
    drm_dev_unregister(&ldev->drm);
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
