#ifndef __VIENC_H__
#define __VIENC_H__

void* vienc_init (void *videv, int frate, int w, int h, int bitrate);
void  vienc_free (void *ctxt);

enum {
    VIENC_CMD_READ = 0,
    VIENC_CMD_START,
    VIENC_CMD_STOP,
    VIENC_CMD_RESET_BUFFER,
    VIENC_CMD_REQUEST_IDR,
};
int vienc_ctrl(void *ctxt, int cmd, void *buf, int size);

#endif


