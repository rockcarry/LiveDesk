#ifndef __VIDEV_H__
#define __VIDEV_H__

void* videv_init  (int frate, int w, int h);
void  videv_free  (void *ctxt);
void  videv_start (void *ctxt, int start);
void* videv_lock  (void *ctxt, int wait);
void  videv_unlock(void *ctxt);

#endif
