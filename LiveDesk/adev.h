#ifndef __ADEV_H__
#define __ADEV_H__

void* adev_init(int channels, int samplerate, int isalaw, int bufsize);
void  adev_free(void *ctxt);

enum {
    ADEV_CMD_READ = 0,
    ADEV_CMD_START,
    ADEV_CMD_STOP,
    ADEV_CMD_RESET_BUFFER,
    ADEV_LOCK_BUFFER,
    ADEV_UNLOCK_BUFFER,
};
int adev_ctrl(void *ctxt, int cmd, void *buf, int size);

#endif
