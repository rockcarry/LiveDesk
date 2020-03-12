#ifndef __VIENC_H__
#define __VIENC_H__

void* vienc_init (void *videv, int frate, int w, int h, int bitrate);
void  vienc_free (void *ctxt);
void  vienc_start(void *ctxt, int start);

// when buf is NULL, size is used for command
enum {
    VIENC_CMD_STOP = 0,
    VIENC_CMD_RESET_BUFFER,
    VIENC_CMD_REQUEST_IDR,
};
int vienc_read(void *ctxt, void *buf, int size, int wait);

#endif


