#ifndef __AIDEV_H__
#define __AIDEV_H__

void* aidev_init (void);
void  aidev_free (void *ctxt);
void  aidev_start(void *ctxt, int start);
int   aidev_read (void *ctxt, void *buf, int size, int wait);

#endif
