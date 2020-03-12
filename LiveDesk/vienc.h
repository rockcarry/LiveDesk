#ifndef __VIENC_H__
#define __VIENC_H__

void* vienc_init (void *videv, int frate, int w, int h, int bitrate);
void  vienc_free (void *ctxt);
void  vienc_start(void *ctxt, int start);
int   vienc_read (void *ctxt, void *buf, int size, int wait);

#endif


