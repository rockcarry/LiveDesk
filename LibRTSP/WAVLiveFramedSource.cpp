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
#include "WAVLiveFramedSource.hh"

////////// ADTSLiveFramedSource //////////

WAVLiveFramedSource* WAVLiveFramedSource::createNew(UsageEnvironment& env, RTSPSERVER* server) {
  return new WAVLiveFramedSource(env, server);
}

WAVLiveFramedSource::WAVLiveFramedSource(UsageEnvironment& env, RTSPSERVER* server)
  : FramedSource(env), mServer(server), mMaxFrameSize(0) {
  fAudioFormat = WA_PCMA;
  fBitsPerSample = 8;
  fNumChannels = 1;
  fSamplingFrequency = 8000;
}

WAVLiveFramedSource::~WAVLiveFramedSource() {
}

void WAVLiveFramedSource::doGetNextFrame() {
    fFrameSize = mServer->aioctl(mServer->adev, AENC_CMD_READ, fTo, 160, NULL);
    fDurationInMicroseconds = 1000000 * fFrameSize / fSamplingFrequency;
    gettimeofday(&fPresentationTime, NULL);

    // To avoid possible infinite recursion, we need to return to the event loop to do this:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)FramedSource::afterGetting, this);
    if (mServer->bexit) handleClosure();
}
