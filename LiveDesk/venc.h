#ifndef __VENC_H__
#define __VENC_H__

//#define H26X_FRAMED_READ

void* venc_init(void *vdev, int frate, int w, int h, int bitrate);
void  venc_free(void *ctxt);

enum {
    VENC_CMD_READ = 0,
    VENC_CMD_START,
    VENC_CMD_STOP,
    VENC_CMD_RESET_BUFFER,
    VENC_CMD_REQUEST_IDR = 0x1000,
};
int venc_ioctl(void *ctxt, int cmd, void *buf, int bsize, int *fsize);

#endif


