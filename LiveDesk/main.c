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
#include "ffrdps.h"
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
    void  *ffrdps;
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
    int       rectype  = 0; // 0:rtsp, 1:rtmp, 2:avi, 3:mp4, 4:avkcp, 5:ffrdp
    int       duration = 60000;
    int       avkcpport= 8000;
    int       ffrdpport= 8000;
    int       ffrdpauto= 0; // ffrdp auto bitrate (adaptive bitrate)
    char      recpath[256] = "livedesk";
    void     *avkcpc = NULL;
    char      ffrdptxkey[32] = {0};
    char      ffrdprxkey[32] = {0};

    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "--aac") == 0) {
            aenctype = 1;
        } else if (strcmp(argv[i], "--h265") == 0) {
            venctype = 1;
        } else if (strstr(argv[i], "--channels=") == argv[i]) {
            channels = atoi(argv[i] + 11);
        } else if (strstr(argv[i], "--samplerate=") == argv[i]) {
            samplerate = atoi(argv[i] + 13);
        } else if (strstr(argv[i], "--abitrate=") == argv[i]) {
            abitrate = atoi(argv[i] + 11);
        } else if (strstr(argv[i], "--vwidth=") == argv[i]) {
            vwidth = atoi(argv[i] + 9);
        } else if (strstr(argv[i], "--vheight=") == argv[i]) {
            vheight = atoi(argv[i] + 10);
        } else if (strstr(argv[i], "--framerate=") == argv[i]) {
            framerate = atoi(argv[i] + 12);
        } else if (strstr(argv[i], "--vbitrate=") == argv[i]) {
            if (strcmp(argv[i] + 11, "auto") == 0) {
                vbitrate = 10000000; ffrdpauto = 1;
            } else {
                vbitrate = atoi(argv[i] + 11);
            }
        } else if (strstr(argv[i], "--rtsp=") == argv[i]) {
            rectype = 0; strncpy(recpath, argv[i] + 7, sizeof(recpath));
        } else if (strstr(argv[i], "--rtmp=") == argv[i]) {
            rectype = 1; strncpy(recpath, argv[i] + 7, sizeof(recpath));
        } else if (strstr(argv[i], "--avi=") == argv[i]) {
            rectype = 2; strncpy(recpath, argv[i] + 6, sizeof(recpath));
        } else if (strstr(argv[i], "--mp4=") == argv[i]) {
            rectype = 3; strncpy(recpath, argv[i] + 6, sizeof(recpath));
        } else if (strstr(argv[i], "--avkcps=") == argv[i]) {
            rectype = 4; avkcpport = atoi(argv[i] + 9);
        } else if (strstr(argv[i], "--ffrdps=") == argv[i]) {
            rectype = 5; ffrdpport = atoi(argv[i] + 9);
        } else if (strstr(argv[i], "--ffrdpstxkey=") == argv[i]) {
            strncpy(ffrdptxkey, argv[i] + 14, sizeof(ffrdptxkey));
        } else if (strstr(argv[i], "--ffrdpsrxkey=") == argv[i]) {
            strncpy(ffrdprxkey, argv[i] + 14, sizeof(ffrdprxkey));
        } else if (strstr(argv[i], "--duration=") == argv[i]) {
            duration = atoi(argv[i] + 11);
        }
    }
    if (rectype == 2) {
        aenctype = 0;
    }
    if (rectype == 3) {
        aenctype = 1;
    }
    if (aenctype == 0) {
        channels   = 1;
        samplerate = 8000;
        abitrate   = 64000;
    }
    printf("rectype   : %s\n", rectype == 0 ? "rtsp" : rectype == 1 ? "rtmp" : rectype == 2 ? "avi" : rectype == 3 ? "mp4" : rectype == 4 ? "avkcps" : rectype == 5 ? "ffrdps" : "unknow");
    printf("recpath   : %s\n", recpath);
    printf("duration  : %d\n", duration);
    printf("avkcpport : %d\n", avkcpport);
    printf("ffrdpport : %d\n", ffrdpport);
    printf("ffrdptxkey: %s\n", ffrdptxkey);
    printf("ffrdprxkey: %s\n", ffrdprxkey);
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
    live->venc = venctype ? h265enc_init(framerate, vwidth, vheight, vbitrate) : h264enc_init(framerate, vwidth, vheight, vbitrate);
    adev_set_callback(live->adev, live->aenc->write, live->aenc);
    vdev_set_callback(live->vdev, live->venc->write, live->venc);

    switch (rectype) {
    case 0: live->rtsp  = rtspserver_init(recpath, live->adev, live->vdev, live->aenc, live->venc, framerate); break;
    case 1: live->rtmp  = rtmppusher_init(recpath, live->adev, live->vdev, live->aenc, live->venc); break;
    case 2: live->rec   = ffrecorder_init(recpath, "avi", duration, channels, samplerate, vwidth, vheight, framerate, live->adev, live->vdev, live->aenc, live->venc); break;
    case 3: live->rec   = ffrecorder_init(recpath, "mp4", duration, channels, samplerate, vwidth, vheight, framerate, live->adev, live->vdev, live->aenc, live->venc); break;
    case 4: live->avkcps= avkcps_init(avkcpport, channels, samplerate, vwidth, vheight, framerate, live->adev, live->vdev, live->aenc, live->venc); break;
    case 5: live->ffrdps= ffrdps_init(ffrdpport, ffrdptxkey, ffrdprxkey, channels, samplerate, vwidth, vheight, framerate, live->adev, live->vdev, live->aenc, live->venc); break;
    }

    if (rectype == 5 && ffrdpauto) { // setup adaptive bitrate list
        int blist[16] = { 250000, 500000, 1000000, 1200000, 1400000, 1600000, 1800000, 2000000, 2100000, 2200000, 2300000, 2400000, 2500000, 2600000, 2700000 };
        ffrdps_adaptive_bitrate_setup (live->ffrdps, blist, 15);
        ffrdps_adaptive_bitrate_enable(live->ffrdps, 1);
    }

    printf("\n\ntype help for more infomation and command.\n\n");
    while (!(live->status & TS_EXIT)) {
        char cmd[256];
        scanf("%256s", cmd);
        if (stricmp(cmd, "quit") == 0 || stricmp(cmd, "exit") == 0) {
            live->status |= TS_EXIT;
        } else if ((rectype == 2 || rectype == 3) && stricmp(cmd, "record_start") == 0) {
            ffrecorder_start(live->rec, 1);
            printf("file recording started !\n");
        } else if ((rectype == 2 || rectype == 3) && stricmp(cmd, "record_pause") == 0) {
            ffrecorder_start(live->rec, 0);
            printf("file recording paused !\n");
        } else if (rectype == 1 && stricmp(cmd, "rtmp_start") == 0) {
            rtmppusher_start(live->rtmp, 1);
            printf("rtmp push started !\n");
        } else if (rectype == 1 && stricmp(cmd, "rtmp_pause") == 0) {
            rtmppusher_start(live->rtmp, 0);
            printf("rtmp push paused !\n");
        } else if (rectype == 5 && stricmp(cmd, "ffrdps_dump") == 0) {
            int val; scanf("%d", &val);
            if (live->ffrdps) ffrdps_dump(live->ffrdps, val);
        } else if (rectype == 5 && stricmp(cmd, "ffrdps_adaptive_bitrate_en") == 0) {
            int val; scanf("%d", &val);
            if (live->ffrdps) ffrdps_adaptive_bitrate_enable(live->ffrdps, val);
        } else if (rectype == 5 && stricmp(cmd, "ffrdps_reconfig_bitrate") == 0) {
            int val; scanf("%d", &val);
            if (live->ffrdps) ffrdps_reconfig_bitrate(live->ffrdps, val);
        } else if (stricmp(cmd, "help") == 0) {
            printf("\nlivedesk v1.0.0\n\n");
            printf("available commmand:\n");
            printf("- help: show this mesage.\n");
            printf("- quit: quit this program.\n");
            printf("- record_start: start recording screen to files.\n");
            printf("- record_pause: pause recording screen to files.\n");
            printf("- rtmp_start  : start rtmp push.\n");
            printf("- rtmp_pause  : pause rtmp push.\n");
            printf("- ffrdps_dump : dump ffrdps server.\n\n");
        }
    }

    avkcpc_exit(live->avkcpc);
    avkcps_exit(live->avkcps);
    ffrdps_exit(live->ffrdps);
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
