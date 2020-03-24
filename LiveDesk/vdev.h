#ifndef __VDEV_H__
#define __VDEV_H__

void* vdev_init(int frate, int w, int h);
void  vdev_free(void *ctxt);

enum {
    VDEV_CMD_READ = 0,
    VDEV_CMD_START,
    VDEV_CMD_STOP,
    VDEV_CMD_RESET_BUFFER,
    VDEV_CMD_LOCK = 0x1000,
    VDEV_CMD_UNLOCK,
};
int vdev_ioctl(void *ctxt, int cmd, void *buf, int size);

#endif
