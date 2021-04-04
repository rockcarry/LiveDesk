#include <stdint.h>
#include <windows.h>
#include <pthread.h>
#include "stdafx.h"
#include "vdev.h"
#include "log.h"

#include "libavutil/time.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"

typedef struct {
    HDC      hdcsrc;
    HDC      hdcdst;
    HBITMAP  hbitmap;
    uint8_t *bmp_buffer;
    int      bmp_stride;
    int      frame_rate;
    int      screen_width;
    int      screen_height;

    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    int       status;
    pthread_t thread;

    void    *codec;
    PFN_CODEC_CALLBACK callback;
} VDEV;

static void* vdev_capture_thread_proc(void *param)
{
    VDEV      *vdev    = (VDEV*)param;
    uint32_t   tickcur = 0, ticknext = 0;
    int32_t    period  = 1000 / vdev->frame_rate, ticksleep = 0;
    CURSORINFO curinfo = {0};
    ICONINFO   icoinfo = {0};
    HCURSOR    hcursor = NULL;

    while (!(vdev->status & TS_EXIT)) {
        if (!(vdev->status & TS_START)) {
            ticknext = 0; usleep(100*1000); continue;
        }
        tickcur   = get_tick_count();
        ticknext  =(ticknext ? ticknext : tickcur) + period;
        ticksleep = (int32_t)ticknext - (int32_t)tickcur;

        BitBlt(vdev->hdcdst, 0, 0, vdev->screen_width, vdev->screen_height, vdev->hdcsrc, 0, 0, SRCCOPY|CAPTUREBLT);
        curinfo.cbSize = sizeof(CURSORINFO);
        GetCursorInfo(&curinfo);
        GetIconInfo(curinfo.hCursor, &icoinfo);
        DrawIcon(vdev->hdcdst, curinfo.ptScreenPos.x - icoinfo.xHotspot, curinfo.ptScreenPos.y - icoinfo.xHotspot, curinfo.hCursor);

        if (vdev->callback) {
            void *data[8] = { vdev->bmp_buffer };
            int   len [8] = { vdev->bmp_stride * vdev->screen_height, vdev->screen_width, vdev->screen_height, vdev->bmp_stride };
            vdev->callback(vdev->codec, data, len);
        }
        if (ticksleep > 0) usleep(ticksleep * 1000);
    }

    return NULL;
}

void* vdev_init(int frate, int w, int h)
{
    BITMAPINFO bmpinfo = {0};
    BITMAP     bitmap;
    VDEV *vdev = calloc(1, sizeof(VDEV));
    if (!vdev) return NULL;

    vdev->hdcsrc = GetDC(NULL);
    vdev->hdcdst = CreateCompatibleDC(NULL);

    vdev->screen_width  = GetSystemMetrics(SM_CXSCREEN);
    vdev->screen_height = GetSystemMetrics(SM_CYSCREEN);
    vdev->frame_rate    = frate;

    bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
    bmpinfo.bmiHeader.biWidth       =  vdev->screen_width;
    bmpinfo.bmiHeader.biHeight      = -vdev->screen_height;
    bmpinfo.bmiHeader.biPlanes      =  1;
    bmpinfo.bmiHeader.biBitCount    =  32;
    bmpinfo.bmiHeader.biCompression =  BI_RGB;
    vdev->hbitmap = CreateDIBSection(vdev->hdcdst, &bmpinfo, DIB_RGB_COLORS, (void**)&vdev->bmp_buffer, NULL, 0);
    GetObject(vdev->hbitmap, sizeof(BITMAP), &bitmap);
    SelectObject(vdev->hdcdst, vdev->hbitmap);
    vdev->bmp_stride  = bitmap.bmWidthBytes;

    pthread_create(&vdev->thread, NULL, vdev_capture_thread_proc, vdev);
    return vdev;
}

void vdev_free(void *ctxt)
{
    VDEV *vdev = (VDEV*)ctxt;
    if (!vdev) return;

    vdev->status |= TS_EXIT;
    pthread_join(vdev->thread, NULL);

    ReleaseDC(NULL, vdev->hdcsrc);
    DeleteDC(vdev->hdcdst);
    DeleteObject(vdev->hbitmap);
    free(vdev);
}

void vdev_start(void *ctxt, int start)
{
    VDEV *vdev = (VDEV*)ctxt;
    if (!vdev) return;
    if (start) {
        vdev->status |= TS_START;
    } else {
        vdev->status &=~TS_START;
    }
}

void vdev_set_callback(void *ctxt, PFN_CODEC_CALLBACK callback, void *codec)
{
    VDEV *vdev = (VDEV*)ctxt;
    if (!vdev) return;
    vdev->codec    = codec;
    vdev->callback = callback;
}
