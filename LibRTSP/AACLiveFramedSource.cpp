/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// A WAV audio live source
// Implementation

#include "OnDemandRTSPServer.h"
#include "AACLiveFramedSource.hh"

////////// ADTSLiveFramedSource //////////

static unsigned const samplingFrequencyTable[16] = {
  96000, 88200, 64000, 48000,
  44100, 32000, 24000, 22050,
  16000, 12000, 11025, 8000,
  7350, 0, 0, 0
};

AACLiveFramedSource* AACLiveFramedSource::createNew(UsageEnvironment& env, RTSPSERVER* server, unsigned char* aacconfig) {
  return new AACLiveFramedSource(env, server, aacconfig);
}

AACLiveFramedSource::AACLiveFramedSource(UsageEnvironment& env, RTSPSERVER* server, unsigned char* aacconfig)
  : FramedSource(env), mServer(server), mMaxFrameSize(0) {
  int sampling_frequency_index = ((aacconfig[1] >> 7) & (1 << 0)) | ((aacconfig[0] & 0x7) << 1);
  int channel_configuration    = (aacconfig[1] >> 3) & 0xf;

  fSamplingFrequency = samplingFrequencyTable[sampling_frequency_index];
  fNumChannels = channel_configuration == 0 ? 2 : channel_configuration;
  fuSecsPerFrame = (1024/*samples-per-frame*/*1000000) / fSamplingFrequency/*samples-per-second*/;
  sprintf_s(fConfigStr, "%02X%02x", aacconfig[0], aacconfig[1]);
}

AACLiveFramedSource::~AACLiveFramedSource() {
}

void AACLiveFramedSource::doGetNextFrame() {
    fFrameSize = mServer->actrl(mServer->adev, AENC_CMD_READ, fTo, fMaxSize);
    fDurationInMicroseconds = fFrameSize ? fuSecsPerFrame : 0;
    gettimeofday(&fPresentationTime, NULL);
    if (fFrameSize > 0 && mMaxFrameSize < fFrameSize) mMaxFrameSize = fFrameSize;

    // To avoid possible infinite recursion, we need to return to the event loop to do this:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)FramedSource::afterGetting, this);
    if (mServer->bexit) handleClosure();
}
