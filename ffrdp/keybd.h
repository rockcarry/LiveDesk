#ifndef __FFKEYBD_H__
#define __FFKEYBD_H__

#include <stdint.h>

void* ffkeybd_init (char *dev);
void  ffkeybd_exit (void *ctx);
void  ffkeybd_event(void *ctx, uint8_t key, uint8_t scancode, uint8_t flags1, uint8_t flags2);

#endif
