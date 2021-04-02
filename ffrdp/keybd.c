#include <windows.h>
#include "keybd.h"

void* ffkeybd_init(char *dev) { return (void*)1; }
void  ffkeybd_exit(void *ctx) {}

void ffkeybd_event(void *ctx, uint8_t key, uint8_t scancode, uint8_t flags1, uint8_t flags2)
{
    DWORD dwFlags = ((flags1 & (1 << 0)) ? KEYEVENTF_EXTENDEDKEY : 0)
                  | ((flags1 & (1 << 7)) ? KEYEVENTF_KEYUP       : 0);
    keybd_event(key, MapVirtualKey(key, 0), dwFlags, 0);
}
