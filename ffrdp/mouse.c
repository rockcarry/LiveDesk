#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mouse.h"

typedef struct {
    int btns;
} MOUSE;

void* ffmouse_init(char *dev)
{
    MOUSE *mouse = calloc(1, sizeof(MOUSE));
    if (!mouse) return NULL;
    return mouse;
}

void ffmouse_exit(void *ctx)
{
    if (ctx) free(ctx);
}

void ffmouse_event(void *ctx, int dx, int dy, int btns, int wheel)
{
    MOUSE *mouse = (MOUSE*)ctx;
    DWORD  flags = 0;
    if (dx || dy) flags |= MOUSEEVENTF_MOVE ;
    if (wheel   ) flags |= MOUSEEVENTF_WHEEL;
    if ((btns ^ mouse->btns) & (1 << 0)) {
        if (btns & (1 << 0)) flags |= MOUSEEVENTF_LEFTDOWN;
        else                 flags |= MOUSEEVENTF_LEFTUP;
    }
    if ((btns ^ mouse->btns) & (1 << 1)) {
        if (btns & (1 << 1)) flags |= MOUSEEVENTF_RIGHTDOWN;
        else                 flags |= MOUSEEVENTF_RIGHTUP;
    }
    if ((btns ^ mouse->btns) & (1 << 2)) {
        if (btns & (1 << 2)) flags |= MOUSEEVENTF_MIDDLEDOWN;
        else                 flags |= MOUSEEVENTF_MIDDLEUP;
    }
    mouse->btns = btns;
    mouse_event(flags, dx, dy, (DWORD)(wheel * 120), (ULONG_PTR)NULL);
}
