#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "librtmp.h"
#include "rtmppusher.h"
#include "adev.h"
#include "vdev.h"

#ifdef WIN32
#include <winsock.h>
#define usleep(t)  Sleep((t) / 1000)
#endif

typedef struct {
    void     *rtmp;
    void     *adev;
    void     *vdev;
    CODEC    *aenc;
    CODEC    *venc;

    #define TS_EXIT           (1 << 0)
    #define TS_START          (1 << 1)
    #define TS_UPSTREAM_START (1 << 2)
    uint32_t  status;
    pthread_t pthread;
    uint8_t   buffer[2 * 1024 * 1024];
} RTMPPUSHER;

static void rtmppush_start_upstream(RTMPPUSHER *pusher, int start)
{
    if (start) {
        if ((pusher->status & TS_UPSTREAM_START) == 0) {
            pusher->status |= TS_UPSTREAM_START;
            codec_reset(pusher->aenc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
            codec_reset(pusher->venc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
            codec_start(pusher->aenc, 1);
            codec_start(pusher->venc, 1);
            adev_start (pusher->adev, 1);
            vdev_start (pusher->vdev, 1);
        }
    } else {
        if ((pusher->status & TS_UPSTREAM_START) != 0) {
            pusher->status |= TS_UPSTREAM_START;
            codec_start(pusher->aenc, 0);
            codec_start(pusher->venc, 0);
            adev_start (pusher->adev, 0);
            vdev_start (pusher->vdev, 0);
        }
    }
}

static void* rtmppush_thread_proc(void *argv)
{
    RTMPPUSHER *pusher = (RTMPPUSHER*)argv;
    int aenctype = strcmp(pusher->aenc->name, "aacenc") == 0, framesize, readsize, connected;

    while (!(pusher->status & TS_EXIT)) {
        if (!(pusher->status & TS_START)) {
            rtmppush_start_upstream(pusher, 0);
            usleep(100*1000); continue;
        }

        connected = rtmp_push_conn(pusher->rtmp) == 0;
        rtmppush_start_upstream(pusher, connected);
        if (!connected) { usleep(100*1000); continue; }

        readsize = codec_read(pusher->aenc, pusher->buffer, sizeof(pusher->buffer), &framesize, NULL, 16);
        if (readsize > 0) {
            if (aenctype) rtmp_push_aac (pusher->rtmp, pusher->buffer, readsize);
            else          rtmp_push_alaw(pusher->rtmp, pusher->buffer, readsize);
        }

        readsize = codec_read(pusher->venc, pusher->buffer, sizeof(pusher->buffer), &framesize, NULL, 16);
        if (readsize > 0) rtmp_push_h264(pusher->rtmp, pusher->buffer, readsize);
    }
    return NULL;
}

void* rtmppusher_init(char *url, void *adev, void *vdev, CODEC *aenc, CODEC *venc)
{
    RTMPPUSHER *pusher = calloc(1, sizeof(RTMPPUSHER));
    if (!pusher) return NULL;

#ifdef WIN32
    if (1) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            exit(1);
        }
    }
#endif

    pusher->rtmp = rtmp_push_init(url, aenc->info);
    pusher->adev = adev;
    pusher->vdev = vdev;
    pusher->aenc = aenc;
    pusher->venc = venc;

    pthread_create(&pusher->pthread, NULL, rtmppush_thread_proc, pusher);
    rtmppusher_start(pusher, 1);
    return pusher;
}

void rtmppusher_exit(void *ctxt)
{
    RTMPPUSHER *pusher = (RTMPPUSHER*)ctxt;
    if (!pusher) return;

    rtmppusher_start(pusher, 0);
    pusher->status |= TS_EXIT;
    pthread_join(pusher->pthread, NULL);

#ifdef WIN32
    WSACleanup();
#endif
}

void rtmppusher_start(void *ctxt, int start)
{
    RTMPPUSHER *pusher = (RTMPPUSHER*)ctxt;
    if (!pusher) return;
    if (start) pusher->status |= TS_START;
    else       pusher->status &=~TS_START;
}
