#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "stdafx.h"
#include "ringbuf.h"
#include "codec.h"
#include "faac.h"
#include "log.h"

#ifdef WIN32
#define timespec timespec32
#endif

#define IN_BUF_SIZE  (1024 * 4 * 4)
#define OUT_BUF_SIZE (1024 * 8 * 1)
typedef struct {
    CODEC_INTERFACE_FUNCS

    uint8_t  ibuff[IN_BUF_SIZE];
    int      ihead;
    int      itail;
    int      isize;

    uint8_t  obuff[OUT_BUF_SIZE];
    int      ohead;
    int      otail;
    int      osize;

    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    int      status;

    pthread_mutex_t imutex;
    pthread_cond_t  icond;
    pthread_mutex_t omutex;
    pthread_cond_t  ocond;
    pthread_t       thread;

    faacEncHandle faacenc;
    unsigned long insamples;
    unsigned long outbufsize;
    unsigned long aaccfgsize;
    uint8_t      *aaccfgptr;
} AACENC;

static void* aenc_encode_thread_proc(void *param)
{
    AACENC *enc = (AACENC*)param;
    uint8_t outbuf[8192];
    int32_t len = 0;

    while (!(enc->status & TS_EXIT)) {
        if (!(enc->status & TS_START)) {
            usleep(100*1000); continue;
        }

        pthread_mutex_lock(&enc->imutex);
        while (enc->isize < (int)(enc->insamples * sizeof(int16_t)) && !(enc->status & TS_EXIT)) pthread_cond_wait(&enc->icond, &enc->imutex);
        if (!(enc->status & TS_EXIT)) {
            len = faacEncEncode(enc->faacenc, (int32_t*)(enc->ibuff + enc->ihead), enc->insamples, outbuf, sizeof(outbuf));
            enc->ihead += enc->insamples * sizeof(int16_t);
            enc->isize -= enc->insamples * sizeof(int16_t);
            if (enc->ihead == sizeof(enc->ibuff)) enc->ihead = 0;
        }
        pthread_mutex_unlock(&enc->imutex);

        pthread_mutex_lock(&enc->omutex);
        if (len > 0 && sizeof(uint32_t) + sizeof(uint32_t) + len <= sizeof(enc->obuff) - enc->osize) {
            uint32_t timestamp = get_tick_count();
            uint32_t typelen   = 'A' | (len << 8);
            enc->otail  = ringbuf_write(enc->obuff, sizeof(enc->obuff), enc->otail, (uint8_t*)&timestamp, sizeof(timestamp));
            enc->otail  = ringbuf_write(enc->obuff, sizeof(enc->obuff), enc->otail, (uint8_t*)&typelen  , sizeof(typelen  ));
            enc->otail  = ringbuf_write(enc->obuff, sizeof(enc->obuff), enc->otail, outbuf, len);
            enc->osize += sizeof(timestamp) + sizeof(typelen) + len;
            pthread_cond_signal(&enc->ocond);
        } else {
            log_printf("aenc drop data %d !\n", len);
        }
        pthread_mutex_unlock(&enc->omutex);
    }
    return NULL;
}

static void uninit(void *ctxt)
{
    AACENC *enc = (AACENC*)ctxt;
    if (!ctxt) return;

    pthread_mutex_lock(&enc->imutex);
    enc->status |= TS_EXIT;
    pthread_cond_signal(&enc->icond);
    pthread_mutex_unlock(&enc->imutex);
    pthread_join(enc->thread, NULL);
    if (enc->faacenc) faacEncClose(enc->faacenc);

    pthread_mutex_destroy(&enc->imutex);
    pthread_cond_destroy (&enc->icond );
    pthread_mutex_destroy(&enc->omutex);
    pthread_cond_destroy (&enc->ocond );
    free(enc);
}

static void write(void *ctxt, void *buf[8], int len[8])
{
    int nwrite;
    AACENC *enc = (AACENC*)ctxt;
    if (!ctxt) return;
    pthread_mutex_lock(&enc->imutex);
    nwrite = MIN(len[0], (int)sizeof(enc->ibuff) - enc->isize);
    enc->itail = ringbuf_write(enc->ibuff, sizeof(enc->ibuff), enc->itail, buf[0], nwrite);
    enc->isize+= nwrite;
    pthread_cond_signal(&enc->icond);
    pthread_mutex_unlock(&enc->imutex);
}

static int read(void *ctxt, void *buf, int len, int *fsize, int *key, uint32_t *pts, int timeout)
{
    AACENC *enc = (AACENC*)ctxt;
    uint32_t timestamp = 0;
    int32_t  framesize = 0, readsize = 0, ret = 0;
    struct   timespec ts;
    if (!ctxt) return 0;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += timeout*1000*1000;
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;

    pthread_mutex_lock(&enc->omutex);
    while (timeout && enc->osize <= 0 && (enc->status & TS_START) && ret != ETIMEDOUT) ret = pthread_cond_timedwait(&enc->ocond, &enc->omutex, &ts);
    if (enc->osize > 0) {
        enc->ohead = ringbuf_read(enc->obuff, sizeof(enc->obuff), enc->ohead, (uint8_t*)&timestamp, sizeof(timestamp));
        enc->ohead = ringbuf_read(enc->obuff, sizeof(enc->obuff), enc->ohead, (uint8_t*)&framesize, sizeof(framesize));
        enc->osize-= sizeof(timestamp) + sizeof(framesize);
        framesize  = ((uint32_t)framesize >> 8);
        readsize   = MIN(len, framesize);
        enc->ohead = ringbuf_read(enc->obuff, sizeof(enc->obuff), enc->ohead,  buf , readsize);
        enc->ohead = ringbuf_read(enc->obuff, sizeof(enc->obuff), enc->ohead,  NULL, framesize - readsize);
        enc->osize-= framesize;
    }
    if (pts  ) *pts   = timestamp;
    if (fsize) *fsize = framesize;
    if (key  ) *key   = 1;
    pthread_mutex_unlock(&enc->omutex);
    return readsize;
}

static void start(void *ctxt, int start)
{
    AACENC *enc = (AACENC*)ctxt;
    if (!ctxt) return;
    if (start) {
        enc->status |= TS_START;
    } else {
        pthread_mutex_lock(&enc->omutex);
        enc->status &= ~TS_START;
        pthread_cond_signal(&enc->ocond);
        pthread_mutex_unlock(&enc->omutex);
    }
}

static void reset(void *ctxt, int type)
{
    AACENC *enc = (AACENC*)ctxt;
    if (!ctxt) return;
    if (type & CODEC_CLEAR_INBUF) {
        pthread_mutex_lock(&enc->imutex);
        enc->ihead = enc->itail = enc->isize = 0;
        pthread_mutex_unlock(&enc->imutex);
    }
    if (type & CODEC_CLEAR_OUTBUF) {
        pthread_mutex_lock(&enc->omutex);
        enc->ohead = enc->otail = enc->osize = 0;
        pthread_mutex_unlock(&enc->omutex);
    }
}

static void obuflock(void *ctxt, uint8_t **pbuf, int *max, int *head, int *tail, int *size)
{
    AACENC *enc = (AACENC*)ctxt;
    if (!ctxt) return;
    pthread_mutex_lock(&enc->omutex);
    *pbuf = enc->obuff;
    *max  = sizeof(enc->obuff);
    *head = enc->ohead;
    *tail = enc->otail;
    *size = enc->osize;
}

static void obufunlock(void *ctxt, int head, int tail, int size)
{
    AACENC *enc = (AACENC*)ctxt;
    if (!ctxt) return;
    enc->ohead = head;
    enc->otail = tail;
    enc->osize = size;
    pthread_mutex_unlock(&enc->omutex);
}

CODEC* aacenc_init(int channels, int samplerate, int bitrate)
{
    faacEncConfigurationPtr conf;
    AACENC *enc = calloc(1, sizeof(AACENC));
    if (!enc) return NULL;

    strncpy(enc->name, "aacenc", sizeof(enc->name));
    enc->uninit     = uninit;
    enc->write      = write;
    enc->read       = read;
    enc->start      = start;
    enc->reset      = reset;
    enc->obuflock   = obuflock;
    enc->obufunlock = obufunlock;

    // init mutex & cond
    pthread_mutex_init(&enc->imutex, NULL);
    pthread_cond_init (&enc->icond , NULL);
    pthread_mutex_init(&enc->omutex, NULL);
    pthread_cond_init (&enc->ocond , NULL);

    enc->faacenc = faacEncOpen((unsigned long)samplerate, (unsigned int)channels, &enc->insamples, &enc->outbufsize);
    conf = faacEncGetCurrentConfiguration(enc->faacenc);
    conf->aacObjectType = LOW;
    conf->mpegVersion   = MPEG4;
    conf->useLfe        = 0;
    conf->useTns        = 0;
    conf->allowMidside  = 0;
    conf->bitRate       = bitrate;
    conf->outputFormat  = 0;
    conf->inputFormat   = FAAC_INPUT_16BIT;
    conf->shortctl      = SHORTCTL_NORMAL;
    conf->quantqual     = 88;
    faacEncSetConfiguration(enc->faacenc, conf);
    faacEncGetDecoderSpecificInfo(enc->faacenc, &enc->aaccfgptr, &enc->aaccfgsize);
    memcpy(enc->aacinfo, enc->aaccfgptr, MIN(sizeof(enc->aacinfo), enc->aaccfgsize));

    pthread_create(&enc->thread, NULL, aenc_encode_thread_proc, enc);
    return (CODEC*)enc;
}
