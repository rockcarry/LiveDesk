#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "adev.h"
#include "vdev.h"
#include "avimuxer.h"
#include "mp4muxer.h"
#include "recorder.h"

#ifdef WIN32
#include <windows.h>
#pragma warning(disable:4996)
#define usleep(s) Sleep((s)/1000)
#define get_tick_count GetTickCount
#define snprintf _snprintf
#endif

#define RECTYPE_AVI  (('A' << 0) | ('V' << 8) | ('I' << 16))
#define RECTYPE_MP4  (('M' << 0) | ('P' << 8) | ('4' << 16))
#define AVI_ALAW_FRAME_SIZE  320

typedef struct {
    char      filename[256];
    int       duration;
    int       channels;
    int       samprate;
    int       width;
    int       height;
    int       fps;
    uint32_t  rectype;
    uint32_t  starttick;
    int       recfilecnt;

    void     *adev;
    void     *vdev;
    CODEC    *aenc;
    CODEC    *venc;

    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    #define TS_NEXT  (1 << 2)
    uint32_t  status;
    pthread_t pthread;
    uint8_t   buffer[2 * 1024 * 1024];
} RECORDER;

static void* record_thread_proc(void *argv)
{
    RECORDER *recorder = (RECORDER*)argv;
    char      filepath[256] = "";
    void     *muxer         = NULL;
    int       framesize, readsize, key;
    uint32_t  pts;

    while (1) {
        readsize = codec_read(recorder->venc, recorder->buffer, sizeof(recorder->buffer), &framesize, &key, &pts, 10);
        if ((recorder->status & TS_START) == 0 || (readsize > 0 && (recorder->status & TS_NEXT) && key)) { // if record stop or change to next record file
            if (muxer) {
                switch (recorder->rectype) {
                case RECTYPE_AVI: avimuxer_exit(muxer); break;
                case RECTYPE_MP4: mp4muxer_exit(muxer); break;
                }
                muxer = NULL;
                recorder->status &= ~TS_NEXT;
            }
        }

        while ((recorder->status & (TS_EXIT|TS_START)) == 0) usleep(100*1000); // if record stopped
        if (recorder->status & TS_EXIT) break; // if recorder exited

        if ((recorder->status & TS_START) != 0 && readsize > 0) { // if recorder started, and got video data
            if (!muxer && key) { // if muxer not created and this is video key frame
                int ish265 = !!strstr(recorder->venc->name, "h265");
                switch (recorder->rectype) {
                case RECTYPE_AVI:
                    snprintf(filepath, sizeof(filepath), "%s-%03d.avi", recorder->filename, ++recorder->recfilecnt);
                    muxer = avimuxer_init(filepath, recorder->duration, recorder->width, recorder->height, recorder->fps, recorder->fps * 1, ish265, AVI_ALAW_FRAME_SIZE);
                    break;
                case RECTYPE_MP4:
                    snprintf(filepath, sizeof(filepath), "%s-%03d.mp4", recorder->filename, ++recorder->recfilecnt);
                    muxer = mp4muxer_init(filepath, recorder->duration, recorder->width, recorder->height, recorder->fps, recorder->fps * 1, ish265, recorder->channels, recorder->samprate, 16, 1024, recorder->aenc->aacinfo);
                    break;
                }
                if (recorder->starttick == 0) {
                    recorder->starttick = get_tick_count();
                    recorder->starttick = recorder->starttick ? recorder->starttick : 1;
                }
            }

            if (muxer) { // if muxer created
                switch (recorder->rectype) {
                case RECTYPE_AVI: avimuxer_video(muxer, recorder->buffer, readsize, key, pts); break;
                case RECTYPE_MP4: mp4muxer_video(muxer, recorder->buffer, readsize, key, pts); break;
                }
            }
        }

        readsize = codec_read(recorder->aenc, recorder->buffer, recorder->rectype == RECTYPE_AVI ? AVI_ALAW_FRAME_SIZE : sizeof(recorder->buffer), &framesize, &key, &pts, 10);
        if ((recorder->status & TS_START) != 0 && readsize > 0 && muxer) { // if recorder started and muxer created and got audio frame
            switch (recorder->rectype) {
            case RECTYPE_AVI: avimuxer_audio(muxer, recorder->buffer, readsize, key, pts); break;
            case RECTYPE_MP4: mp4muxer_audio(muxer, recorder->buffer, readsize, key, pts); break;
            }
        }

        if (recorder->starttick && (int32_t)get_tick_count() - (int32_t)recorder->starttick > recorder->duration) {
            recorder->starttick += recorder->duration;
            recorder->status    |= TS_NEXT;
            codec_reset(recorder->venc, CODEC_REQUEST_IDR);
        }
    }
    return NULL;
}

void* ffrecorder_init(char *name, char *type, int duration, int channels, int samprate, int width, int height, int fps, void *adev, void *vdev, CODEC *aenc, CODEC *venc)
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

    if (stricmp(type, "mp4") == 0) recorder->rectype = RECTYPE_MP4;
    if (stricmp(type, "avi") == 0) recorder->rectype = RECTYPE_AVI;

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
    recorder->status = (recorder->status & ~TS_START) | TS_EXIT;
    pthread_join(recorder->pthread, NULL);
    free(recorder);
}

void ffrecorder_start(void *ctxt, int start)
{
    RECORDER *recorder = (RECORDER*)ctxt;
    if (!ctxt) return;
    if (start) {
        recorder->status |= TS_START;
        codec_reset(recorder->aenc, CODEC_CLEAR_INBUF|CODEC_CLEAR_OUTBUF|CODEC_REQUEST_IDR);
        codec_reset(recorder->venc, CODEC_CLEAR_INBUF|CODEC_CLEAR_OUTBUF|CODEC_REQUEST_IDR);
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

