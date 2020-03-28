#ifndef __STDAFX_H__
#define __STDAFX_H__

#include <windows.h>

#define usleep(t)      Sleep((t) / 1000)
#define get_tick_count GetTickCount
#define ARRAY_SIZE(a)  (sizeof(a) / sizeof(a[0]))
#define ALIGN(x, y)    ((x + y - 1) & ~(y - 1))

#endif
