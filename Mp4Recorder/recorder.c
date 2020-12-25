#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "adev.h"
#include "vdev.h"
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

    void     *adev;
    void     *vdev;
    CODEC    *aenc;
    CODEC    *venc;

    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    uint32_t  status;
    pthread_t pthread;
    uint8_t   buffer[2 * 1024 * 1024];
} RECORDER;

static void* record_thread_proc(void *argv)
{
    RECORDER *recorder = (RECORDER*)argv;
    char      filepath[256] = "";
    void     *mp4muxer      = NULL;
    int       framesize, readsize;
    uint32_t  pts;

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
            mp4muxer = mp4muxer_init(filepath, recorder->duration, recorder->width, recorder->height, recorder->fps, recorder->fps * 2, recorder->channels, recorder->samprate, 16, 1024, recorder->aenc->info);
            recorder->starttick = get_tick_count();
        }

        readsize = codec_read(recorder->aenc, recorder->buffer, sizeof(recorder->buffer), &framesize, NULL, &pts, 16);
        if (readsize > 0) {
            mp4muxer_audio(mp4muxer, recorder->buffer, readsize, pts);
        }

        readsize = codec_read(recorder->venc, recorder->buffer, sizeof(recorder->buffer), &framesize, NULL, &pts, 16);
        if (readsize > 0) {
            mp4muxer_video(mp4muxer, recorder->buffer, readsize, pts);
        }

        if ((int32_t)get_tick_count() - (int32_t)recorder->starttick > recorder->duration) {
            recorder->starttick += recorder->duration;
            codec_reset(recorder->venc, CODEC_RESET_REQUEST_IDR);
            mp4muxer_exit(mp4muxer);
            mp4muxer = NULL;
        }
    }

    mp4muxer_exit(mp4muxer);
    return NULL;
}

void* ffrecorder_init(char *name, int duration, int channels, int samprate, int width, int height, int fps, void *adev, void *vdev, CODEC *aenc, CODEC *venc)
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
    recorder->adev     = adev;
    recorder->vdev     = vdev;
    recorder->aenc     = aenc;
    recorder->venc     = venc;

    // create server thread
    pthread_create(&recorder->pthread, NULL, record_thread_proc, recorder);
    ffrecorder_start(recorder, 1);
    return recorder;
}

void ffrecorder_exit(void *ctxt)
{
    RECORDER *recorder = (RECORDER*)ctxt;
    if (!ctxt) return;

    adev_start (recorder->adev, 0);
    vdev_start (recorder->vdev, 0);
    codec_start(recorder->aenc, 0);
    codec_start(recorder->venc, 0);
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
        codec_reset(recorder->aenc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
        codec_reset(recorder->venc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
        codec_start(recorder->aenc, 1);
        codec_start(recorder->venc, 1);
        adev_start (recorder->adev, 1);
        vdev_start (recorder->vdev, 1);
    } else {
        recorder->status &=~TS_START;
        codec_start(recorder->aenc, 0);
        codec_start(recorder->venc, 0);
        adev_start (recorder->adev, 0);
        vdev_start (recorder->vdev, 0);
    }
}

