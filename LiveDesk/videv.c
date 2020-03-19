#include <stdint.h>
#include <windows.h>
#include <pthread.h>
#include "stdafx.h"
#include "videv.h"
#include "log.h"

#include "libavutil/time.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"

#define VIDEV_BUFFER_NUM  3

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
} VIDEV;

static void* videv_capture_thread_proc(void *param)
{
    VIDEV *videv = (VIDEV*)param;
    uint32_t   tickcur = 0, ticknext = 0;
    int32_t    period  = 1000 / videv->frame_rate, ticksleep = 0;
    AVFrame    picsrc  = {0}, picdst = {0};
    CURSORINFO curinfo = {0};
    ICONINFO   icoinfo = {0};
    HCURSOR    hcursor = NULL;

    picsrc.data[0]     = videv->bmp_buffer;
    picsrc.linesize[0] = videv->bmp_stride;
    picdst.linesize[0] = videv->yuv_width;
    picdst.linesize[1] = videv->yuv_width / 2;
    picdst.linesize[2] = videv->yuv_width / 2;

    while (!(videv->status & TS_EXIT)) {
        if (!(videv->status & TS_START)) {
            ticknext = 0; usleep(100*1000); continue;
        }
        tickcur   = get_tick_count();
        ticknext  =(ticknext ? ticknext : tickcur) + period;
        ticksleep = ticknext - tickcur;

        BitBlt(videv->hdcdst, 0, 0, videv->screen_width, videv->screen_height, videv->hdcsrc, 0, 0, SRCCOPY);
        curinfo.cbSize = sizeof(CURSORINFO);
        GetCursorInfo(&curinfo);
        GetIconInfo(curinfo.hCursor, &icoinfo);
        DrawIcon(videv->hdcdst, curinfo.ptScreenPos.x - icoinfo.xHotspot, curinfo.ptScreenPos.y - icoinfo.xHotspot, curinfo.hCursor);

        pthread_mutex_lock(&videv->mutex);
        picdst.data[0] = videv->yuv_buffer + videv->yuv_tail * videv->yuv_width * videv->yuv_height * 3 / 2;
        picdst.data[1] = picdst.data[0] + videv->yuv_width * videv->yuv_height * 4 / 4;
        picdst.data[2] = picdst.data[0] + videv->yuv_width * videv->yuv_height * 5 / 4;
        sws_scale(videv->sws_context, (const uint8_t * const*)picsrc.data, picsrc.linesize, 0, videv->screen_height, picdst.data, picdst.linesize);
        if (++videv->yuv_tail == VIDEV_BUFFER_NUM) videv->yuv_tail = 0;
        if (videv->yuv_size < VIDEV_BUFFER_NUM) {
            videv->yuv_size++;
        } else {
            videv->yuv_head = videv->yuv_tail;
        }
        pthread_cond_signal(&videv->cond);
        pthread_mutex_unlock(&videv->mutex);
        if (ticksleep > 0) usleep(ticksleep * 1000);
    }

    return NULL;
}

void* videv_init(int frate, int w, int h)
{
    BITMAPINFO bmpinfo = {0};
    BITMAP     bitmap;
    VIDEV *videv = calloc(1, sizeof(VIDEV) + (w * h * 3 / 2) * VIDEV_BUFFER_NUM);
    if (!videv) return NULL;

    // init mutex & cond
    pthread_mutex_init(&videv->mutex, NULL);
    pthread_cond_init (&videv->cond , NULL);

    videv->hdcsrc = GetDC(GetDesktopWindow());
    videv->hdcdst = CreateCompatibleDC(videv->hdcsrc);

    videv->screen_width  = GetSystemMetrics(SM_CXSCREEN);
    videv->screen_height = GetSystemMetrics(SM_CYSCREEN);
    videv->frame_rate    = frate;
    videv->yuv_width     = w;
    videv->yuv_height    = h;
    videv->yuv_buffer    = (BYTE*)videv + sizeof(VIDEV);

    bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
    bmpinfo.bmiHeader.biWidth       =  videv->screen_width;
    bmpinfo.bmiHeader.biHeight      = -videv->screen_height;
    bmpinfo.bmiHeader.biPlanes      =  1;
    bmpinfo.bmiHeader.biBitCount    =  32;
    bmpinfo.bmiHeader.biCompression =  BI_RGB;
    videv->hbitmap = CreateDIBSection(videv->hdcdst, &bmpinfo, DIB_RGB_COLORS, (void**)&videv->bmp_buffer, NULL, 0);
    GetObject(videv->hbitmap, sizeof(BITMAP), &bitmap);
    SelectObject(videv->hdcdst, videv->hbitmap);
    videv->bmp_stride  = bitmap.bmWidthBytes;
    videv->sws_context = sws_getContext(videv->screen_width, videv->screen_height, AV_PIX_FMT_BGRA,
        videv->yuv_width, videv->yuv_height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

    pthread_create(&videv->thread, NULL, videv_capture_thread_proc, videv);
    return videv;
}

void videv_free(void *ctxt)
{
    VIDEV *videv = (VIDEV*)ctxt;
    if (!videv) return;

    videv->status |= TS_EXIT;
    pthread_join(videv->thread, NULL);

    if (videv->sws_context) {
        sws_freeContext(videv->sws_context);
    }

    ReleaseDC(GetDesktopWindow(), videv->hdcsrc);
    DeleteDC(videv->hdcdst);
    DeleteObject(videv->hbitmap);

    pthread_mutex_destroy(&videv->mutex);
    pthread_cond_destroy (&videv->cond );
    free(videv);
}

void videv_start(void *ctxt, int start)
{
    VIDEV *videv = (VIDEV*)ctxt;
    if (!videv) return;
    if (start) videv->status |= TS_START;
    else {
        pthread_mutex_lock(&videv->mutex);
        videv->status &= ~TS_START;
        pthread_cond_signal(&videv->cond);
        pthread_mutex_unlock(&videv->mutex);
    }
}

void* videv_lock(void *ctxt, int wait)
{
    VIDEV *videv = (VIDEV*)ctxt;
    uint8_t *buf = NULL;
    if (!ctxt) return NULL;
    pthread_mutex_lock(&videv->mutex);
    if (wait) {
        while (videv->yuv_size <= 0 && (videv->status & TS_START)) pthread_cond_wait(&videv->cond, &videv->mutex);
    }
    if (videv->yuv_size > 0) {
        buf = videv->yuv_buffer + videv->yuv_head * videv->yuv_width * videv->yuv_height * 3 / 2;
        videv->yuv_size--;
        if (++videv->yuv_head == VIDEV_BUFFER_NUM) videv->yuv_head = 0;
    }
    return buf;
}

void videv_unlock(void *ctxt)
{
    VIDEV *videv = (VIDEV*)ctxt;
    if (!ctxt) return;
    pthread_mutex_unlock(&videv->mutex);
}
