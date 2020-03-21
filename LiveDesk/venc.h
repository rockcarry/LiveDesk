#ifndef __VENC_H__
#define __VENC_H__

void* venc_init(void *vdev, int frate, int w, int h, int bitrate);
void  venc_free(void *ctxt);

enum {
    VENC_CMD_READ = 0,
    VENC_CMD_START,
    VENC_CMD_STOP,
    VENC_CMD_RESET_BUFFER,
    VENC_CMD_REQUEST_IDR,
};
int venc_ctrl(void *ctxt, int cmd, void *buf, int size);

#endif


