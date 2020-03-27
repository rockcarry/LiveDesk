#ifndef __RTMPPUSH_H__
#define __RTMPPUSH_H__

#include <stdint.h>

void* rtmp_push_init(char *url, uint8_t *aac_dec_spec);
void  rtmp_push_exit(void *ctxt);
void  rtmp_push_h264(void *ctxt, uint8_t *data, int len, uint32_t pts);
void  rtmp_push_alaw(void *ctxt, uint8_t *data, int len, uint32_t pts);
void  rtmp_push_aac (void *ctxt, uint8_t *data, int len, uint32_t pts);
void  rtmp_push_url (void *ctxt, char *url);

#endif








