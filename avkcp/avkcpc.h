#ifndef __AVKCPC_H__
#define __AVKCPC_H__

void* avkcpc_init (char *ip, int port);
void  avkcpc_exit (void *ctxt);
void  avkcpc_start(void *ctxt, int start);

#endif
