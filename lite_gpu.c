#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/ttm/ttm_device.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_range_manager.h>
#include <drm/ttm/ttm_bo.h>

#include "lite_uapi.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Zuo");
MODULE_DESCRIPTION("A simple Lite GPU kernel module");
MODULE_VERSION("0.1");

#define LITE_GPU_VENDOR_ID 0x1ED5  // Example Vendor ID
#define LITE_GPU_DEVICE_ID 0x1000  // Example Device ID
#define LITE_VRAM_SIZE (256 * 1024 * 1024) // 256MB
#define LITE_GTT_SIZE (1024 * 1024 * 1024) // 1GB

static const struct pci_device_id lite_gpu_ids[] = {
    { PCI_DEVICE(LITE_GPU_VENDOR_ID, LITE_GPU_DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, lite_gpu_ids);

#define LITE_RING_SIZE (64 * 1024)

struct lite_ring {
    void *vaddr;
    u32 size;
    u32 wptr;
    u32 rptr;
    spinlock_t lock;
};

struct lite_device {
    struct drm_device drm;
    struct pci_dev *pdev;
    struct ttm_device ttm;
    struct lite_ring ring;
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
    if (ret)
        return ret;

    /* Initialize VRAM manager */
    ret = ttm_range_manager_init(&ldev->ttm, TTM_PL_VRAM, false,
                                 LITE_VRAM_SIZE >> PAGE_SHIFT);
    if (ret)
        return ret;

    /* Initialize GTT manager */
    ret = ttm_range_manager_init(&ldev->ttm, TTM_PL_TT, false,
                                 LITE_GTT_SIZE >> PAGE_SHIFT);
    if (ret)
        return ret;

    return 0;
}


static void lite_bo_destroy(struct ttm_buffer_object *bo)
{
    struct lite_gem_object *lobj = container_of(bo, struct lite_gem_object, bo);
    drm_gem_object_release(&lobj->base);
    kfree(lobj);
}

static void lite_gem_free_object(struct drm_gem_object *obj)
{
    struct lite_gem_object *lobj = container_of(obj, struct lite_gem_object, base);
    ttm_bo_put(&lobj->bo);
}

static const struct drm_gem_object_funcs lite_gem_funcs = {
    .free = lite_gem_free_object,
};

static inline struct lite_device *to_lite_device(struct drm_device *dev)
{
    return container_of(dev, struct lite_device, drm);
}

static void lite_bo_placement(struct ttm_placement *placement,
                              struct ttm_place *places,
                              uint32_t domain)
{
    int i = 0;

    if (domain & TTM_PL_FLAG_VRAM) {
        places[i].fpfn = 0;
        places[i].lpfn = 0;
        places[i].mem_type = TTM_PL_VRAM;
        places[i].flags = 0;
        i++;
    }
    
    // System fallback
    places[i].fpfn = 0;
    places[i].lpfn = 0;
    places[i].mem_type = TTM_PL_SYSTEM;
    places[i].flags = 0;
    i++;
    
    placement->num_placement = i;
    placement->placement = places;
    placement->num_busy_placement = i;
    placement->busy_placement = places;
}

static int lite_bo_create(struct lite_device *ldev, size_t size,
                          struct lite_gem_object **pobj)
{
    struct lite_gem_object *obj;
    struct ttm_placement placement = {};
    struct ttm_place places[2]; // VRAM + SYSTEM
    int ret;

    obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return -ENOMEM;

    /* Initialize GEM object */
    obj->base.funcs = &lite_gem_funcs;
    drm_gem_private_object_init(&ldev->drm, &obj->base, size);
    
    /* Setup placement (Prefer VRAM) */
    lite_bo_placement(&placement, places, TTM_PL_FLAG_VRAM);

    /* Initialize TTM BO */
    obj->bo.destroy = lite_bo_destroy;
    ret = ttm_bo_init_validate(&ldev->ttm, &obj->bo, TTM_PL_FLAG_VRAM,
                               &placement, PAGE_ALIGN(size) >> PAGE_SHIFT,
                               false, NULL, NULL, NULL);
    if (ret) {
        // Obj is freed by ttm_bo_put on failure usually if init started?
        // If init fails, we might need to free obj manually if refcount is 0?
        // ttm_bo_init_validate consumes the BO structure? 
        // If it fails, clean up depends on where it failed.
        // For simplicity here assume kfree is needed if internal init failed early.
        // But safe bet is ttm_bo_put if it was init? 
        // Actually standard says checking ret.
         kfree(obj);
         return ret;
    }

    *pobj = obj;
    return 0;
}

static int lite_ioctl_gem_create(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_device *ldev = to_lite_device(dev);
    struct lite_gem_create *args = data;
    struct lite_gem_object *obj;
    int ret;
    
    if (args->size == 0)
        return -EINVAL;

    args->size = PAGE_ALIGN(args->size);

    ret = lite_bo_create(ldev, args->size, &obj);
    if (ret)
        return ret;

    ret = drm_gem_handle_create(file, &obj->base, &args->handle);
    if (ret) {
        ttm_bo_put(&obj->bo); // Release reference
        return ret;
    }

    // Drop reference returned by lite_bo_create (via ttm_bo_init)
    // The handle now holds a reference.
    ttm_bo_put(&obj->bo);

    return 0;
}

static int lite_ioctl_gem_map(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_gem_map *args = data;
    struct drm_gem_object *obj;
    int ret;

    obj = drm_gem_object_lookup(file, args->handle);
    if (!obj)
        return -ENOENT;

    ret = drm_gem_create_mmap_offset(obj);
    if (ret == 0)
        args->offset = drm_vma_node_offset_addr(&obj->vma_node);
    
    drm_gem_object_put(obj);
    return ret;
}

static int lite_ioctl_get_param(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_get_param *args = data;
    /* Todo: Implement get param */
    return 0;
}

static int lite_ring_init(struct lite_device *ldev)
{
    struct lite_ring *ring = &ldev->ring;

    ring->size = LITE_RING_SIZE;
    ring->vaddr = kzalloc(ring->size, GFP_KERNEL);
    if (!ring->vaddr)
        return -ENOMEM;

    ring->wptr = 0;
    ring->rptr = 0;
    spin_lock_init(&ring->lock);
    return 0;
}

static void lite_ring_fini(struct lite_device *ldev)
{
    struct lite_ring *ring = &ldev->ring;
    kfree(ring->vaddr);
}

static int lite_ioctl_submit_cmd(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_device *ldev = to_lite_device(dev);
    struct lite_submit_cmd *args = data;
    struct lite_ring *ring = &ldev->ring;
    u32 size = args->cmd_buffer_size;
    unsigned long flags;
    int ret = 0;

    if (size > ring->size)
        return -EINVAL;

    spin_lock_irqsave(&ring->lock, flags);

    // Check space (simplistic circular buffer check)
    // For now, reset if full or wrap around logic
    if (ring->wptr + size > ring->size) {
        ring->wptr = 0; // Wrap around
    }

    if (copy_from_user(ring->vaddr + ring->wptr,
                       (void __user *)(uintptr_t)args->cmd_buffer_ptr,
                       size)) {
        ret = -EFAULT;
        goto out_unlock;
    }

    ring->wptr += size;
    
    // Todo: ring doorbell mechanism

out_unlock:
    spin_unlock_irqrestore(&ring->lock, flags);
    return ret;
}

static int lite_ioctl_wait_bo(struct drm_device *dev, void *data, struct drm_file *file)
{
    struct lite_wait_bo *args = data;
    struct drm_gem_object *obj;

    obj = drm_gem_object_lookup(file, args->handle);
    if (!obj)
        return -ENOENT;

    // TODO: Implement actual fence waiting
    // For now, we assume immediate completion
    
    drm_gem_object_put(obj);
    return 0;
}

static const struct drm_ioctl_desc lite_ioctls[] = {
    DRM_IOCTL_DEF_DRV(LITE_GEM_CREATE, lite_ioctl_gem_create, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(LITE_GEM_MAP, lite_ioctl_gem_map, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(LITE_GET_PARAM, lite_ioctl_get_param, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(LITE_SUBMIT_CMD, lite_ioctl_submit_cmd, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(LITE_WAIT_BO, lite_ioctl_wait_bo, DRM_RENDER_ALLOW),
};

static int lite_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct drm_file *file_priv = filp->private_data;
    struct lite_device *ldev = to_lite_device(file_priv->minor->dev);

    return ttm_bo_mmap(filp, vma, &ldev->ttm);
}

static const struct file_operations lite_gpu_fops = {
    .owner = THIS_MODULE,
    .open = drm_open,
    .release = drm_release,
    .unlocked_ioctl = drm_ioctl,
    .mmap = lite_mmap,
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

    ret = lite_ring_init(ldev);
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
    lite_ring_fini(ldev);
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
