#ifndef __CODEC_H__
#define __CODEC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CODEC_RESET_CLEAR_INBUF  = (1 << 0),
    CODEC_RESET_CLEAR_OUTBUF = (1 << 1),
    CODEC_RESET_REQUEST_IDR  = (1 << 2),
};

typedef void (*PFN_CODEC_CALLBACK)(void *ctxt, void *buf[8], int len[8]);

#define CODEC_INTERFACE_FUNCS \
    char    name[8]; \
    uint8_t info[8]; \
    void (*uninit)(void *ctxt); \
    void (*write )(void *ctxt, void *buf[8], int len[8]); \
    int  (*read  )(void *ctxt, void *buf, int len, int *fsize, int timeout); \
    void (*start )(void *ctxt, int start); \
    void (*reset )(void *ctxt, int type );

typedef struct {
    CODEC_INTERFACE_FUNCS
} CODEC;

CODEC* alawenc_init(void);
CODEC* aacenc_init (int channels, int samplerate, int bitrate);
CODEC* h264enc_init(int frate, int w, int h, int bitrate);

#define codec_uninit(codec)                   (codec)->uninit(codec)
#define codec_write(codec, buf, len)          (codec)->write(codec, buf, len)
#define codec_read(codec, buf, len, fsize, t) (codec)->read(codec, buf, len, fsize, t)
#define codec_start(codec, s)                 (codec)->start(codec, s)
#define codec_reset(codec, t)                 (codec)->reset(codec, t)

#ifdef __cplusplus
}
#endif

#endif
