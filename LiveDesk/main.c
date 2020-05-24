#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <pthread.h>
#include "stdafx.h"
#include "adev.h"
#include "vdev.h"
#include "codec.h"
#include "rtspserver.h"
#include "rtmppusher.h"
#include "recorder.h"
#include "avkcps.h"
#include "avkcpc.h"
#include "log.h"

#ifdef WIN32
#pragma warning(disable:4996)
#endif

typedef struct {
    void  *adev;
    void  *vdev;
    CODEC *aenc;
    CODEC *venc;
    void  *rtsp;
    void  *rtmp;
    void  *rec;
    void  *avkcps;
    void  *avkcpc;
    #define TS_EXIT  (1 << 0)
    int   status;
} LIVEDESK;

int main(int argc, char *argv[])
{
    LIVEDESK  livedesk = {0};
    LIVEDESK *live = &livedesk;
    int       aenctype = 0; // 0:alaw, 1:aac
    int       channels = 1, samplerate = 8000, abitrate = 16000, i;
    int       vwidth   = GetSystemMetrics(SM_CXSCREEN);
    int       vheight  = GetSystemMetrics(SM_CYSCREEN);
    int       venctype = 0, framerate= 20, vbitrate = 512000;
    int       rectype  = 0; // 0:rtsp, 1:rtmp, 2:mp4
    int       duration = 60000;
    int       avkcpport= 8000;
    char      recpath[256] = "livedesk";
    void     *avkcpc = NULL;

    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-aac") == 0) {
            aenctype = 1;
        } else if (strstr(argv[i], "-channels=") == argv[i]) {
            channels = atoi(argv[i] + 10);
        } else if (strstr(argv[i], "-samplerate=") == argv[i]) {
            samplerate = atoi(argv[i] + 12);
        } else if (strstr(argv[i], "-abitrate=") == argv[i]) {
            abitrate = atoi(argv[i] + 10);
        } else if (strstr(argv[i], "-vwidth=") == argv[i]) {
            vwidth = atoi(argv[i] + 8);
        } else if (strstr(argv[i], "-vheight=") == argv[i]) {
            vheight = atoi(argv[i] + 9);
        } else if (strstr(argv[i], "-framerate=") == argv[i]) {
            framerate = atoi(argv[i] + 11);
        } else if (strstr(argv[i], "-vbitrate=") == argv[i]) {
            vbitrate  = atoi(argv[i] + 10);
        } else if (strstr(argv[i], "-rtsp=") == argv[i]) {
            rectype = 0; strncpy(recpath, argv[i] + 6, sizeof(recpath));
        } else if (strstr(argv[i], "-rtmp=") == argv[i]) {
            rectype = 1; strncpy(recpath, argv[i] + 6, sizeof(recpath));
        } else if (strstr(argv[i], "-mp4=") == argv[i]) {
            rectype = 2; strncpy(recpath, argv[i] + 5, sizeof(recpath));
        } else if (strstr(argv[i], "-avkcps=") == argv[i]) {
            rectype = 3; avkcpport = atoi(argv[i] + 8);
        } else if (strstr(argv[i], "-duration=") == argv[i]) {
            duration = atoi(argv[i] + 10);
        }
    }
    if (rectype == 2) {
        aenctype = 1;
    }
    if (aenctype == 0) {
        channels   = 1;
        samplerate = 8000;
        abitrate   = 64000;
    }
    printf("rectype   : %s\n", rectype == 0 ? "rtsp" : rectype == 1 ? "rtmp" : rectype == 2 ? "mp4" : rectype == 3 ? "avkcps" : "unknow");
    printf("recpath   : %s\n", recpath);
    printf("duration  : %d\n", duration);
    printf("avkcpport : %d\n", avkcpport);
    printf("aenctype  : %s\n", aenctype ? "aac" : "alaw");
    printf("channels  : %d\n", channels);
    printf("samplerate: %d\n", samplerate);
    printf("abitrate  : %d\n", abitrate);
    printf("venctype  : %s\n", venctype ? "h265" : "h264");
    printf("vwidth    : %d\n", vwidth);
    printf("vheight   : %d\n", vheight);
    printf("framerate : %d\n", framerate);
    printf("vbitrate  : %d\n", vbitrate);
    printf("\n\n");

    log_init("DEBUGER");
    live->adev = adev_init(channels, samplerate);
    live->vdev = vdev_init(framerate, vwidth, vheight);
    live->aenc = aenctype ? aacenc_init(channels, samplerate, abitrate) : alawenc_init();
    live->venc = h264enc_init(framerate, vwidth, vheight, vbitrate);
    adev_set_callback(live->adev, live->aenc->write, live->aenc);
    vdev_set_callback(live->vdev, live->venc->write, live->venc);

    switch (rectype) {
    case 0: live->rtsp  = rtspserver_init(recpath, live->adev, live->vdev, live->aenc, live->venc, framerate); break;
    case 1: live->rtmp  = rtmppusher_init(recpath, live->adev, live->vdev, live->aenc, live->venc); break;
    case 2: live->rec   = ffrecorder_init(recpath, duration, channels, samplerate, vwidth, vheight, framerate, live->adev, live->vdev, live->aenc, live->venc); break;
    case 3: live->avkcps= avkcps_init(avkcpport, channels, samplerate, vwidth, vheight, framerate, live->adev, live->vdev, live->aenc, live->venc); break;
    }

    printf("\n\ntype help for more infomation and command.\n\n");
    while (!(live->status & TS_EXIT)) {
        char cmd[256];
        scanf("%256s", cmd);
        if (stricmp(cmd, "quit") == 0 || stricmp(cmd, "exit") == 0) {
            live->status |= TS_EXIT;
        } else if (rectype == 2 && stricmp(cmd, "mp4_start") == 0) {
            ffrecorder_start(live->rec, 1);
            printf("mp4 file recording started !\n");
        } else if (rectype == 2 && stricmp(cmd, "mp4_pause") == 0) {
            ffrecorder_start(live->rec, 0);
            printf("mp4 file recording paused !\n");
        } else if (rectype == 1 && stricmp(cmd, "rtmp_start") == 0) {
            rtmppusher_start(live->rtmp, 1);
            printf("rtmp push started !\n");
        } else if (rectype == 1 && stricmp(cmd, "rtmp_pause") == 0) {
            rtmppusher_start(live->rtmp, 0);
            printf("rtmp push paused !\n");
        } else if (rectype == 3 && stricmp(cmd, "avkcpc_start") == 0) {
            if (live->avkcpc == NULL) live->avkcpc = avkcpc_init("127.0.0.1", avkcpport, NULL, NULL);
        } else if (stricmp(cmd, "help") == 0) {
            printf("\nlivedesk v1.0.0\n\n");
            printf("available commmand:\n");
            printf("- help: show this mesage.\n");
            printf("- quit: quit this program.\n");
            printf("- mp4_start : start recording screen to mp4 files.\n");
            printf("- mp4_pause : pause recording screen to mp4 files.\n");
            printf("- rtmp_start: start rtmp push.\n");
            printf("- rtmp_pause: pause rtmp push.\n\n");
        }
    }

    avkcpc_exit(live->avkcpc);
    avkcps_exit(live->avkcps);
    ffrecorder_exit(live->rec );
    rtmppusher_exit(live->rtmp);
    rtspserver_exit(live->rtsp);
    adev_free(live->adev);
    vdev_free(live->vdev);
    codec_uninit(live->aenc);
    codec_uninit(live->venc);

    log_done();
    return 0;
}
