#ifndef __RTSPSERVER_H__
#define __RTSPSERVER_H__

#include <stdint.h>
#include "aenc.h"
#include "venc.h"

#ifdef __cplusplus
extern "C" {
#endif

void* rtspserver_init(char *name, void *adev, PFN_AVDEV_IOCTL aioctl, void *vdev, PFN_AVDEV_IOCTL vioctl, int aenc_type, int venc_type, uint8_t *aac_config, int frate);
void  rtspserver_exit(void *ctx);
int   rtspserver_running_streams(void *ctx);

#ifdef __cplusplus
}
#endif

#endif







