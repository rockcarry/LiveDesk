#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "adev.h"
#include "vdev.h"
#include "codec.h"
#include "ikcp.h"
#include "avkcps.h"

#define AVKCP_CONV (('A' << 0) | ('V' << 8) | ('K' << 16) | ('C' << 24))

#ifdef WIN32
#include <winsock2.h>
#define usleep(t) Sleep((t) / 1000)
#define get_tick_count GetTickCount
#define snprintf _snprintf
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define SOCKET int
#define closesocket close
static uint32_t get_tick_count()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

typedef struct {
    #define TS_EXIT  (1 << 0)
    #define TS_START (1 << 1)
    uint32_t  status;
    pthread_t pthread;

    int channels, samprate, width, height, frate;

    void     *adev;
    void     *vdev;
    CODEC    *aenc;
    CODEC    *venc;
    ikcpcb   *ikcp;
    uint32_t  tick_kcp_update;
    struct    sockaddr_in server_addr;
    struct    sockaddr_in client_addr;
    int       client_connected;
    SOCKET    server_fd;
    char      avinfostr[256];
    uint8_t   buff[2 * 1024 * 1024];
} AVKCPS;

static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    AVKCPS *avkcps = (AVKCPS*)user;
    return sendto(avkcps->server_fd, buf, len, 0, (struct sockaddr*)&avkcps->client_addr, sizeof(avkcps->client_addr));
}

static void avkcps_ikcp_update(AVKCPS *avkcps)
{
    uint32_t tickcur = get_tick_count();
    if ((int32_t)tickcur - (int32_t)avkcps->tick_kcp_update >= 0) {
        ikcp_update(avkcps->ikcp, tickcur);
        avkcps->tick_kcp_update = ikcp_check(avkcps->ikcp, get_tick_count());
    }
}

static void ikcp_send_packet(AVKCPS *avkcps, char type, uint8_t *buf, int len, uint32_t pts)
{
    int remaining = len + 2 * sizeof(uint32_t), cursend;
    ((uint32_t*)buf)[0] = ('T'  << 0) | (pts << 8);
    ((uint32_t*)buf)[1] = (type << 0) | (len << 8);
    do {
        cursend = remaining < 1024 * 1024 ? remaining : 1024 * 1024;
        ikcp_send(avkcps->ikcp, buf, cursend);
        buf += cursend; remaining -= cursend;
    } while (remaining > 0);
}

static int avkcps_do_connect(AVKCPS *avkcps)
{
    avkcps->ikcp = ikcp_create(AVKCP_CONV, avkcps);
    if (!avkcps->ikcp) return -1;
    ikcp_setoutput(avkcps->ikcp, udp_output);
    ikcp_nodelay(avkcps->ikcp, 1, 10, 2, 1);
    ikcp_wndsize(avkcps->ikcp, 1024, 256);
    avkcps->ikcp->interval   = 10;
    avkcps->ikcp->rx_minrto  = 50;
    avkcps->ikcp->fastresend = 1;
    avkcps->ikcp->stream     = 1;
    return 0;
}

static void avkcps_do_disconnect(AVKCPS *avkcps)
{
    avkcps->client_connected = 0;
    codec_start(avkcps->aenc, 0);
    codec_start(avkcps->venc, 0);
    adev_start (avkcps->adev, 0);
    vdev_start (avkcps->vdev, 0);
    memset(&avkcps->client_addr, 0, sizeof(avkcps->client_addr));
    ikcp_release(avkcps->ikcp);
    avkcps->ikcp = NULL;
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

static void* avkcps_thread_proc(void *argv)
{
    AVKCPS  *avkcps = (AVKCPS*)argv;
    struct   sockaddr_in fromaddr;
    int      addrlen = sizeof(fromaddr), ret;
    uint32_t tickheartbeat = 0;
    uint8_t  buffer[1500];
    unsigned long opt;

#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed !\n");
        return NULL;
    }
#endif

    avkcps->server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (avkcps->server_fd < 0) {
        printf("failed to open socket !\n");
        goto _exit;
    }

    if (bind(avkcps->server_fd, (struct sockaddr*)&avkcps->server_addr, sizeof(avkcps->server_addr)) == -1) {
        printf("failed to bind !\n");
        goto _exit;
    }

    opt = 2*1024; setsockopt(avkcps->server_fd, SOL_SOCKET, SO_RCVBUF, (char*)&opt, sizeof(int));
#ifdef WIN32
    opt = 1; ioctlsocket(avkcps->server_fd, FIONBIO, &opt); // setup non-block io mode
#else
    fcntl(avkcps->server_fd, F_SETFL, fcntl(avkcps->server_fd, F_GETFL, 0) | O_NONBLOCK);  // setup non-block io mode
#endif

    while (!(avkcps->status & TS_EXIT)) {
        if (!(avkcps->status & TS_START)) { usleep(100*1000); continue; }

        if (!avkcps->ikcp) {
            if (avkcps_do_connect(avkcps) != 0) { usleep(100*1000); continue; }
        }

        if (avkcps->client_connected) {
            if (ikcp_waitsnd(avkcps->ikcp) < 2000) {
                int readsize, framesize; uint32_t pts;
                readsize = codec_read(avkcps->aenc, avkcps->buff + 2 * sizeof(int32_t), sizeof(avkcps->buff) - 2 * sizeof(int32_t), &framesize, NULL, &pts, 0);
                if (readsize > 0 && readsize == framesize && readsize <= 0xFFFFFF) {
                    ikcp_send_packet(avkcps, 'A', avkcps->buff, framesize, pts);
                }
                readsize = codec_read(avkcps->venc, avkcps->buff + 2 * sizeof(int32_t), sizeof(avkcps->buff) - 2 * sizeof(int32_t), &framesize, NULL, &pts, 0);
                if (readsize > 0 && readsize == framesize && readsize <= 0xFFFFFF) {
                    ikcp_send_packet(avkcps, 'V', avkcps->buff, framesize, pts);
                }
            } else {
                printf("===ck=== client disconnect, max wait send buffer number reached !\n");
                avkcps_do_disconnect(avkcps); continue;
            }
        }

        while (1) {
            if ((ret = recvfrom(avkcps->server_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&fromaddr, &addrlen)) <= 0) break;
            if (avkcps->client_connected == 0) {
                char spsstr[256] = "", ppsstr[256] = "", vpsstr[256] = "";
                memcpy(&avkcps->client_addr, &fromaddr, sizeof(avkcps->client_addr));
                codec_reset(avkcps->aenc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
                codec_reset(avkcps->venc, CODEC_RESET_CLEAR_INBUF|CODEC_RESET_CLEAR_OUTBUF|CODEC_RESET_REQUEST_IDR);
                codec_start(avkcps->aenc, 1);
                codec_start(avkcps->venc, 1);
                adev_start (avkcps->adev, 1);
                vdev_start (avkcps->vdev, 1);
                buf2hexstr(spsstr, sizeof(spsstr), avkcps->venc->spsinfo + 1, avkcps->venc->spsinfo[0]);
                buf2hexstr(ppsstr, sizeof(ppsstr), avkcps->venc->ppsinfo + 1, avkcps->venc->ppsinfo[0]);
                buf2hexstr(vpsstr, sizeof(vpsstr), avkcps->venc->vpsinfo + 1, avkcps->venc->vpsinfo[0]);
                snprintf(avkcps->avinfostr + 2 * sizeof(uint32_t), sizeof(avkcps->avinfostr) - 2 * sizeof(uint32_t),
                    "aenc=%s,channels=%d,samprate=%d;venc=%s,width=%d,height=%d,frate=%d,sps=%s,pps=%s,vps=%s;",
                    avkcps->aenc->name, avkcps->channels, avkcps->samprate, avkcps->venc->name, avkcps->width, avkcps->height, avkcps->frate, spsstr, ppsstr, vpsstr);
                ikcp_send_packet(avkcps, 'I', avkcps->avinfostr, (int)strlen(avkcps->avinfostr + 2 * sizeof(uint32_t)) + 1, 0);
                tickheartbeat = get_tick_count();
                avkcps->client_connected = 1;
                printf("===ck=== client connected !\n");
            }
            if (memcmp(&avkcps->client_addr, &fromaddr, sizeof(avkcps->client_addr)) == 0) ikcp_input(avkcps->ikcp, buffer, ret);
        }

        if (avkcps->client_connected) {
            ret = ikcp_recv(avkcps->ikcp, buffer, sizeof(buffer));
            if (ret > 0 && strcmp(buffer, "hb") == 0) {
                tickheartbeat = get_tick_count();
                printf("ikcp_waitsnd: %d\n", ikcp_waitsnd(avkcps->ikcp));
            } else {
                if ((int32_t)get_tick_count() - (int32_t)tickheartbeat > 3000) {
                    printf("===ck=== client disconnect, no heart beat !\n");
                    avkcps_do_disconnect(avkcps);
                }
            }
        }

        if (avkcps->ikcp) avkcps_ikcp_update(avkcps);
        usleep(1*1000);
    }

_exit:
    if (avkcps->server_fd >= 0) closesocket(avkcps->server_fd);
    if (avkcps->ikcp) ikcp_release(avkcps->ikcp);
#ifdef WIN32
    WSACleanup();
#endif
    return NULL;
}

void* avkcps_init(int port, int channels, int samprate, int width, int height, int frate, void *adev, void *vdev, CODEC *aenc, CODEC *venc)
{
    AVKCPS *avkcps = calloc(1, sizeof(AVKCPS));
    if (!avkcps) {
        printf("failed to allocate memory for avkcps !\n");
        return NULL;
    }

    avkcps->server_addr.sin_family      = AF_INET;
    avkcps->server_addr.sin_port        = htons(port);
    avkcps->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    avkcps->adev     = adev;
    avkcps->vdev     = vdev;
    avkcps->aenc     = aenc;
    avkcps->venc     = venc;
    avkcps->channels = channels;
    avkcps->samprate = samprate;
    avkcps->width    = width;
    avkcps->height   = height;
    avkcps->frate    = frate;

    // create server thread
    pthread_create(&avkcps->pthread, NULL, avkcps_thread_proc, avkcps);
    avkcps_start(avkcps, 1);
    return avkcps;
}

void avkcps_exit(void *ctxt)
{
    AVKCPS *avkcps = ctxt;
    if (!ctxt) return;
    adev_start (avkcps->adev, 0);
    vdev_start (avkcps->vdev, 0);
    codec_start(avkcps->aenc, 0);
    codec_start(avkcps->venc, 0);
    avkcps->status |= TS_EXIT;
    pthread_join(avkcps->pthread, NULL);
    free(ctxt);
}

void avkcps_start(void *ctxt, int start)
{
    AVKCPS *avkcps = ctxt;
    if (!ctxt) return;
    if (start) {
        avkcps->status |= TS_START;
    } else {
        avkcps->status &=~TS_START;
    }
}
