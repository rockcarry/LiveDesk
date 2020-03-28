#ifndef __AENC_H__
#define __AENC_H__

typedef int (*PFN_AVDEV_IOCTL)(void *ctxt, int cmd, void *buf, int bsize, int *fsize);

void* aenc_init(void *adev, int channels, int samplerate, int bitrate, char **aacinfo);
void  aenc_free(void *ctxt);

enum {
    AENC_CMD_READ = 0,
    AENC_CMD_START,
    AENC_CMD_STOP,
    AENC_CMD_RESET_BUFFER,
};
int aenc_ioctl(void *ctxt, int cmd, void *buf, int bsize, int *fsize);

#endif


