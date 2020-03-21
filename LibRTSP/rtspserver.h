#ifndef __RTSPSERVER_H__
#define __RTSPSERVER_H__

#include <stdint.h>
#include "adev.h"
#include "venc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*PFN_CTRL)(void *ctxt, int cmd, void *buf, int size);

void* rtspserver_init(void *adev, PFN_CTRL actrl, void *vdev, PFN_CTRL vctrl, int aenc_type, int venc_type, uint8_t *aac_config);
void  rtspserver_exit(void *ctx);
int   rtspserver_running_streams(void *ctx);

#ifdef WIN32
void gettimeofday(struct timeval *tp, void *tzp);
#endif

#ifdef __cplusplus
}
#endif

#endif







