#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "avimuxer.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#define AVIF_HASINDEX      (1 << 4)
#define AVIF_ISINTERLEAVED (1 << 8)
#define AVIIF_KEYFRAME     (1 << 4)

#define AVI_AUDIO_FRAME    (0 << 31)
#define AVI_VIDEO_FRAME    (1 << 31)
#define AVI_KEY_FRAME      (1 << 30)

#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type*)0)->member)
#endif

#pragma pack(1)
typedef struct {
    uint32_t microsec_per_frame;
    uint32_t maxbytes_per_Sec;
    uint32_t padding_granularity;
    uint32_t flags;
    uint32_t total_frames;
    uint32_t initial_frames;
    uint32_t number_streams;
    uint32_t suggested_bufsize;
    uint32_t width;
    uint32_t height;
    uint32_t reserved[4];
} AVI_HEADER;

typedef struct  {
    char     fcc_type [4];
    char     fcc_codec[4];
    uint32_t flags;
    uint16_t priority;
    uint16_t language;
    uint32_t initial_frames;
    uint32_t scale;
    uint32_t rate;
    uint32_t start;
    uint32_t length;
    uint32_t suggested_bufsize;
    uint32_t quality;
    uint32_t sample_size;
    int32_t  vrect_left;
    int32_t  vrect_top;
    int32_t  vrect_right;
    int32_t  vrect_bottom;
} STREAM_HEADER;

typedef struct {
    uint16_t format_tag;
    uint16_t channels;
    uint32_t sample_per_sec;
    uint32_t avgbyte_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint16_t size;
} WAVE_FORMAT;

typedef struct {
    uint32_t  size;
    uint32_t  width;
    uint32_t  height;
    uint16_t  planes;
    uint16_t  bitcount;
    uint32_t  compression;
    uint32_t  image_size;
    uint32_t  xpels_per_meter;
    uint32_t  ypels_per_meter;
    uint32_t  color_used;
    uint32_t  color_important;
} BITMAP_FORMAT;

typedef struct {
    uint32_t     *framesize_lst;
    uint32_t      framesize_fix;
    uint32_t      framesize_idx;
    uint32_t      framesize_max;
    FILE         *fp;

    char          riff[4];
    uint32_t      riff_size;
    char          type_avi[4];

    char          hlist[4];
    uint32_t      hlist_size;
    char          type_hdrl[4];

    char          avih[4];
    uint32_t      avih_size;
    AVI_HEADER    avi_header;

    char          slist1[4];
    uint32_t      slist1_size;
    char          type_str1[4];

    char          strhdr1[4];
    uint32_t      strhdr1_size;
    STREAM_HEADER strhdr_audio;

    char          strfmt1[4];
    uint32_t      strfmt1_size;
    WAVE_FORMAT   strfmt_audio;

    char          slist2[4];
    uint32_t      slist2_size;
    char          type_str2[4];

    char          strhdr2[4];
    uint32_t      strhdr2_size;
    STREAM_HEADER strhdr_video;

    char          strfmt2[4];
    uint32_t      strfmt2_size;
    BITMAP_FORMAT strfmt_video;

    char          mlist[4];
    uint32_t      mlist_size;
    char          type_movi[4];
} AVI_FILE;
#pragma pack()

void* avimuxer_init(char *file, int duration, int w, int h, int fps, int sampnum, int h265)
{
    int samprate = 8000, channels = 1, sampbits = 8;
    AVI_FILE *avi = calloc(1, sizeof(AVI_FILE));
    if (!avi) goto failed;
    avi->fp = fopen(file, "wb");
    if (!avi->fp) goto failed;

    memcpy(avi->avih, "avih", 4);
    avi->avih_size                      = sizeof(AVI_HEADER);
    avi->avi_header.microsec_per_frame  = 1000000 / fps;
    avi->avi_header.maxbytes_per_Sec    = w * h * 3;
    avi->avi_header.flags               = AVIF_ISINTERLEAVED|AVIF_HASINDEX;
    avi->avi_header.number_streams      = 2;
    avi->avi_header.width               = w;
    avi->avi_header.height              = h;
    avi->avi_header.suggested_bufsize   = w * h * 3;

    memcpy(avi->strhdr1, "strh", 4);
    memcpy(avi->strhdr_audio.fcc_type , "auds", 4);
    memcpy(avi->strhdr_audio.fcc_codec, "G711", 4);
    avi->strhdr1_size                   = sizeof(STREAM_HEADER);
    avi->strhdr_audio.scale             = 1;
    avi->strhdr_audio.rate              = samprate;
    avi->strhdr_audio.suggested_bufsize = samprate * channels * sampbits / 8;
    avi->strhdr_audio.sample_size       = channels * sampbits / 8;

    memcpy(avi->strfmt1, "strf", 4);
    avi->strfmt1_size                   = sizeof(WAVE_FORMAT);
    avi->strfmt_audio.format_tag        = 6;
    avi->strfmt_audio.channels          = channels;
    avi->strfmt_audio.sample_per_sec    = samprate;
    avi->strfmt_audio.avgbyte_per_sec   = samprate * channels * sampbits / 8;
    avi->strfmt_audio.block_align       = channels * sampbits / 8;
    avi->strfmt_audio.bits_per_sample   = sampbits;

    memcpy(avi->slist1   , "LIST", 4);
    memcpy(avi->type_str1, "strl", 4);
    avi->slist1_size = 4 + 4 + 4 + avi->strhdr1_size + 4 + 4 + avi->strfmt1_size;

    memcpy(avi->strhdr2, "strh", 4);
    memcpy(avi->strhdr_video.fcc_type, "vids", 4);
    memcpy(avi->strhdr_video.fcc_codec, h265 ? "HEV1" : "H264", 4);
    avi->strhdr2_size                   = sizeof(STREAM_HEADER);
    avi->strhdr_video.scale             = 1;
    avi->strhdr_video.rate              = fps;
    avi->strhdr_video.suggested_bufsize = w * h * 3;

    memcpy(avi->strfmt2, "strf", 4);
    avi->strfmt2_size                   = sizeof(BITMAP_FORMAT);
    avi->strfmt_video.size              = 40;
    avi->strfmt_video.width             = w;
    avi->strfmt_video.height            = h;
    avi->strfmt_video.planes            = 1;
    avi->strfmt_video.bitcount          = 24;
    avi->strfmt_video.compression       = h265 ? (('H' << 0) | ('E' << 8) | ('V' << 16) | ('1' << 24)) : (('H' << 0) | ('2' << 8) | ('6' << 16) | ('4' << 24));
    avi->strfmt_video.image_size        = w * h * 3;

    memcpy(avi->slist2   , "LIST", 4);
    memcpy(avi->type_str2, "strl", 4);
    avi->slist2_size = 4 + 4 + 4 + avi->strhdr2_size + 4 + 4 + avi->strfmt2_size;

    memcpy(avi->riff     , "RIFF", 4);
    memcpy(avi->type_avi , "AVI ", 4);
    memcpy(avi->hlist    , "LIST", 4);
    memcpy(avi->type_hdrl, "hdrl", 4);
    memcpy(avi->mlist    , "LIST", 4);
    memcpy(avi->type_movi, "movi", 4);
    avi->hlist_size = 4 + 8 + avi->avih_size + 8 + avi->slist1_size + 8 + avi->slist2_size;

    if (duration == 0) duration = 10 * 60 * 1000; // default duration is 10min
    avi->framesize_max = duration * fps / 1000 + fps / 2 + duration * samprate / 1000 / sampnum + samprate / sampnum / 2;
    avi->framesize_lst = malloc(avi->framesize_max * sizeof(uint32_t));
    fwrite(&avi->riff, sizeof(AVI_FILE) - offsetof(AVI_FILE, riff), 1, avi->fp);
    return avi;

failed:
    if (avi) free(avi);
    return NULL;
}

static void avimuxer_fix_data(AVI_FILE *avi, int writeidx)
{
    uint32_t data, movisize;

    data = ftell(avi->fp) - 8;
    fseek(avi->fp, offsetof(AVI_FILE, riff_size) - offsetof(AVI_FILE, riff), SEEK_SET);
    fwrite(&data, 4, 1, avi->fp);

    avi->avi_header.total_frames = avi->strhdr_video.length;
    data = avi->avi_header.total_frames;
    fseek(avi->fp, offsetof(AVI_FILE, avi_header.total_frames) - offsetof(AVI_FILE, riff), SEEK_SET);
    fwrite(&data, 4, 1, avi->fp);

    data = avi->strhdr_audio.length;
    fseek(avi->fp, offsetof(AVI_FILE, strhdr_audio.length) - offsetof(AVI_FILE, riff), SEEK_SET);
    fwrite(&data, 4, 1, avi->fp);

    data = avi->strhdr_video.length;
    fseek(avi->fp, offsetof(AVI_FILE, strhdr_video.length) - offsetof(AVI_FILE, riff), SEEK_SET);
    fwrite(&data, 4, 1, avi->fp);

    movisize = ftell(avi->fp) - (offsetof(AVI_FILE, type_movi) - offsetof(AVI_FILE, riff));
    fseek(avi->fp, offsetof(AVI_FILE, mlist_size) - offsetof(AVI_FILE, riff), SEEK_SET);
    fwrite(&movisize, 4, 1, avi->fp);
    fseek(avi->fp, 0, SEEK_END);

    if (writeidx) {
        uint32_t idx1size, curpos = 4;
        fwrite("idx1", 4, 1, avi->fp);
        idx1size = avi->framesize_idx * sizeof(uint32_t) * 4;
        fwrite(&idx1size , 4, 1, avi->fp);
        while (avi->framesize_fix < avi->framesize_idx) {
            fwrite((avi->framesize_lst[avi->framesize_fix] & AVI_VIDEO_FRAME) ? "01dc" : "00wb", 4, 1, avi->fp);
            data = (avi->framesize_lst[avi->framesize_fix] & AVI_KEY_FRAME  ) ? AVIIF_KEYFRAME : 0;
            fwrite(&data, 4, 1, avi->fp);
            data = curpos;
            fwrite(&data, 4, 1, avi->fp);
            data = avi->framesize_lst ? avi->framesize_lst[avi->framesize_fix] & 0x3fffffff : 0;
            fwrite(&data, 4, 1, avi->fp);
            curpos += data + 8;
            avi->framesize_fix++;
        }
    }
}

void avimuxer_exit(void *ctx)
{
    AVI_FILE *avi = (AVI_FILE*)ctx;
    if (avi) {
        if (avi->fp) {
            avimuxer_fix_data(avi, 1);
            fclose(avi->fp);
        }
        free(avi->framesize_lst);
        free(avi);
    }
}

void avimuxer_audio(void *ctx, unsigned char *buf, int len, int key, unsigned pts)
{
    AVI_FILE *avi = (AVI_FILE*)ctx;
    if (avi && avi->fp) {
        int alignlen = (len & 1) ? len + 1 : len;
        fwrite("00wb"   , 4  , 1, avi->fp);
        fwrite(&alignlen, 4  , 1, avi->fp);
        fwrite( buf     , len, 1, avi->fp);
        if (len & 1) fputc(0, avi->fp);
        if (avi->framesize_lst && avi->framesize_idx < avi->framesize_max) {
            avi->framesize_lst[avi->framesize_idx++] = alignlen | AVI_AUDIO_FRAME;
        }
        avi->strhdr_audio.length += len;
    }
}

void avimuxer_video(void *ctx, unsigned char *buf, int len, int key, unsigned pts)
{
    AVI_FILE *avi = (AVI_FILE*)ctx;
    if (avi && avi->fp) {
        int alignlen = (len & 1) ? len + 1 : len;
        fwrite("01dc"   , 4  , 1, avi->fp);
        fwrite(&alignlen, 4  , 1, avi->fp);
        fwrite( buf     , len, 1, avi->fp);
        if (len & 1) fputc(0, avi->fp);
        if (avi->framesize_lst && avi->framesize_idx < avi->framesize_max) {
            avi->framesize_lst[avi->framesize_idx++] = alignlen | AVI_VIDEO_FRAME | (key << 30);
        }
        avi->strhdr_video.length++;
    }
    if (avi->strhdr_video.length % (avi->strhdr_video.rate * 5) == 0) {
        avimuxer_fix_data(avi, 0);
    }
}
