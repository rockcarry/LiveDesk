#ifndef __LIBRTMP_H__
#define __LIBRTMP_H__

#include <stdint.h>

void* rtmp_push_init(char *url, uint8_t *aac_dec_spec);
void  rtmp_push_exit(void *ctxt);
int   rtmp_push_conn(void *ctxt);
void  rtmp_push_h264(void *ctxt, uint8_t *data, int len);
void  rtmp_push_alaw(void *ctxt, uint8_t *data, int len);
void  rtmp_push_aac (void *ctxt, uint8_t *data, int len);
void  rtmp_push_url (void *ctxt, char *url);

#endif








