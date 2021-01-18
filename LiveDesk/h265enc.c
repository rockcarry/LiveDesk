#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "stdafx.h"
#include "ringbuf.h"
#include "codec.h"
#include "x265.h"
#include "log.h"

#include "libavutil/time.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"

#define YUV_BUF_NUM    3
#define OUT_BUF_SIZE  (2 * 1024 * 1024)
typedef struct {
    CODEC_INTERFACE_FUNCS

    x265_param   param;
    x265_encoder *x265;
    int      iw;
    int      ih;
    int      ow;
    int      oh;

    uint8_t *ibuff[YUV_BUF_NUM];
    int      ihead;
    int      itail;
    int      isize;

    uint8_t  obuff[OUT_BUF_SIZE];
    int      ohead;
    int      otail;
    int      osize;

    #define TS_EXIT             (1 << 0)
    #define TS_START            (1 << 1)
    #define TS_REQUEST_IDR      (1 << 2)
    #define TS_KEYFRAME_DROPPED (1 << 3)
    int      status;

    pthread_mutex_t imutex;
    pthread_cond_t  icond;
    pthread_mutex_t omutex;
    pthread_cond_t  ocond;
    pthread_t       thread;

    struct SwsContext *sws_context;
} H265ENC;

static void* venc_encode_thread_proc(void *param)
{
    H265ENC  *enc = (H265ENC*)param;
    uint8_t  *yuv = NULL;
    x265_nal *nals= NULL;
    x265_picture pic_in, pic_out;
    int32_t key, len, num, i;

    x265_picture_init(&enc->param, &pic_in );
    x265_picture_init(&enc->param, &pic_out);
    pic_in.stride[0] = enc->ow;
    pic_in.stride[1] = enc->ow / 2;
    pic_in.stride[2] = enc->ow / 2;

    while (!(enc->status & TS_EXIT)) {
        if (!(enc->status & TS_START)) {
            usleep(100*1000); continue;
        }

        pthread_mutex_lock(&enc->imutex);
        while (enc->isize <= 0 && !(enc->status & TS_EXIT)) pthread_cond_wait(&enc->icond, &enc->imutex);
        if (enc->isize > 0) {
            pic_in.planes[0] = enc->ibuff[enc->ihead];
            pic_in.planes[1] = enc->ibuff[enc->ihead] + enc->ow * enc->oh * 4 / 4;
            pic_in.planes[2] = enc->ibuff[enc->ihead] + enc->ow * enc->oh * 5 / 4;
            pic_in.sliceType = X265_TYPE_AUTO;
            if (enc->status & TS_REQUEST_IDR) {
                enc->status     &= ~TS_REQUEST_IDR;
                pic_in.sliceType =  X265_TYPE_IDR;
            }
            x265_encoder_encode(enc->x265, &nals, &num, &pic_in, &pic_out);
            for (len=0,i=0; i<num; i++) len += nals[i].sizeBytes;
            if (++enc->ihead == YUV_BUF_NUM) enc->ihead = 0;
            enc->isize--;
        } else {
            len = 0;
        }
        pthread_mutex_unlock(&enc->imutex);
        if (len <= 0) continue;

        pthread_mutex_lock(&enc->omutex);
        key = (nals[0].type == NAL_UNIT_VPS);
        if ((enc->status & TS_KEYFRAME_DROPPED) && !key) {
            log_printf("h265enc last key frame has dropped, and current frame is non-key frame, so drop it !\n");
        } else {
            if (sizeof(uint32_t) + sizeof(uint32_t) + len <= sizeof(enc->obuff) - enc->osize) {
                uint32_t timestamp = get_tick_count();
                uint32_t typelen   = (key ? 'V' : 'v') | (len << 8);
                enc->otail = ringbuf_write(enc->obuff, sizeof(enc->obuff), enc->otail, (uint8_t*)&timestamp, sizeof(timestamp));
                enc->otail = ringbuf_write(enc->obuff, sizeof(enc->obuff), enc->otail, (uint8_t*)&typelen  , sizeof(typelen  ));
                enc->osize+= sizeof(timestamp) + sizeof(typelen);
                for (i=0; i<num; i++) {
                    enc->otail = ringbuf_write(enc->obuff, sizeof(enc->obuff), enc->otail, nals[i].payload, nals[i].sizeBytes);
                }
                enc->osize += len;
                pthread_cond_signal(&enc->ocond);
                if (key) enc->status &= ~TS_KEYFRAME_DROPPED;
            } else {
                log_printf("h265enc %s frame dropped !\n", key ? "key" : "non-key");
                if (key) enc->status |=  TS_KEYFRAME_DROPPED;
            }
        }
        pthread_mutex_unlock(&enc->omutex);
    }
    return NULL;
}

static void uninit(void *ctxt)
{
    H265ENC *enc = (H265ENC*)ctxt;
    if (!ctxt) return;

    pthread_mutex_lock(&enc->imutex);
    enc->status |= TS_EXIT;
    pthread_cond_signal(&enc->icond);
    pthread_mutex_unlock(&enc->imutex);
    pthread_join(enc->thread, NULL);

    if (enc->x265) x265_encoder_close(enc->x265);
    if (enc->sws_context) sws_freeContext(enc->sws_context);

    pthread_mutex_destroy(&enc->imutex);
    pthread_cond_destroy (&enc->icond );
    pthread_mutex_destroy(&enc->omutex);
    pthread_cond_destroy (&enc->ocond );
    free(enc);
}

static void write(void *ctxt, void *buf[8], int len[8])
{
    H265ENC *enc = (H265ENC*)ctxt;
    if (!ctxt) return;
    pthread_mutex_lock(&enc->imutex);
    if (enc->isize < YUV_BUF_NUM) {
        AVFrame picsrc = {0}, picdst = {0};

        if (enc->iw != len[1] || enc->ih != len[2]) {
            enc->iw  = len[1];
            enc->ih  = len[2];
            if (enc->sws_context) {
                sws_freeContext(enc->sws_context);
                enc->sws_context = NULL;
            }
        }
        if (!enc->sws_context) {
            enc->sws_context = sws_getContext(enc->iw, enc->ih, AV_PIX_FMT_BGRA, enc->ow, enc->oh, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
        }

        picsrc.data[0]     = buf[0];
        picsrc.linesize[0] = len[3];
        picdst.linesize[0] = enc->ow;
        picdst.linesize[1] = enc->ow / 2;
        picdst.linesize[2] = enc->ow / 2;
        picdst.data[0] = enc->ibuff[enc->itail];
        picdst.data[1] = picdst.data[0] + enc->ow * enc->oh * 4 / 4;
        picdst.data[2] = picdst.data[0] + enc->ow * enc->oh * 5 / 4;

        sws_scale(enc->sws_context, (const uint8_t * const*)picsrc.data, picsrc.linesize, 0, enc->ih, picdst.data, picdst.linesize);
        if (++enc->itail == YUV_BUF_NUM) enc->itail = 0;
        enc->isize++;
    }
    pthread_cond_signal(&enc->icond);
    pthread_mutex_unlock(&enc->imutex);
}

static int read(void *ctxt, void *buf, int len, int *fsize, int *key, uint32_t *pts, int timeout)
{
    H265ENC *enc = (H265ENC*)ctxt;
    uint32_t timestamp = 0;
    int32_t  typelen = 0, framesize = 0, readsize = 0, ret = 0;
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
        enc->ohead = ringbuf_read(enc->obuff, sizeof(enc->obuff), enc->ohead, (uint8_t*)&typelen  , sizeof(typelen  ));
        enc->osize-= sizeof(timestamp) + sizeof(framesize);
        framesize  = ((uint32_t)typelen >> 8);
        readsize   = MIN(len, framesize);
        enc->ohead = ringbuf_read(enc->obuff, sizeof(enc->obuff), enc->ohead,  buf , readsize);
        enc->ohead = ringbuf_read(enc->obuff, sizeof(enc->obuff), enc->ohead,  NULL, framesize - readsize);
        enc->osize-= framesize;
    }
    if (pts  ) *pts   = timestamp;
    if (fsize) *fsize = framesize;
    if (key  ) *key   = ((typelen & 0xFF) == 'V');
    pthread_mutex_unlock(&enc->omutex);
    return readsize;
}

static void start(void *ctxt, int start)
{
    H265ENC *enc = (H265ENC*)ctxt;
    if (!ctxt) return;
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
    H265ENC *enc = (H265ENC*)ctxt;
    if (!ctxt) return;
    if (type & CODEC_RESET_CLEAR_INBUF) {
        pthread_mutex_lock(&enc->imutex);
        enc->ihead = enc->itail = enc->isize = 0;
        pthread_mutex_unlock(&enc->imutex);
    }
    if (type & CODEC_RESET_CLEAR_OUTBUF) {
        pthread_mutex_lock(&enc->omutex);
        enc->ohead = enc->otail = enc->osize = 0;
        pthread_mutex_unlock(&enc->omutex);
    }
    if (type & CODEC_RESET_REQUEST_IDR) {
        enc->status |= TS_REQUEST_IDR;
    }
}

static void obuflock(void *ctxt, uint8_t **pbuf, int *max, int *head, int *tail, int *size)
{
    H265ENC *enc = (H265ENC*)ctxt;
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
    H265ENC *enc = (H265ENC*)ctxt;
    if (!ctxt) return;
    enc->ohead = head;
    enc->otail = tail;
    enc->osize = size;
    pthread_mutex_unlock(&enc->omutex);
}

static void reconfig(CODEC *codec, int bitrate)
{
    H265ENC *enc = (H265ENC*)codec;
    int      ret;
    enc->param.rc.bitrate         = bitrate / 1000;
    enc->param.rc.rateControlMode = X265_RC_ABR;
    enc->param.rc.vbvMaxBitrate = 2 * bitrate / 1000;
    enc->param.rc.vbvBufferSize = 2 * bitrate / 1000;
    ret = x265_encoder_reconfig(enc->x265, &enc->param);
    printf("x265_encoder_reconfig bitrate: %d, ret = %d\n", bitrate, ret);
}

CODEC* h265enc_init(int frate, int w, int h, int bitrate)
{
    x265_nal *nals; int n, i;
    H265ENC  *enc = calloc(1, sizeof(H265ENC) + (w * h * 3 / 2) * YUV_BUF_NUM);
    if (!enc) return NULL;

    strncpy(enc->name, "h265enc", sizeof(enc->name));
    enc->uninit     = uninit;
    enc->write      = write;
    enc->read       = read;
    enc->start      = start;
    enc->reset      = reset;
    enc->obuflock   = obuflock;
    enc->obufunlock = obufunlock;
    enc->reconfig   = reconfig;

    // init mutex & cond
    pthread_mutex_init(&enc->imutex, NULL);
    pthread_cond_init (&enc->icond , NULL);
    pthread_mutex_init(&enc->omutex, NULL);
    pthread_cond_init (&enc->ocond , NULL);

    x265_param_default_preset(&enc->param, "ultrafast", "zerolatency");
    x265_param_apply_profile (&enc->param, "main");
    enc->param.bRepeatHeaders   = 1;
    enc->param.internalCsp      = X265_CSP_I420;
    enc->param.sourceWidth      = w;
    enc->param.sourceHeight     = h;
    enc->param.fpsNum           = frate;
    enc->param.fpsDenom         = 1;
    enc->param.keyframeMin      = frate * 2;
    enc->param.keyframeMax      = frate * 5;
    enc->param.rc.bitrate       = bitrate / 1000;
#if 0 // X265_RC_CQP
    enc->param.rc.rateControlMode   = X265_RC_CQP;
    enc->param.rc.qp                = 35;
    enc->param.rc.qpMin             = 25;
    enc->param.rc.qpMax             = 50;
#endif
#if 0 // X265_RC_CRF
    enc->param.rc.rateControlMode   = X265_RC_CRF;
    enc->param.rc.rfConstant        = 25;
    enc->param.rc.rfConstantMax     = 50;
#endif
#if 1 // X265_RC_ABR
    enc->param.rc.rateControlMode   = X265_RC_ABR;
    enc->param.rc.vbvMaxBitrate     = 2 * bitrate / 1000;
    enc->param.rc.vbvBufferSize     = 2 * bitrate / 1000;
#endif

    enc->ow   = w;
    enc->oh   = h;
    enc->x265 = x265_encoder_open(&enc->param);
    for (i=0; i<YUV_BUF_NUM; i++) {
        enc->ibuff[i] = (uint8_t*)enc + sizeof(H265ENC) + i * (w * h * 3 / 2);
    }

    x265_encoder_headers(enc->x265, &nals, &n);
    for (i=0; i<n; i++) {
        switch (nals[i].type) {
        case NAL_UNIT_SPS:
            enc->spsinfo[0] = MIN(nals[i].sizeBytes, 255);
            memcpy(enc->spsinfo + 1, nals[i].payload, enc->spsinfo[0]);
            break;
        case NAL_UNIT_PPS:
            enc->ppsinfo[0] = MIN(nals[i].sizeBytes, 255);
            memcpy(enc->ppsinfo + 1, nals[i].payload, enc->ppsinfo[0]);
            break;
        case NAL_UNIT_VPS:
            enc->vpsinfo[0] = MIN(nals[i].sizeBytes, 255);
            memcpy(enc->vpsinfo + 1, nals[i].payload, enc->vpsinfo[0]);
            break;
        }
    }

    pthread_create(&enc->thread, NULL, venc_encode_thread_proc, enc);
    return (CODEC*)enc;
}


