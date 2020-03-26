#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "aenc.h"
#include "venc.h"
#include "mp4muxer.h"
#include "recorder.h"

#ifdef WIN32
#include <windows.h>
#pragma warning(disable:4996)
#define usleep(s) Sleep((s)/1000)
#define get_tick_count GetTickCount
#define snprintf _snprintf
#endif

typedef struct {
    char      filename[256];
    int       duration;
    int       channels;
    int       samprate;
    int       width;
    int       height;
    int       fps;
    uint32_t  starttick;
    int       recfilecnt;

    void     *aenc;
    void     *venc;

    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    uint32_t  status;
    pthread_t pthread;
    uint8_t   aaccfg[2];
} RECORDER;

static void* record_thread_proc(void *argv)
{
    RECORDER *recorder = (RECORDER*)argv;
    uint8_t   buf[512*1024]  = "";
    char      filepath[256]  = "";
    void     *mp4muxer       = NULL;
    int       framesize, readsize;

    while (!(recorder->status & TS_EXIT)) {
        if (!(recorder->status & TS_START)) {
            if (mp4muxer) {
                mp4muxer_exit(mp4muxer);
                mp4muxer = NULL;
            }
            usleep(100*1000);
            continue;
        }

        if (!mp4muxer) {
            snprintf(filepath, sizeof(filepath), "%s-%03d.mp4", recorder->filename, ++recorder->recfilecnt);
            mp4muxer = mp4muxer_init(filepath, recorder->duration, recorder->width, recorder->height, recorder->fps, recorder->fps * 2, recorder->channels, recorder->samprate, 16, 1024, recorder->aaccfg);
            recorder->starttick = get_tick_count();
        }

        readsize = aenc_ioctl(recorder->aenc, AENC_CMD_READ, buf, sizeof(buf), &framesize);
        if (readsize > 0) {
            mp4muxer_audio(mp4muxer, buf, readsize, 0);
        }

        readsize = venc_ioctl(recorder->venc, VENC_CMD_READ, buf, sizeof(buf), &framesize);
        if (readsize > 0) {
            mp4muxer_video(mp4muxer, buf, readsize, 0);
        }

        if (get_tick_count() - recorder->starttick > recorder->duration) {
            recorder->starttick += recorder->duration;
            venc_ioctl(recorder->venc, VENC_CMD_REQUEST_IDR , NULL, 0, NULL);
            mp4muxer_exit(mp4muxer);
            mp4muxer = NULL;
        }
    }

    mp4muxer_exit(mp4muxer);
    return NULL;
}

void* ffrecorder_init(char *name, int duration, int channels, int samprate, int width, int height, int fps, unsigned char *aaccfg, void *aenc, void *venc)
{
    RECORDER *recorder = calloc(1, sizeof(RECORDER));
    if (!recorder) return NULL;

    strncpy(recorder->filename, name, sizeof(recorder->filename));
    recorder->duration = duration;
    recorder->channels = channels;
    recorder->samprate = samprate;
    recorder->width    = width;
    recorder->height   = height;
    recorder->fps      = fps;
    recorder->aaccfg[0]= aaccfg ? aaccfg[0] : 0;
    recorder->aaccfg[1]= aaccfg ? aaccfg[1] : 0;
    recorder->aenc     = aenc;
    recorder->venc     = venc;

    // create server thread
    pthread_create(&recorder->pthread, NULL, record_thread_proc, recorder);
    return recorder;
}

void ffrecorder_exit(void *ctxt)
{
    RECORDER *recorder = (RECORDER*)ctxt;
    if (!ctxt) return;

    aenc_ioctl(recorder->aenc, AENC_CMD_STOP, NULL, 0, NULL);
    venc_ioctl(recorder->venc, AENC_CMD_STOP, NULL, 0, NULL);
    recorder->status |= TS_EXIT;
    pthread_join(recorder->pthread, NULL);
    free(recorder);
}

void ffrecorder_start(void *ctxt, int start)
{
    RECORDER *recorder = (RECORDER*)ctxt;
    if (!ctxt) return;
    if (start) {
        recorder->status |= TS_START;
        aenc_ioctl(recorder->aenc, AENC_CMD_RESET_BUFFER, NULL, 0, NULL);
        venc_ioctl(recorder->venc, VENC_CMD_RESET_BUFFER, NULL, 0, NULL);
        venc_ioctl(recorder->venc, VENC_CMD_REQUEST_IDR , NULL, 0, NULL);
        aenc_ioctl(recorder->aenc, AENC_CMD_START, NULL, 0, NULL);
        venc_ioctl(recorder->venc, VENC_CMD_START, NULL, 0, NULL);
    } else {
        recorder->status &=~TS_START;
        aenc_ioctl(recorder->aenc, AENC_CMD_STOP, NULL, 0, NULL);
        venc_ioctl(recorder->venc, VENC_CMD_STOP, NULL, 0, NULL);
    }
}

