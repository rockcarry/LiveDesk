#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OnDemandRTSPServer.h"
#include "rtspserver.h"

static void* rtsp_server_thread_proc(void *argv)
{
    RTSPSERVER *server = (RTSPSERVER*)argv;
    rtsp_servermain(server, &server->bexit);
    return NULL;
}

void* rtspserver_init(void *adev, PFN_READDATA aread, void *vdev, PFN_READDATA vread, int aenc_type, int venc_type, uint8_t *aac_config)
{
    RTSPSERVER *server = (RTSPSERVER*)calloc(1, sizeof(RTSPSERVER));
    server->audio_enctype = aenc_type;
    server->video_enctype = venc_type;
    server->adev = adev;
    server->aread= aread;
    server->vdev = vdev;
    server->vread= vread;

    if (aac_config) {
        server->aac_config[0] = aac_config[0];
        server->aac_config[1] = aac_config[1];
    }

    // create server thread
    pthread_create(&server->pthread, NULL, rtsp_server_thread_proc, server);
    return server;
}

void rtspserver_exit(void *ctx)
{
    RTSPSERVER *server = (RTSPSERVER*)ctx;
    if (!ctx) return;

    server->bexit = 1;
    if (server->pthread) pthread_join(server->pthread, NULL);
    free(ctx);
}

int rtspserver_running_streams(void *ctx)
{
    RTSPSERVER *server = (RTSPSERVER*)ctx;
    return server ? server->running_streams : 0;
}

#ifdef WIN32
#include <windows.h>
void gettimeofday(struct timeval *tp, void *tzp)
{
    uint64_t intervals;
    FILETIME ftime;

    GetSystemTimeAsFileTime(&ftime);

    /*
     * A file time is a 64-bit value that represents the number
     * of 100-nanosecond intervals that have elapsed since
     * January 1, 1601 12:00 A.M. UTC.
     *
     * Between January 1, 1970 (Epoch) and January 1, 1601 there were
     * 134744 days,
     * 11644473600 seconds or
     * 11644473600,000,000,0 100-nanosecond intervals.
     *
     * See also MSKB Q167296.
     */

    intervals   = ((uint64_t)ftime.dwHighDateTime << 32) | ftime.dwLowDateTime;
    intervals  -= 116444736000000000;
    tp->tv_sec  = (long) (intervals / 10000000);
    tp->tv_usec = (long) (intervals % 10000000) / 10;
}
#endif
