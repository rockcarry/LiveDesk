#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <pthread.h>
#include "stdafx.h"
#include "aidev.h"
#include "videv.h"
#include "vienc.h"
#include "rtspserver.h"
#include "log.h"

#define VIDEO_FRAME_RATE  10
#define VIDEO_WIDTH       GetSystemMetrics(SM_CXSCREEN)
#define VIDEO_HEIGHT      GetSystemMetrics(SM_CYSCREEN)
#define VIDEO_BITRATE     256000

typedef struct {
    void *aidev;
    void *videv;
    void *vienc;
    void *rtsp ;
    #define TS_EXIT  (1 << 0)
    int  status;
} LIVEDESK;

int main(int argc, char *argv[])
{
    LIVEDESK  livedesk = {0};
    LIVEDESK *live = &livedesk;

    log_init("DEBUGER");

    live->aidev = aidev_init();
    live->videv = videv_init(VIDEO_FRAME_RATE, VIDEO_WIDTH, VIDEO_HEIGHT);
    live->vienc = vienc_init(live->videv, VIDEO_FRAME_RATE, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_BITRATE);
    live->rtsp  = rtspserver_init(live->aidev, aidev_read, live->vienc, vienc_read, 0, 0, NULL);

    while (!(live->status & TS_EXIT)) {
        int cmd = _getch();
        switch (cmd) {
        case 'q': case 'Q':
            live->status |= TS_EXIT;
            break;
        }
    }

    aidev_start(live->aidev, 0);
    videv_start(live->videv, 0);
    vienc_start(live->vienc, 0);

    rtspserver_exit(live->rtsp);
    vienc_free(live->vienc);
    videv_free(live->videv);
    aidev_free(live->aidev);

    log_done();
    return 0;
}
