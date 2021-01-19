#ifndef __RECORDER_H__
#define __RECORDER_H__

#include "codec.h"

void* ffrecorder_init (char *name, char *type, int duration, int channels, int samprate, int width, int height, int fps, void *adev, void *vdev, CODEC *aenc, CODEC *venc);
void  ffrecorder_exit (void *ctxt);
void  ffrecorder_start(void *ctxt, int start);

#endif
