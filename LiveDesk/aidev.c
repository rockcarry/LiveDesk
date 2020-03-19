#include <stdint.h>
#include <windows.h>
#include <pthread.h>
#include "stdafx.h"
#include "ringbuf.h"
#include "aidev.h"
#include "log.h"

#define WAVE_CHANNEL_NUM  1
#define WAVE_SAMPLE_SIZE  16
#define WAVE_SAMPLE_RATE  8000
#define WAVE_FRAME_RATE   25
#define WAVE_BUFFER_NUM   3

typedef struct {
    HWAVEIN  hwavein;
    int      head;
    int      tail;
    int      size;
    uint8_t  alawbuf[WAVE_BUFFER_NUM * ALIGN(WAVE_SAMPLE_RATE/WAVE_FRAME_RATE, 4)];
    uint8_t  wavebuf[WAVE_BUFFER_NUM][sizeof(WAVEHDR) + ALIGN(WAVE_SAMPLE_RATE/WAVE_FRAME_RATE, 2) * sizeof(int16_t)];
    #define  TS_START (1 << 0)
    int      status;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} AIDEV;

static uint8_t pcm2alaw(int16_t pcm)
{
    uint8_t sign = (pcm >> 8) & (1 << 7);
    int  mask, eee, wxyz, alaw;
    if (sign) pcm = -pcm;
    for (mask=0x4000,eee=7; (pcm&mask)==0&&eee>0; eee--,mask>>=1);
    wxyz  = (pcm >> ((eee == 0) ? 4 : (eee + 3))) & 0xf;
    alaw  = sign | (eee << 4) | wxyz;
    return (alaw ^ 0xd5);
}

static BOOL CALLBACK waveInProc(HWAVEIN hWav, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    AIDEV   *aidev = (AIDEV  *)dwInstance;
    WAVEHDR *phdr  = (WAVEHDR*)dwParam1;
    int      i;

    switch (uMsg) {
    case WIM_DATA:
        pthread_mutex_lock(&aidev->mutex);
        for (i=0; i<(int)(phdr->dwBytesRecorded/sizeof(int16_t)); i++) {
            aidev->alawbuf[aidev->tail] = pcm2alaw(((int16_t*)phdr->lpData)[i]);
            if (++aidev->tail == sizeof(aidev->alawbuf)) aidev->tail = 0;
            if (aidev->size < sizeof(aidev->alawbuf)) {
                aidev->size++;
            } else {
                aidev->head = aidev->tail;
            }
        }
        pthread_cond_signal(&aidev->cond);
        pthread_mutex_unlock(&aidev->mutex);
        waveInAddBuffer(hWav, phdr, sizeof(WAVEHDR));
        break;
    }
    return TRUE;
}

void* aidev_init(void)
{
    AIDEV       *aidev  = NULL;
    WAVEFORMATEX wavfmt = {0};
    int          i;

    aidev = calloc(1, sizeof(AIDEV));
    if (!aidev) {
        log_printf("failed to allocate memory for AIDEV context !\n");
        return NULL;
    }

    // init mutex & cond
    pthread_mutex_init(&aidev->mutex, NULL);
    pthread_cond_init (&aidev->cond , NULL);

    wavfmt.wFormatTag     = WAVE_FORMAT_PCM;
    wavfmt.nChannels      = WAVE_CHANNEL_NUM;
    wavfmt.nSamplesPerSec = WAVE_SAMPLE_RATE;
    wavfmt.wBitsPerSample = WAVE_SAMPLE_SIZE;
    wavfmt.nBlockAlign    = WAVE_SAMPLE_SIZE * WAVE_CHANNEL_NUM / 8;
    wavfmt.nAvgBytesPerSec= WAVE_SAMPLE_RATE * wavfmt.nBlockAlign;
    waveInOpen(&aidev->hwavein, WAVE_MAPPER, &wavfmt, (DWORD_PTR)waveInProc, (DWORD_PTR)aidev, CALLBACK_FUNCTION);
    if (!aidev->hwavein) {
        log_printf("failed to open wavein device !\n");
        free(aidev);
        return NULL;
    }

    for (i=0; i<WAVE_BUFFER_NUM; i++) {
        WAVEHDR *phdr = (WAVEHDR*)aidev->wavebuf[i];
        phdr->dwBufferLength = sizeof(aidev->wavebuf[i]) - sizeof(WAVEHDR);
        phdr->lpData         = (LPSTR)phdr + sizeof(WAVEHDR);
        waveInPrepareHeader(aidev->hwavein, phdr, sizeof(WAVEHDR));
        waveInAddBuffer(aidev->hwavein, phdr, sizeof(WAVEHDR));
    }
    return aidev;
}

void aidev_free(void *ctxt)
{
    AIDEV *aidev = (AIDEV*)ctxt;
    int    i;

    if (!aidev) return;
    if (aidev->hwavein) {
        waveInStop(aidev->hwavein);
        for (i=0; i<ARRAY_SIZE(aidev->wavebuf); i++) {
            WAVEHDR *phdr = (WAVEHDR*)aidev->wavebuf[i];
            waveInUnprepareHeader(aidev->hwavein, phdr, sizeof(WAVEHDR));
        }
        waveInClose(aidev->hwavein);
    }

    pthread_mutex_destroy(&aidev->mutex);
    pthread_cond_destroy (&aidev->cond );
    free(aidev);
}

int aidev_ctrl(void *ctxt, int cmd, void *buf, int size)
{
    AIDEV *aidev = (AIDEV*)ctxt;
    int    ret   = 0;
    if (!aidev || !aidev->hwavein) return -1;

    switch (cmd) {
    case AIDEV_CMD_START:
        waveInStart(aidev->hwavein);
        aidev->status |= TS_START;
        return 0;
    case AIDEV_CMD_STOP:
        waveInStop(aidev->hwavein);
        pthread_mutex_lock(&aidev->mutex);
        aidev->status &=~TS_START;
        pthread_cond_signal(&aidev->cond);
        pthread_mutex_unlock(&aidev->mutex);
        return 0;
    case AIDEV_CMD_RESET_BUFFER:
        pthread_mutex_lock(&aidev->mutex);
        aidev->head = aidev->tail = aidev->size = 0;
        pthread_mutex_unlock(&aidev->mutex);
        return 0;
    case AIDEV_CMD_READ:
        pthread_mutex_lock(&aidev->mutex);
        while (aidev->size <= 0 && (aidev->status & TS_START)) pthread_cond_wait(&aidev->cond, &aidev->mutex);
        if (aidev->size > 0) {
            ret = size < aidev->size / WAVE_FRAME_RATE ? size : aidev->size / WAVE_FRAME_RATE;
            aidev->head = ringbuf_read(aidev->alawbuf, sizeof(aidev->alawbuf), aidev->head, buf, ret);
            aidev->size-= ret;
        }
        pthread_mutex_unlock(&aidev->mutex);
        return ret;
    default: return -1;
    }
}
