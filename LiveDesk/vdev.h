#ifndef __VDEV_H__
#define __VDEV_H__

#include "codec.h"

#ifdef __cplusplus
extern "C" {
#endif

void* vdev_init (int frate, int w, int h);
void  vdev_free (void *ctxt);
void  vdev_start(void *ctxt, int start);
void  vdev_set_callback(void *ctxt, PFN_CODEC_CALLBACK callback, void *codec);

#ifdef __cplusplus
}
#endif

#endif
