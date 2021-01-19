#ifndef __AVIMUXER_H__
#define __AVIMUXER_H__

void* avimuxer_init (char *fname, int duration, int w, int h, int fps, int sampnum, int h265);
void  avimuxer_exit (void *ctxt);
void  avimuxer_video(void *ctxt, unsigned char *buf, int len, int key, unsigned pts);
void  avimuxer_audio(void *ctxt, unsigned char *buf, int len, int key, unsigned pts);

#endif



