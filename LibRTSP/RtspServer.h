#ifndef __RTSPSERVER_H__
#define __RTSPSERVER_H__

#include <stdint.h>
#include "adev.h"
#include "vdev.h"
#include "codec.h"

#ifdef __cplusplus
extern "C" {
#endif

void* rtspserver_init(char *name, void *adev, void *vdev, CODEC *aenc, CODEC *venc, int frate);
void  rtspserver_exit(void *ctx);
int   rtspserver_running_streams(void *ctx);

#ifdef __cplusplus
}
#endif

#endif







