#ifndef __RECORDER_H__
#define __RECORDER_H__

void* ffrecorder_init (char *name, int duration, int channels, int samprate, int width, int height, int fps, unsigned char *aaccfg, void *aenc, void *venc);
void  ffrecorder_exit (void *ctxt);
void  ffrecorder_start(void *ctxt, int start);

#endif
