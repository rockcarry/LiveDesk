#ifndef __RTMPPUSHER_H__
#define __RTMPPUSHER_H__

#include "aenc.h"

void* rtmppusher_init (char *url, int aenctype, unsigned char *aaccfg, void *aenc, PFN_AVDEV_IOCTL aioctl, void *venc, PFN_AVDEV_IOCTL vioctl);
void  rtmppusher_exit (void *ctxt);
void  rtmppusher_start(void *ctxt, int start);

#endif

