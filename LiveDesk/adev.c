#include <stdint.h>
#include <windows.h>
#include "stdafx.h"
#include "adev.h"
#include "log.h"

#define WAVE_SAMPLE_SIZE  16
#define WAVE_FRAME_RATE   50
#define WAVE_BUFFER_NUM   5
typedef struct {
    HWAVEIN  hwavein;
    WAVEHDR  wavhdr[WAVE_BUFFER_NUM];
    void    *codec;
    PFN_CODEC_CALLBACK callback;
} ADEV;

static BOOL CALLBACK waveInProc(HWAVEIN hWav, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    ADEV   *adev = (ADEV   *)dwInstance;
    WAVEHDR *phdr= (WAVEHDR*)dwParam1;

    switch (uMsg) {
    case WIM_DATA:
        if (adev->callback) {
            void *buf[8] = { phdr->lpData };
            int   len[8] = { phdr->dwBytesRecorded };
            adev->callback(adev->codec, buf, len);
        }
        waveInAddBuffer(hWav, phdr, sizeof(WAVEHDR));
        break;
    }
    return TRUE;
}

void* adev_init(int channels, int samplerate)
{
    ADEV        *adev   = NULL;
    WAVEFORMATEX wavfmt = {0};
    int          wavsize, i;

    wavsize = ALIGN(samplerate * channels / WAVE_FRAME_RATE, 2) * sizeof(int16_t);
    adev    = calloc(1, sizeof(ADEV) + WAVE_BUFFER_NUM * wavsize);
    if (!adev) {
        log_printf("failed to allocate memory for ADEV context !\n");
        return NULL;
    }

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
        adev->wavhdr[i].dwBufferLength = wavsize;
        adev->wavhdr[i].lpData         = (LPSTR)adev + sizeof(ADEV) + i * wavsize;
        waveInPrepareHeader(adev->hwavein, &adev->wavhdr[i], sizeof(WAVEHDR));
        waveInAddBuffer(adev->hwavein, &adev->wavhdr[i], sizeof(WAVEHDR));
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
        for (i=0; i<WAVE_BUFFER_NUM; i++) {
            waveInUnprepareHeader(adev->hwavein, &adev->wavhdr[i], sizeof(WAVEHDR));
        }
        waveInClose(adev->hwavein);
    }
    free(adev);
}

void adev_start(void *ctxt, int start)
{
    ADEV *adev = (ADEV*)ctxt;
    if (!adev) return;
    if (start) waveInStart(adev->hwavein);
    else waveInStop(adev->hwavein);
}

void adev_set_callback(void *ctxt, PFN_CODEC_CALLBACK callback, void *codec)
{
    ADEV *adev = (ADEV*)ctxt;
    if (!adev) return;
    adev->codec    = codec;
    adev->callback = callback;
}

