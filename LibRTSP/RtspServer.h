#ifndef __RTSPSERVER_H__
#define __RTSPSERVER_H__

#include <stdint.h>
#include "aenc.h"
#include "venc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*PFN_IOCTL)(void *ctxt, int cmd, void *buf, int bsize, int *fsize);

void* rtspserver_init(void *adev, PFN_IOCTL aioctl, void *vdev, PFN_IOCTL vioctl, int aenc_type, int venc_type, uint8_t *aac_config, int frate);
void  rtspserver_exit(void *ctx);
int   rtspserver_running_streams(void *ctx);

#ifdef __cplusplus
}
#endif

#endif







