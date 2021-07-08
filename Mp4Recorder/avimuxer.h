#ifndef __AVIMUXER_H__
#define __AVIMUXER_H__

void* avimuxer_init (char *file, int duration, int w, int h, int fps, int sampnum, int h265);
void  avimuxer_exit (void *ctx);
void  avimuxer_video(void *ctx, unsigned char *buf, int len, int key, unsigned pts);
void  avimuxer_audio(void *ctx, unsigned char *buf, int len, int key, unsigned pts);

#endif



