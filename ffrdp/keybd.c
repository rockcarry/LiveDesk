#include <windows.h>
#include "keybd.h"

void* ffkeybd_init(char *dev) { return (void*)1; }
void  ffkeybd_exit(void *ctx) {}

void ffkeybd_event(void *ctx, uint8_t key, uint8_t scancode, uint8_t flags1, uint8_t flags2)
{
    keybd_event(key, scancode, (flags1 & (1 << 7)) ? 0 : KEYEVENTF_KEYUP, 0);
}
