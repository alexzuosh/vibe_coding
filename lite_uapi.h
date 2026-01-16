#ifndef _LITE_UAPI_H_
#define _LITE_UAPI_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define LITE_IOCTL_BASE 'L'

/* Command definitions */
#define DRM_LITE_GET_PARAM      0x00
#define DRM_LITE_GEM_CREATE     0x01
#define DRM_LITE_GEM_MAP        0x02
#define DRM_LITE_SUBMIT_CMD     0x03
#define DRM_LITE_WAIT_BO        0x04

/* Data structures */

/**
 * struct lite_get_param - Query device parameters
 * @param: Parameter to query (e.g., GPU_FREQ, MEM_SIZE)
 * @value: Returned value
 */
struct lite_get_param {
    __u64 param;
    __u64 value;
};

/**
 * struct lite_gem_create - Create a GEM buffer object
 * @size: Size of the buffer in bytes
 * @handle: Returned GEM handle
 * @flags: Creation flags (e.g. CACHED, UNCACHED)
 */
struct lite_gem_create {
    __u64 size;
    __u32 handle;
    __u32 flags;
};

/**
 * struct lite_gem_map - Map a GEM object to user space
 * @handle: GEM handle to map
 * @offset: Returned fake mmap offset
 */
struct lite_gem_map {
    __u32 handle;
    __u32 pad;
    __u64 offset;
};

/**
 * struct lite_submit_cmd - Submit commands to the GPU
 * @cmd_buffer_ptr: Pointer to user-space command buffer
 * @cmd_buffer_size: Size of the command buffer
 * @engine_id: Which engine/EU queue to submit to
 * @fence_out: Returned sync object/fence
 */
struct lite_submit_cmd {
    __u64 cmd_buffer_ptr;
    __u32 cmd_buffer_size;
    __u32 engine_id;
    __u64 fence_out;
};

/* IOCTL Definitions */
#define DRM_IOCTL_LITE_GET_PARAM    _IOWR(LITE_IOCTL_BASE, DRM_LITE_GET_PARAM, struct lite_get_param)
#define DRM_IOCTL_LITE_GEM_CREATE   _IOWR(LITE_IOCTL_BASE, DRM_LITE_GEM_CREATE, struct lite_gem_create)
#define DRM_IOCTL_LITE_GEM_MAP      _IOWR(LITE_IOCTL_BASE, DRM_LITE_GEM_MAP, struct lite_gem_map)
#define DRM_IOCTL_LITE_SUBMIT_CMD   _IOWR(LITE_IOCTL_BASE, DRM_LITE_SUBMIT_CMD, struct lite_submit_cmd)

#endif /* _LITE_UAPI_H_ */
