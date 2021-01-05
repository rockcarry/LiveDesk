#ifndef __FFMOUSE_H__
#define __FFMOUSE_H__

void* ffmouse_init (char *dev);
void  ffmouse_exit (void *ctx);
void  ffmouse_event(void *ctx, int dx, int dy, int btns, int wheel);

#endif
