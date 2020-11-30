#ifndef __FFRDP_H__
#define __FFRDP_H__

void* ffrdps_init (int port, int channels, int samprate, int width, int height, int frate, void *adev, void *vdev, CODEC *aenc, CODEC *venc);
void  ffrdps_exit (void *ctxt);
void  ffrdps_start(void *ctxt, int start);
void  ffrdps_dump (void *ctxt, int clearhistory);
void  ffrdps_reconfig_bitrate(void *ctxt, int bitrate);
void  ffrdps_adaptive_bitrate_setup (void *ctxt, int *blist, int n);
void  ffrdps_adaptive_bitrate_enable(void *ctxt, int en);

#endif

