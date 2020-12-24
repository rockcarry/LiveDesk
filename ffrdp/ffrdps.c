#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "adev.h"
#include "vdev.h"
#include "codec.h"
#include "ffrdp.h"
#include "ffrdps.h"

#pragma warning(disable:4996) // disable warnings
#define usleep(t) Sleep((t) / 1000)
#define get_tick_count GetTickCount
#define snprintf       _snprintf
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct {
    #define TS_EXIT             (1 << 0)
    #define TS_START            (1 << 1)
    #define TS_CLIENT_CONNECTED (1 << 2)
    #define TS_KEYFRAME_DROPPED (1 << 3)
    #define TS_ADAPTIVE_BITRATE (1 << 4)
    uint32_t  status;
    pthread_t pthread;

    int channels, samprate, width, height, frate;

    void     *ffrdp;
    void     *adev;
    void     *vdev;
    CODEC    *aenc;
    CODEC    *venc;
    int       port;

    char      avinfostr[256];
    uint8_t   buff[2 * 1024 * 1024];

    #define MAX_BITRATE_LIST_SIZE  100
    int       bitrate_list_buf[MAX_BITRATE_LIST_SIZE];
    int       bitrate_list_size;
    int       bitrate_cur_idx;
    uint32_t  cnt_qos_increase;
    uint32_t  tick_qos_check;
} FFRDPS;

static int ffrdp_send_packet(FFRDPS *ffrdps, char type, uint8_t *buf, int len)
{
    int ret;
    *(int32_t*)buf = (type << 0) | (len << 8);
    ret = ffrdp_send(ffrdps->ffrdp, buf, len + sizeof(int32_t));
    if (ret != len + sizeof(int32_t)) {
        printf("ffrdp_send_packet send packet failed ! %d %d\n", ret, len + sizeof(int32_t));
        return -1;
    } else return 0;
}

static void buf2hexstr(char *str, int len, uint8_t *buf, int size)
{
    char tmp[3];
    int  i;
    for (i=0; i<size; i++) {
        snprintf(tmp, sizeof(tmp), "%02x", buf[i]);
        strncat(str, tmp, len);
    }
}

static void* ffrdps_thread_proc(void *argv)
{
    FFRDPS  *ffrdps = (FFRDPS*)argv;
    uint8_t  buffer[256];
    int      ret;
    while (!(ffrdps->status & TS_EXIT)) {
        if (!(ffrdps->status & TS_START)) { usleep(100*1000); continue; }

        if (!ffrdps->ffrdp) {
            ffrdps->ffrdp = ffrdp_init("0.0.0.0", ffrdps->port, 1, 1500, 0);
            if (!ffrdps->ffrdp) { usleep(100*1000); continue; }
        }

        ret = ffrdp_recv(ffrdps->ffrdp, (char*)buffer, sizeof(buffer));
        if (ret > 0) {
            if ((ffrdps->status & TS_CLIENT_CONNECTED) == 0) {
                uint8_t spsbuf[256], ppsbuf[256];
                char    spsstr[256] = "", ppsstr[256] = "";
                int     spslen, ppslen;
                codec_reset(ffrdps->aenc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
                codec_reset(ffrdps->venc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
                codec_start(ffrdps->aenc, 1);
                codec_start(ffrdps->venc, 1);
                adev_start (ffrdps->adev, 1);
                vdev_start (ffrdps->vdev, 1);
                spslen = h264enc_getinfo(ffrdps->venc, "sps", spsbuf, sizeof(spsbuf));
                ppslen = h264enc_getinfo(ffrdps->venc, "pps", ppsbuf, sizeof(ppsbuf));
                buf2hexstr(spsstr, sizeof(spsstr), spsbuf, spslen);
                buf2hexstr(ppsstr, sizeof(ppsstr), ppsbuf, ppslen);
                snprintf(ffrdps->avinfostr+sizeof(uint32_t), sizeof(ffrdps->avinfostr)-sizeof(uint32_t),
                    "aenc=%s,channels=%d,samprate=%d;venc=%s,width=%d,height=%d,frate=%d,sps=%s,pps=%s;",
                    ffrdps->aenc->name, ffrdps->channels, ffrdps->samprate, ffrdps->venc->name, ffrdps->width, ffrdps->height, ffrdps->frate, spsstr, ppsstr);
                ret = ffrdp_send_packet(ffrdps, 'I', ffrdps->avinfostr, (int)strlen(ffrdps->avinfostr+sizeof(uint32_t)) + 1);
                if (ret == 0) {
                    ffrdps->status |= TS_CLIENT_CONNECTED;
                    printf("client connected !\n");
                }
            }
        }

        if ((ffrdps->status & TS_CLIENT_CONNECTED)) {
            int readsize, framesize, keyframe;
            readsize = codec_read(ffrdps->aenc, ffrdps->buff + sizeof(int32_t), sizeof(ffrdps->buff) - sizeof(int32_t), &framesize, &keyframe, 0);
            if (readsize > 0 && readsize == framesize && readsize <= 0xFFFFFF) {
                ret = ffrdp_send_packet(ffrdps, 'A', ffrdps->buff, framesize);
            }
            readsize = codec_read(ffrdps->venc, ffrdps->buff + sizeof(int32_t), sizeof(ffrdps->buff) - sizeof(int32_t), &framesize, &keyframe, 0);
            if (readsize > 0 && readsize == framesize && readsize <= 0xFFFFFF) {
                if ((ffrdps->status & TS_KEYFRAME_DROPPED) && !keyframe) {
                    printf("ffrdp key frame has dropped, and current frame is non-key frame, so drop it !\n");
                } else {
                    ret = ffrdp_send_packet(ffrdps, 'V', ffrdps->buff, framesize);
                    if (ret == 0 && keyframe) ffrdps->status &=~TS_KEYFRAME_DROPPED;
                    if (ret != 0 && keyframe) ffrdps->status |= TS_KEYFRAME_DROPPED;
                }
            }
        }

        ffrdp_update(ffrdps->ffrdp);
        if ((ffrdps->status & TS_CLIENT_CONNECTED) && ffrdp_isdead(ffrdps->ffrdp)) {
            printf("client lost !\n");
            codec_start(ffrdps->aenc, 0);
            codec_start(ffrdps->venc, 0);
            adev_start (ffrdps->adev, 0);
            vdev_start (ffrdps->vdev, 0);
            ffrdp_free(ffrdps->ffrdp); ffrdps->ffrdp = NULL;
            ffrdps->status &= ~TS_CLIENT_CONNECTED;
        }

        if ((ffrdps->status & TS_CLIENT_CONNECTED) && (ffrdps->status & TS_ADAPTIVE_BITRATE) && (int32_t)get_tick_count() - (int32_t)ffrdps->tick_qos_check > 1000) {
            int last_idx = ffrdps->bitrate_cur_idx, qos = ffrdp_qos(ffrdps->ffrdp);
            if (qos < 0) {
                ffrdps->bitrate_cur_idx = MAX(ffrdps->bitrate_cur_idx - 1, 0);
                ffrdps->cnt_qos_increase= 0;
            } else if (qos > 0) {
                if (ffrdps->cnt_qos_increase == 3) {
                    ffrdps->bitrate_cur_idx  = MIN(ffrdps->bitrate_cur_idx + 1, ffrdps->bitrate_list_size - 1);
                    ffrdps->cnt_qos_increase = 0;
                } else ffrdps->cnt_qos_increase++;
            }
            if (ffrdps->bitrate_cur_idx != last_idx) {
                ffrdps_reconfig_bitrate(ffrdps, ffrdps->bitrate_list_buf[ffrdps->bitrate_cur_idx]);
            }
            ffrdps->tick_qos_check = get_tick_count();
        }
    }

    ffrdp_free(ffrdps->ffrdp);
    return NULL;
}

void* ffrdps_init(int port, int channels, int samprate, int width, int height, int frate, void *adev, void *vdev, CODEC *aenc, CODEC *venc)
{
    FFRDPS *ffrdps = calloc(1, sizeof(FFRDPS));
    if (!ffrdps) {
        printf("failed to allocate memory for ffrdps !\n");
        return NULL;
    }

    ffrdps->adev     = adev;
    ffrdps->vdev     = vdev;
    ffrdps->aenc     = aenc;
    ffrdps->venc     = venc;
    ffrdps->port     = port;
    ffrdps->channels = channels;
    ffrdps->samprate = samprate;
    ffrdps->width    = width;
    ffrdps->height   = height;
    ffrdps->frate    = frate;

    // create server thread
    pthread_create(&ffrdps->pthread, NULL, ffrdps_thread_proc, ffrdps);
    ffrdps_start(ffrdps, 1);
    return ffrdps;
}

void ffrdps_exit(void *ctxt)
{
    FFRDPS *ffrdps = ctxt;
    if (!ctxt) return;
    adev_start (ffrdps->adev, 0);
    vdev_start (ffrdps->vdev, 0);
    codec_start(ffrdps->aenc, 0);
    codec_start(ffrdps->venc, 0);
    ffrdps->status |= TS_EXIT;
    pthread_join(ffrdps->pthread, NULL);
    free(ctxt);
}

void ffrdps_start(void *ctxt, int start)
{
    FFRDPS *ffrdps = ctxt;
    if (!ctxt) return;
    if (start) {
        ffrdps->status |= TS_START;
    } else {
        ffrdps->status &=~TS_START;
    }
}

void ffrdps_dump(void *ctxt, int clearhistory)
{
    FFRDPS *ffrdps = ctxt;
    if (!ctxt) return;
    ffrdp_dump(ffrdps->ffrdp, clearhistory);
}

void ffrdps_reconfig_bitrate(void *ctxt, int bitrate)
{
    FFRDPS *ffrdps = ctxt;
    if (!ctxt) return;
    h264enc_reconfig(ffrdps->venc, bitrate);
}

void ffrdps_adaptive_bitrate_setup(void *ctxt, int *blist, int n)
{
    FFRDPS *ffrdps = ctxt;
    if (!ctxt) return;
    ffrdps->bitrate_list_size = MIN(n, MAX_BITRATE_LIST_SIZE);
    memcpy(ffrdps->bitrate_list_buf, blist, ffrdps->bitrate_list_size * sizeof(int));
}

void ffrdps_adaptive_bitrate_enable(void *ctxt, int en)
{
    FFRDPS *ffrdps = ctxt;
    if (!ctxt) return;
    ffrdps->bitrate_cur_idx= ffrdps->bitrate_list_size / 2;
    h264enc_reconfig(ffrdps->venc, ffrdps->bitrate_list_buf[ffrdps->bitrate_cur_idx]);
    if (en && ffrdps->bitrate_list_size > 0) {
        ffrdps->tick_qos_check = get_tick_count();
        ffrdps->status |= TS_ADAPTIVE_BITRATE;
    } else {
        ffrdps->status &=~TS_ADAPTIVE_BITRATE;
    }
}
