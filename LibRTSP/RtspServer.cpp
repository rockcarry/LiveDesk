#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OnDemandRTSPServer.h"
#include "rtspserver.h"

#ifdef WIN32
#pragma warning(disable:4996)
#endif

static void* rtsp_server_thread_proc(void *argv)
{
    RTSPSERVER *server = (RTSPSERVER*)argv;
    rtsp_servermain(server->name, server, &server->bexit);
    return NULL;
}

void* rtspserver_init(char *name, void *adev, void *vdev, CODEC *aenc, CODEC *venc, int frate)
{
    RTSPSERVER *server = (RTSPSERVER*)calloc(1, sizeof(RTSPSERVER));
    strncpy(server->name, name, sizeof(server->name));
    server->adev  = adev;
    server->aenc  = aenc;
    server->vdev  = vdev;
    server->venc  = venc;
    server->frate = frate;
    pthread_create(&server->pthread, NULL, rtsp_server_thread_proc, server);
    return server;
}

void rtspserver_exit(void *ctx)
{
    RTSPSERVER *server = (RTSPSERVER*)ctx;
    if (!ctx) return;

    adev_start(server->adev, 0);
    vdev_start(server->vdev, 0);
    server->bexit = 1;
    if (server->pthread) pthread_join(server->pthread, NULL);
    free(ctx);
}

int rtspserver_running_streams(void *ctx)
{
    RTSPSERVER *server = (RTSPSERVER*)ctx;
    return server ? server->running_streams : 0;
}
