#ifndef __AVKCPC_H__
#define __AVKCPC_H__

typedef void (*PFN_AVKCPC_CB)(void *ctxt, int type, char *buf, int len);

void* avkcpc_init (char *ip, int port, PFN_AVKCPC_CB callback, void *cbctxt);
void  avkcpc_exit (void *ctxt);
void  avkcpc_start(void *ctxt, int start);

#endif
