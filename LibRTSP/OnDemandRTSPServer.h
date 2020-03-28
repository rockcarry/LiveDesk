#ifndef __ONDEMAND_RTSP_SERVER_H__
#define __ONDEMAND_RTSP_SERVER_H__

#include <stdint.h>
#include <pthread.h>
#include "rtspserver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char            name[256];
    int             audio_enctype; // 0 - alaw, 1 - aac
    int             video_enctype; // 0 - h264, 1 - h265
    uint8_t         aac_config[2]; // aac_config
    int             frate;         // video frame rate
    pthread_t       pthread;
    char            bexit;
    int             running_streams;
    void           *adev;
    void           *vdev;
    PFN_AVDEV_IOCTL aioctl;
    PFN_AVDEV_IOCTL vioctl;
} RTSPSERVER;

int rtsp_servermain(char *name, RTSPSERVER *server, char *pexit);

#ifdef __cplusplus
}
#endif

#endif
