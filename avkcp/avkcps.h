#ifndef __AVKCPS_H__
#define __AVKCPS_H__

void* avkcps_init (int port, int channels, int samprate, int width, int height, int frate, void *adev, void *vdev, CODEC *aenc, CODEC *venc);
void  avkcps_exit (void *ctxt);
void  avkcps_start(void *ctxt, int start);

#endif
