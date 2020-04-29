#ifndef __ONDEMAND_RTSP_SERVER_H__
#define __ONDEMAND_RTSP_SERVER_H__

#include <stdint.h>
#include <pthread.h>
#include "rtspserver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char        name[256];
    int         frate;
    pthread_t   pthread;
    char        bexit;
    int         running_streams;
    void       *adev;
    void       *vdev;
    CODEC      *aenc;
    CODEC      *venc;
} RTSPSERVER;

int rtsp_servermain(char *name, RTSPSERVER *server, char *pexit);

#ifdef __cplusplus
}
#endif

#endif
