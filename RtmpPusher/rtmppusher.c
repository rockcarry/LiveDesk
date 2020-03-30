#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "adev.h"
#include "aenc.h"
#include "venc.h"
#include "librtmp.h"
#include "rtmppusher.h"

#ifdef WIN32
#include <winsock.h>
#define usleep(t)  Sleep((t) / 1000)
#endif

typedef struct {
    void     *rtmp;
    void     *aenc;
    void     *venc;

    PFN_AVDEV_IOCTL aioctl;
    PFN_AVDEV_IOCTL vioctl;

    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    uint32_t  status;
    pthread_t pthread;
    int       aenctype;
} RTMPPUSHER;

static void* rtmppush_thread_proc(void *argv)
{
    RTMPPUSHER *pusher = (RTMPPUSHER*)argv;
    uint8_t buf[512*1024];
    int framesize, readsize;

    while (!(pusher->status & TS_EXIT)) {
        if (!(pusher->status & TS_START)) {
            usleep(100*1000);
            continue;
        }

        readsize = pusher->aioctl(pusher->aenc, AENC_CMD_READ, buf, pusher->aenctype ? sizeof(buf) : 160, &framesize);
        if (readsize > 0) {
            if (pusher->aenctype) {
                rtmp_push_aac (pusher->rtmp, buf, readsize);
            } else {
                rtmp_push_alaw(pusher->rtmp, buf, readsize);
            }
        }

        readsize = pusher->vioctl(pusher->venc, VENC_CMD_READ, buf, sizeof(buf), &framesize);
        if (readsize > 0) {
            rtmp_push_h264(pusher->rtmp, buf, readsize);
        }
    }
    return NULL;
}

void* rtmppusher_init(char *url, int aenctype, unsigned char *aaccfg, void *aenc, PFN_AVDEV_IOCTL aioctl, void *venc, PFN_AVDEV_IOCTL vioctl)
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

    pusher->rtmp     = rtmp_push_init(url, aaccfg);
    pusher->aenctype = aenctype;
    pusher->aenc     = aenc;
    pusher->venc     = venc;
    pusher->aioctl   = aioctl;
    pusher->vioctl   = vioctl;

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
    if (start) {
        pusher->status |= TS_START;
        pusher->aioctl(pusher->aenc, AENC_CMD_RESET_BUFFER, NULL, 0, NULL);
        pusher->vioctl(pusher->venc, VENC_CMD_RESET_BUFFER, NULL, 0, NULL);
        pusher->vioctl(pusher->venc, VENC_CMD_REQUEST_IDR , NULL, 0, NULL);
        pusher->aioctl(pusher->aenc, AENC_CMD_START, NULL, 0, NULL);
        pusher->vioctl(pusher->venc, VENC_CMD_START, NULL, 0, NULL);
    } else {
        pusher->status &=~TS_START;
        pusher->aioctl(pusher->aenc, ADEV_CMD_STOP, NULL, 0, NULL);
        pusher->vioctl(pusher->venc, VENC_CMD_STOP, NULL, 0, NULL);
    }
}
