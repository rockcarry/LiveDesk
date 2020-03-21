#include <stdint.h>
#include <windows.h>
#include <pthread.h>
#include "stdafx.h"
#include "vdev.h"
#include "log.h"

#include "libavutil/time.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"

#define VDEV_BUFFER_NUM  3

typedef struct {
    HDC      hdcsrc;
    HDC      hdcdst;
    HBITMAP  hbitmap;
    uint8_t *bmp_buffer;
    int      bmp_stride;
    int      frame_rate;
    int      screen_width;
    int      screen_height;
    int      yuv_width;
    int      yuv_height;
    uint8_t *yuv_buffer;
    int      yuv_head;
    int      yuv_tail;
    int      yuv_size;

    struct SwsContext *sws_context;

    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    int      status;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    pthread_t       thread;
} VDEV;

static void* vdev_capture_thread_proc(void *param)
{
    VDEV *vdev = (VDEV*)param;
    uint32_t   tickcur = 0, ticknext = 0;
    int32_t    period  = 1000 / vdev->frame_rate, ticksleep = 0;
    AVFrame    picsrc  = {0}, picdst = {0};
    CURSORINFO curinfo = {0};
    ICONINFO   icoinfo = {0};
    HCURSOR    hcursor = NULL;

    picsrc.data[0]     = vdev->bmp_buffer;
    picsrc.linesize[0] = vdev->bmp_stride;
    picdst.linesize[0] = vdev->yuv_width;
    picdst.linesize[1] = vdev->yuv_width / 2;
    picdst.linesize[2] = vdev->yuv_width / 2;

    while (!(vdev->status & TS_EXIT)) {
        if (!(vdev->status & TS_START)) {
            ticknext = 0; usleep(100*1000); continue;
        }
        tickcur   = get_tick_count();
        ticknext  =(ticknext ? ticknext : tickcur) + period;
        ticksleep = ticknext - tickcur;

        BitBlt(vdev->hdcdst, 0, 0, vdev->screen_width, vdev->screen_height, vdev->hdcsrc, 0, 0, SRCCOPY);
        curinfo.cbSize = sizeof(CURSORINFO);
        GetCursorInfo(&curinfo);
        GetIconInfo(curinfo.hCursor, &icoinfo);
        DrawIcon(vdev->hdcdst, curinfo.ptScreenPos.x - icoinfo.xHotspot, curinfo.ptScreenPos.y - icoinfo.xHotspot, curinfo.hCursor);

        pthread_mutex_lock(&vdev->mutex);
        picdst.data[0] = vdev->yuv_buffer + vdev->yuv_tail * vdev->yuv_width * vdev->yuv_height * 3 / 2;
        picdst.data[1] = picdst.data[0] + vdev->yuv_width * vdev->yuv_height * 4 / 4;
        picdst.data[2] = picdst.data[0] + vdev->yuv_width * vdev->yuv_height * 5 / 4;
        sws_scale(vdev->sws_context, (const uint8_t * const*)picsrc.data, picsrc.linesize, 0, vdev->screen_height, picdst.data, picdst.linesize);
        if (++vdev->yuv_tail == VDEV_BUFFER_NUM) vdev->yuv_tail = 0;
        if (vdev->yuv_size < VDEV_BUFFER_NUM) {
            vdev->yuv_size++;
        } else {
            vdev->yuv_head = vdev->yuv_tail;
        }
        pthread_cond_signal(&vdev->cond);
        pthread_mutex_unlock(&vdev->mutex);
        if (ticksleep > 0) usleep(ticksleep * 1000);
    }

    return NULL;
}

void* vdev_init(int frate, int w, int h)
{
    BITMAPINFO bmpinfo = {0};
    BITMAP     bitmap;
    VDEV *vdev = calloc(1, sizeof(VDEV) + (w * h * 3 / 2) * VDEV_BUFFER_NUM);
    if (!vdev) return NULL;

    // init mutex & cond
    pthread_mutex_init(&vdev->mutex, NULL);
    pthread_cond_init (&vdev->cond , NULL);

    vdev->hdcsrc = GetDC(GetDesktopWindow());
    vdev->hdcdst = CreateCompatibleDC(vdev->hdcsrc);

    vdev->screen_width  = GetSystemMetrics(SM_CXSCREEN);
    vdev->screen_height = GetSystemMetrics(SM_CYSCREEN);
    vdev->frame_rate    = frate;
    vdev->yuv_width     = w;
    vdev->yuv_height    = h;
    vdev->yuv_buffer    = (BYTE*)vdev + sizeof(VDEV);

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
    vdev->sws_context = sws_getContext(vdev->screen_width, vdev->screen_height, AV_PIX_FMT_BGRA,
        vdev->yuv_width, vdev->yuv_height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

    pthread_create(&vdev->thread, NULL, vdev_capture_thread_proc, vdev);
    return vdev;
}

void vdev_free(void *ctxt)
{
    VDEV *vdev = (VDEV*)ctxt;
    if (!vdev) return;

    vdev->status |= TS_EXIT;
    pthread_join(vdev->thread, NULL);

    if (vdev->sws_context) {
        sws_freeContext(vdev->sws_context);
    }

    ReleaseDC(GetDesktopWindow(), vdev->hdcsrc);
    DeleteDC(vdev->hdcdst);
    DeleteObject(vdev->hbitmap);

    pthread_mutex_destroy(&vdev->mutex);
    pthread_cond_destroy (&vdev->cond );
    free(vdev);
}

void vdev_start(void *ctxt, int start)
{
    VDEV *vdev = (VDEV*)ctxt;
    if (!vdev) return;
    if (start) vdev->status |= TS_START;
    else {
        pthread_mutex_lock(&vdev->mutex);
        vdev->status &= ~TS_START;
        pthread_cond_signal(&vdev->cond);
        pthread_mutex_unlock(&vdev->mutex);
    }
}

void* vdev_lock(void *ctxt, int wait)
{
    VDEV   *vdev = (VDEV*)ctxt;
    uint8_t *buf = NULL;
    if (!ctxt) return NULL;
    pthread_mutex_lock(&vdev->mutex);
    if (wait) {
        while (vdev->yuv_size <= 0 && (vdev->status & TS_START)) pthread_cond_wait(&vdev->cond, &vdev->mutex);
    }
    if (vdev->yuv_size > 0) {
        buf = vdev->yuv_buffer + vdev->yuv_head * vdev->yuv_width * vdev->yuv_height * 3 / 2;
        vdev->yuv_size--;
        if (++vdev->yuv_head == VDEV_BUFFER_NUM) vdev->yuv_head = 0;
    }
    return buf;
}

void vdev_unlock(void *ctxt)
{
    VDEV *vdev = (VDEV*)ctxt;
    if (!ctxt) return;
    pthread_mutex_unlock(&vdev->mutex);
}
