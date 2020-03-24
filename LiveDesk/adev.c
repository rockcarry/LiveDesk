#include <stdint.h>
#include <windows.h>
#include <pthread.h>
#include "stdafx.h"
#include "ringbuf.h"
#include "adev.h"
#include "log.h"

#define WAVE_SAMPLE_SIZE  16
#define WAVE_FRAME_RATE   50
#define WAVE_BUFFER_NUM   6

typedef struct {
    HWAVEIN  hwavein;
    int      isalaw;
    int      channels;
    int      samprate;
    int      head;
    int      tail;
    int      size;
    int      bufsize;
    uint8_t *buffer;
    uint8_t *wavbuf[WAVE_BUFFER_NUM];
    #define  TS_START (1 << 0)
    int      status;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} ADEV;

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
    ADEV   *adev = (ADEV   *)dwInstance;
    WAVEHDR *phdr= (WAVEHDR*)dwParam1;
    int      writesize, i;

    switch (uMsg) {
    case WIM_DATA:
        pthread_mutex_lock(&adev->mutex);
        if (adev->isalaw) {
            for (i=0; i<(int)(phdr->dwBytesRecorded/sizeof(int16_t)); i++) {
                adev->buffer[adev->tail] = pcm2alaw(((int16_t*)phdr->lpData)[i]);
                if (++adev->tail == adev->bufsize) adev->tail = 0;
                if (adev->size < adev->bufsize) {
                    adev->size++;
                } else {
                    adev->head = adev->tail;
                }
            }
        } else {
            writesize = (int)phdr->dwBytesRecorded < adev->bufsize - adev->size ? (int)phdr->dwBytesRecorded : adev->bufsize - adev->size;
            if (writesize < (int)phdr->dwBytesRecorded) {
                log_printf("adev drop data %d !\n", phdr->dwBytesRecorded - writesize);
            }
            adev->tail = ringbuf_write(adev->buffer, adev->bufsize, adev->tail, phdr->lpData, writesize);
            adev->size+= writesize;
        }
        pthread_cond_signal(&adev->cond);
        pthread_mutex_unlock(&adev->mutex);
        waveInAddBuffer(hWav, phdr, sizeof(WAVEHDR));
        break;
    }
    return TRUE;
}

void* adev_init(int channels, int samplerate, int isalaw, int bufsize)
{
    ADEV        *adev   = NULL;
    WAVEFORMATEX wavfmt = {0};
    int          wavsize, i;

    wavsize = sizeof(WAVEHDR) + ALIGN(samplerate * channels / WAVE_FRAME_RATE, 2) * sizeof(int16_t);
    adev    = calloc(1, sizeof(ADEV) + bufsize + WAVE_BUFFER_NUM * wavsize);
    if (!adev) {
        log_printf("failed to allocate memory for ADEV context !\n");
        return NULL;
    }

    adev->isalaw   = isalaw;
    adev->channels = channels;
    adev->samprate = samplerate;
    adev->bufsize  = bufsize;
    adev->buffer   = (uint8_t*)adev + sizeof(ADEV);
    for (i=0; i<WAVE_BUFFER_NUM; i++) {
        adev->wavbuf[i] = (uint8_t*)adev + sizeof(ADEV) + bufsize + i * wavsize;
    }

    // init mutex & cond
    pthread_mutex_init(&adev->mutex, NULL);
    pthread_cond_init (&adev->cond , NULL);

    wavfmt.wFormatTag     = WAVE_FORMAT_PCM;
    wavfmt.nChannels      = channels;
    wavfmt.nSamplesPerSec = samplerate;
    wavfmt.wBitsPerSample = WAVE_SAMPLE_SIZE;
    wavfmt.nBlockAlign    = WAVE_SAMPLE_SIZE * channels / 8;
    wavfmt.nAvgBytesPerSec= samplerate * wavfmt.nBlockAlign;
    waveInOpen(&adev->hwavein, WAVE_MAPPER, &wavfmt, (DWORD_PTR)waveInProc, (DWORD_PTR)adev, CALLBACK_FUNCTION);
    if (!adev->hwavein) {
        log_printf("failed to open wavein device !\n");
        free(adev);
        return NULL;
    }

    for (i=0; i<WAVE_BUFFER_NUM; i++) {
        WAVEHDR *phdr = (WAVEHDR*)adev->wavbuf[i];
        phdr->dwBufferLength = wavsize - sizeof(WAVEHDR);
        phdr->lpData         = (LPSTR)phdr + sizeof(WAVEHDR);
        waveInPrepareHeader(adev->hwavein, phdr, sizeof(WAVEHDR));
        waveInAddBuffer(adev->hwavein, phdr, sizeof(WAVEHDR));
    }
    return adev;
}

void adev_free(void *ctxt)
{
    ADEV *adev = (ADEV*)ctxt;
    int   i;

    if (!adev) return;
    if (adev->hwavein) {
        waveInStop(adev->hwavein);
        for (i=0; i<ARRAY_SIZE(adev->wavbuf); i++) {
            WAVEHDR *phdr = (WAVEHDR*)adev->wavbuf[i];
            waveInUnprepareHeader(adev->hwavein, phdr, sizeof(WAVEHDR));
        }
        waveInClose(adev->hwavein);
    }

    pthread_mutex_destroy(&adev->mutex);
    pthread_cond_destroy (&adev->cond );
    free(adev);
}

int adev_ioctl(void *ctxt, int cmd, void *buf, int bsize, int *fsize)
{
    ADEV *adev = (ADEV*)ctxt;
    int   ret  = 0;
    if (!adev || !adev->hwavein) return -1;

    switch (cmd) {
    case ADEV_CMD_START:
        waveInStart(adev->hwavein);
        adev->status |= TS_START;
        break;
    case ADEV_CMD_STOP:
        waveInStop(adev->hwavein);
        pthread_mutex_lock(&adev->mutex);
        adev->status &=~TS_START;
        pthread_cond_signal(&adev->cond);
        pthread_mutex_unlock(&adev->mutex);
        break;
    case ADEV_CMD_RESET_BUFFER:
        pthread_mutex_lock(&adev->mutex);
        adev->head = adev->tail = adev->size = 0;
        pthread_mutex_unlock(&adev->mutex);
        break;
    case ADEV_CMD_READ:
        pthread_mutex_lock(&adev->mutex);
        while (adev->size < bsize && (adev->status & TS_START)) pthread_cond_wait(&adev->cond, &adev->mutex);
        if (adev->size > 0) {
            ret = bsize < adev->size ? bsize : adev->size;
            adev->head = ringbuf_read(adev->buffer, adev->bufsize, adev->head, buf, ret);
            adev->size-= ret;
        }
        pthread_mutex_unlock(&adev->mutex);
        return ret;
    case ADEV_LOCK_BUFFER:
        pthread_mutex_lock(&adev->mutex);
        while (adev->size < bsize && (adev->status & TS_START)) pthread_cond_wait(&adev->cond, &adev->mutex);
        *(void**)buf = &adev->buffer[adev->head];
        adev->size -= bsize;
        adev->head += bsize;
        if (adev->head == adev->bufsize) adev->head = 0;
        return bsize;
    case ADEV_UNLOCK_BUFFER:
        pthread_mutex_unlock(&adev->mutex);
        break;
    default: return -1;
    }
    return 0;
}
