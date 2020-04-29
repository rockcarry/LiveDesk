#ifndef __ADEV_H__
#define __ADEV_H__

#include "codec.h"

#ifdef __cplusplus
extern "C" {
#endif

void* adev_init (int channels, int samplerate);
void  adev_free (void *ctxt);
void  adev_start(void *ctxt, int start);
void  adev_set_callback(void *ctxt, PFN_CODEC_CALLBACK callback, void *codec);

#ifdef __cplusplus
}
#endif

#endif
