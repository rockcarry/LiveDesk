#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "stdafx.h"
#include "ringbuf.h"
#include "videv.h"
#include "vienc.h"
#include "x264.h"
#include "log.h"

#define ENC_BUF_SIZE  (512 * 1024)
typedef struct {
    void    *videv;
    x264_t  *x264;
    int      width;
    int      height;

    uint8_t  buffer[ENC_BUF_SIZE];
    int      head;
    int      tail;
    int      size;

    #define TS_EXIT        (1 << 0)
    #define TS_START       (1 << 1)
    #define TS_REQUEST_IDR (1 << 2)
    int      status;

    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    pthread_t       thread;
} VIENC;

static void* vienc_encode_thread_proc(void *param)
{
    VIENC   *enc = (VIENC*)param;
    uint8_t *yuv = NULL;
    x264_picture_t pic_in, pic_out;
    x264_nal_t *nals;
    int     len, num, i;

    x264_picture_init(&pic_in );
    x264_picture_init(&pic_out);
    pic_in.img.i_csp   = X264_CSP_I420;
    pic_in.img.i_plane = 3;
    pic_in.img.i_stride[0] = enc->width;
    pic_in.img.i_stride[1] = enc->width / 2;
    pic_in.img.i_stride[2] = enc->width / 2;

    while (!(enc->status & TS_EXIT)) {
        if (!(enc->status & TS_START)) {
            usleep(100*1000); continue;
        }

        yuv = videv_lock(enc->videv, 1);
        if (yuv) {
            pic_in.img.plane[0] = yuv;
            pic_in.img.plane[1] = yuv + enc->width * enc->height * 4 / 4;
            pic_in.img.plane[2] = yuv + enc->width * enc->height * 5 / 4;
            pic_in.i_type       = 0;
            if (enc->status & TS_REQUEST_IDR) {
                enc->status  &= ~TS_REQUEST_IDR;
                pic_in.i_type =  X264_TYPE_IDR;
            }
            len = x264_encoder_encode(enc->x264, &nals, &num, &pic_in, &pic_out);
        } else {
            len = 0;
        }
        videv_unlock(enc->videv);
        if (len <= 0) continue;

        pthread_mutex_lock(&enc->mutex);
        if (len <= (int)sizeof(enc->buffer) - enc->size) {
            for (i=0; i<num; i++) {
                enc->tail = ringbuf_write(enc->buffer, sizeof(enc->buffer), enc->tail, nals[i].p_payload, nals[i].i_payload);
            }
            enc->size += len;
            pthread_cond_signal(&enc->cond);
        } else {
            log_printf("vienc drop data !\n");
        }
        pthread_mutex_unlock(&enc->mutex);
    }
    return NULL;
}

void* vienc_init(void *videv, int frate, int w, int h, int bitrate)
{
    x264_param_t param;
    VIENC *enc = calloc(1, sizeof(VIENC));
    if (!enc) return NULL;

    // init mutex & cond
    pthread_mutex_init(&enc->mutex, NULL);
    pthread_cond_init (&enc->cond , NULL);

    x264_param_default_preset(&param, "ultrafast", "zerolatency");
    x264_param_apply_profile (&param, "baseline");
    param.i_csp            = X264_CSP_I420;
    param.i_width          = w;
    param.i_height         = h;
    param.i_fps_num        = frate;
    param.i_fps_den        = 1;
    param.i_threads        = X264_THREADS_AUTO;
    param.i_keyint_min     = frate * 2;
    param.i_keyint_max     = frate * 5;
    param.rc.i_bitrate     = bitrate;
    param.rc.i_rc_method   = X264_RC_ABR;
    param.i_timebase_num   = 1;
    param.i_timebase_den   = 1000;
    enc->width = w;
    enc->height= h;
    enc->videv = videv;
    enc->x264  = x264_encoder_open(&param);

    pthread_create(&enc->thread, NULL, vienc_encode_thread_proc, enc);
    return enc;
}

void vienc_free(void *ctxt)
{
    VIENC *enc = (VIENC*)ctxt;
    if (!ctxt) return;

    enc->status |= TS_EXIT;
    pthread_join(enc->thread, NULL);

    x264_encoder_close(enc->x264);
    pthread_mutex_destroy(&enc->mutex);
    pthread_cond_destroy (&enc->cond );
    free(enc);
}

void vienc_start(void *ctxt, int start)
{
    VIENC *enc = (VIENC*)ctxt;
    if (!enc) return;
    if (start) enc->status |= TS_START;
    else {
        pthread_mutex_lock(&enc->mutex);
        enc->status &= ~TS_START;
        pthread_cond_signal(&enc->cond);
        pthread_mutex_unlock(&enc->mutex);
    }
}

int vienc_read(void *ctxt, void *buf, int size, int wait)
{
    VIENC *enc = (VIENC*)ctxt;
    int    ret   = 0;
    if (!ctxt) return -1;

    if (buf == NULL) {
        videv_start(enc->videv, size);
        vienc_start(enc, size);
        pthread_mutex_lock(&enc->mutex);
        switch (size) {
        case VIENC_CMD_STOP: break;
        case VIENC_CMD_RESET_BUFFER: enc->head = enc->tail = enc->size = 0; break;
        case VIENC_CMD_REQUEST_IDR : enc->status |= TS_REQUEST_IDR; break;
        }
        pthread_mutex_unlock(&enc->mutex);
        return 0;
    }

    pthread_mutex_lock(&enc->mutex);
    if (wait) {
        while (enc->size <= 0 && (enc->status & TS_START)) pthread_cond_wait(&enc->cond, &enc->mutex);
    }
    if (enc->size > 0) {
        ret = size < enc->size ? size : enc->size;
        enc->head = ringbuf_read(enc->buffer, sizeof(enc->buffer), enc->head,  buf , ret);
        enc->size-= ret;
    }
    pthread_mutex_unlock(&enc->mutex);
    return ret;
}
