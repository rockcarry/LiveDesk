#ifndef __RTMPPUSHER_H__
#define __RTMPPUSHER_H__

#include "codec.h"

void* rtmppusher_init (char *url, void *adev, void *vdev, CODEC *aenc, CODEC *venc);
void  rtmppusher_exit (void *ctxt);
void  rtmppusher_start(void *ctxt, int start);

#endif

