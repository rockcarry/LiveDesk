#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <winsock2.h>
#include "ringbuf.h"
#include "ikcp.h"
#include "avkcpc.h"

#define AVKCP_CONV (('A' << 0) | ('V' << 8) | ('K' << 16) | ('C' << 24))

#ifdef WIN32
#include <winsock2.h>
#define usleep(t) Sleep((t) / 1000)
#define get_tick_count GetTickCount
#endif

typedef struct {
    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    uint32_t  status;
    pthread_t pthread;

    ikcpcb   *ikcp;
    uint32_t  tick_kcp_update;
    struct    sockaddr_in server_addr;
    struct    sockaddr_in client_addr;
    SOCKET    client_fd;
    uint8_t   buff[2 * 1024 * 1024];
    int       head;
    int       tail;
    int       size;
} AVKCPC;

static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    AVKCPC *avkcpc = (AVKCPC*)user;
    return sendto(avkcpc->client_fd, buf, len, 0, (struct sockaddr*)&avkcpc->server_addr, sizeof(avkcpc->server_addr));
}

static void avkcpc_ikcp_update(AVKCPC *avkcpc)
{
    uint32_t tickcur = get_tick_count();
    if (tickcur >= avkcpc->tick_kcp_update) {
        ikcp_update(avkcpc->ikcp, tickcur);
        avkcpc->tick_kcp_update = ikcp_check(avkcpc->ikcp, get_tick_count());
    }
}

static void* avkcpc_thread_proc(void *argv)
{
    AVKCPC  *avkcpc = (AVKCPC*)argv;
    struct   sockaddr_in fromaddr;
    uint8_t  buffer[1500];
    uint32_t tickcur = 0, tickheartbeat = 0;
    int      addrlen = sizeof(fromaddr), ret;
    unsigned long opt;

#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed !\n");
        return NULL;
    }
#endif

    avkcpc->client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (avkcpc->client_fd < 0) {
        printf("failed to open socket !\n");
        goto _exit;
    }
    opt = 1;        ioctlsocket(avkcpc->client_fd, FIONBIO, &opt); // setup non-block io mode
    opt = 256*1024; setsockopt(avkcpc->client_fd, SOL_SOCKET, SO_RCVBUF, (char*)&opt, sizeof(int));

    avkcpc->ikcp = ikcp_create(AVKCP_CONV, avkcpc);
    if (!avkcpc->ikcp) {
        printf("failed to create ikcp !\n");
        goto _exit;
    }

    ikcp_setoutput(avkcpc->ikcp, udp_output);
    ikcp_nodelay(avkcpc->ikcp, 2, 10, 2, 0);
    ikcp_wndsize(avkcpc->ikcp, 128, 128);
    avkcpc->ikcp->interval = 1;
    avkcpc->ikcp->rx_minrto = 5;
    avkcpc->ikcp->fastresend = 1;
    avkcpc->ikcp->stream = 1;

    while (!(avkcpc->status & TS_EXIT)) {
        if (!(avkcpc->status & TS_START)) { usleep(100*1000); continue; }

        if (get_tick_count() >= tickheartbeat + 2000) {
            tickheartbeat = get_tick_count();
            ikcp_send(avkcpc->ikcp, "hb", 3);
        }

        do {
            ret = recvfrom(avkcpc->client_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&fromaddr, &addrlen);
            if (ret > 0) ikcp_input(avkcpc->ikcp, buffer, ret);
        } while (ret > 0);

        while (1) {
            if ((ret = ikcp_recv(avkcpc->ikcp, buffer, sizeof(buffer))) <= 0) break;
            if (ret <= (int)sizeof(avkcpc->buff) - avkcpc->size) {
                avkcpc->tail = ringbuf_write(avkcpc->buff, sizeof(avkcpc->buff), avkcpc->tail, buffer, ret);
                avkcpc->size+= ret;
            } else {
                printf("drop data ! %d\n", ret);
            }
            if (avkcpc->size > 4) {
                uint32_t typelen, head;
                head = ringbuf_read(avkcpc->buff, sizeof(avkcpc->buff), avkcpc->head, (uint8_t*)&typelen, sizeof(typelen));
                if ((int)((typelen >> 8) + sizeof(typelen)) <= avkcpc->size) {
                    avkcpc->head = ringbuf_read(avkcpc->buff, sizeof(avkcpc->buff), head, NULL, (typelen >> 8));
                    avkcpc->size-= sizeof(typelen) + (typelen >> 8);
//                  printf("get %c frame, size: %d\n", (typelen & 0xFF), (typelen >> 8));
                }
            }
        };

        avkcpc_ikcp_update(avkcpc);
        usleep(1*1000);
    }

_exit:
    if (avkcpc->client_fd >= 0) closesocket(avkcpc->client_fd);
    if (avkcpc->ikcp) ikcp_release(avkcpc->ikcp);
#ifdef WIN32
    WSACleanup();
#endif
    return NULL;
}

void* avkcpc_init(char *ip, int port)
{
    AVKCPC *avkcpc = calloc(1, sizeof(AVKCPC));
    if (!avkcpc) {
        printf("failed to allocate memory for avkcpc !\n");
        return NULL;
    }

    avkcpc->server_addr.sin_family      = AF_INET;
    avkcpc->server_addr.sin_port        = htons(port);
    avkcpc->server_addr.sin_addr.s_addr = inet_addr(ip);

    // create client thread
    pthread_create(&avkcpc->pthread, NULL, avkcpc_thread_proc, avkcpc);
    avkcpc_start(avkcpc, 1);
    return avkcpc;
}

void avkcpc_exit(void *ctxt)
{
    AVKCPC *avkcpc = ctxt;
    if (!ctxt) return;
    avkcpc->status |= TS_EXIT;
    pthread_join(avkcpc->pthread, NULL);
    free(ctxt);
}

void avkcpc_start(void *ctxt, int start)
{
    AVKCPC *avkcpc = ctxt;
    if (!ctxt) return;
    if (start) {
        avkcpc->status |= TS_START;
    } else {
        avkcpc->status &=~TS_START;
    }
}
