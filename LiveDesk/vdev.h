#ifndef __VDEV_H__
#define __VDEV_H__

void* vdev_init  (int frate, int w, int h);
void  vdev_free  (void *ctxt);
void  vdev_start (void *ctxt, int start);
void* vdev_lock  (void *ctxt, int wait);
void  vdev_unlock(void *ctxt);

#endif
