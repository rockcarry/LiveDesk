#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <pthread.h>
#include "stdafx.h"
#include "adev.h"
#include "aenc.h"
#include "vdev.h"
#include "venc.h"
#include "rtspserver.h"
#include "log.h"

typedef struct {
    void *adev;
    void *aenc;
    void *vdev;
    void *venc;
    void *rtsp;
    #define TS_EXIT  (1 << 0)
    int  status;
} LIVEDESK;

int main(int argc, char *argv[])
{
    LIVEDESK  livedesk = {0};
    LIVEDESK *live = &livedesk;
    int       aenctype = 0, rectype = 0, i;
    int       channels = 1, samplerate = 8000, abitrate = 16000;
    int       vwidth   = GetSystemMetrics(SM_CXSCREEN);
    int       vheight  = GetSystemMetrics(SM_CYSCREEN);
    int       venctype = 0, framerate= 10, vbitrate = 256000;
    uint8_t  *aacinfo  = NULL;

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
        }
    }
    if (!aenctype) {
        channels   = 1;
        samplerate = 8000;
        abitrate   = 64000;
    }
    printf("aenctype  : %s\n", aenctype ? "aac" : "alaw");
    printf("channels  : %d\n", channels);
    printf("samplerate: %d\n", samplerate);
    printf("abitrate  : %d\n", abitrate);
    printf("venctype  : %s\n", venctype ? "h265" : "h264");
    printf("vwidth    : %d\n", vwidth);
    printf("vheight   : %d\n", vheight);
    printf("framerate : %d\n", framerate);
    printf("vbitrate  : %d\n", vbitrate);

    log_init("DEBUGER");

    live->adev = adev_init(channels, samplerate, !aenctype, aenctype ? 16*1024 : 960);
    live->aenc = aenc_init(live->adev, channels, samplerate, abitrate, &aacinfo);
    live->vdev = vdev_init(framerate, vwidth, vheight);
    live->venc = venc_init(live->vdev, framerate, vwidth, vheight, vbitrate);
    live->rtsp = rtspserver_init(aenctype ? live->aenc : live->adev, aenctype ? aenc_ioctl : adev_ioctl, live->venc, venc_ioctl, aenctype, venctype, aacinfo, framerate);

    while (!(live->status & TS_EXIT)) {
        int cmd = _getch();
        switch (cmd) {
        case 'q': case 'Q':
            live->status |= TS_EXIT;
            break;
        }
    }

    rtspserver_exit(live->rtsp);
    venc_free(live->venc);
    vdev_free(live->vdev);
    aenc_free(live->aenc);
    adev_free(live->adev);

    log_done();
    return 0;
}
