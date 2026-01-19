#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <drm/drm.h>
#include "lite_uapi.h"

int main() {
    int fd;
    struct lite_gem_create create_args = {0};
    struct lite_gem_map map_args = {0};
    void *ptr;

    fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd < 0) {
        // Try card0 if renderD128 fails (though render node is preferred)
        fd = open("/dev/dri/card0", O_RDWR);
        if (fd < 0) {
            perror("Failed to open GPU device");
            return 1;
        }
    }
    printf("Opened GPU device\n");

    /* 1. Allocate Memory */
    create_args.size = 4096;
    create_args.flags = 0;
    if (ioctl(fd, DRM_IOCTL_LITE_GEM_CREATE, &create_args)) {
        perror("GEM Create failed");
        close(fd);
        return 1;
    }
    printf("Created GEM Object. Handle: %u, Size: %llu\n", create_args.handle, create_args.size);

    /* 2. Map Memory */
    map_args.handle = create_args.handle;
    if (ioctl(fd, DRM_IOCTL_LITE_GEM_MAP, &map_args)) {
        perror("GEM Map failed");
        close(fd);
        return 1;
    }
    printf("GEM Mapped. Offset: 0x%llx\n", map_args.offset);

    /* 3. MMAP */
    ptr = mmap(NULL, create_args.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_args.offset);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }
    printf("mmap successful. Ptr: %p\n", ptr);

    /* Write validation */
    *(int*)ptr = 0xDEADBEEF;
    printf("Wrote to memory: 0x%x\n", *(int*)ptr);

    munmap(ptr, create_args.size);
    close(fd);
    return 0;
}
