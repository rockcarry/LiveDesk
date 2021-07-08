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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a H264/H265 video live.
// Implementation

#include "H26XVideoLiveServerMediaSubsession.hh"
#include "H26XLiveFramedSource.hh"
#include "H264VideoRTPSink.hh"
#include "H264VideoStreamFramer.hh"
#include "H265VideoRTPSink.hh"
#include "H265VideoStreamFramer.hh"

H26XVideoLiveServerMediaSubsession*
H26XVideoLiveServerMediaSubsession::createNew(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource) {
  return new H26XVideoLiveServerMediaSubsession(env, server, reuseFirstSource);
}

H26XVideoLiveServerMediaSubsession::H26XVideoLiveServerMediaSubsession(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource)
  : OnDemandServerMediaSubsession(env, reuseFirstSource),
    fAuxSDPLine(NULL), fDoneFlag(0), fDummyRTPSink(NULL), mServer(server) {
}

H26XVideoLiveServerMediaSubsession::~H26XVideoLiveServerMediaSubsession() {
  delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void* clientData) {
  H26XVideoLiveServerMediaSubsession* subsess = (H26XVideoLiveServerMediaSubsession*)clientData;
  subsess->afterPlayingDummy1();
}

void H26XVideoLiveServerMediaSubsession::afterPlayingDummy1() {
  // Unschedule any pending 'checking' task:
  envir().taskScheduler().unscheduleDelayedTask(nextTask());
  // Signal the event loop that we're done:
  setDoneFlag();
}

static void checkForAuxSDPLine(void* clientData) {
  H26XVideoLiveServerMediaSubsession* subsess = (H26XVideoLiveServerMediaSubsession*)clientData;
  subsess->checkForAuxSDPLine1();
}

void H26XVideoLiveServerMediaSubsession::checkForAuxSDPLine1() {
  nextTask() = NULL;

  char const* dasl;
  if (fAuxSDPLine != NULL) {
    // Signal the event loop that we're done:
    setDoneFlag();
  } else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
    fAuxSDPLine = strDup(dasl);
    fDummyRTPSink = NULL;

    // Signal the event loop that we're done:
    setDoneFlag();
  } else if (!fDoneFlag) {
    // try again after a brief delay:
    int uSecsToDelay = 100000; // 100 ms
    nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                  (TaskFunc*)checkForAuxSDPLine, this);
  }
}

char const* H26XVideoLiveServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
  if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

  if (fDummyRTPSink == NULL) { // we're not already setting it up for another, concurrent stream
    // Note: For H264/H265 video files, the 'config' information (used for several payload-format
    // specific parameters in the SDP description) isn't known until we start reading the file.
    // This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
    // and we need to start reading data from our file until this changes.
    fDummyRTPSink = rtpSink;

    // Start reading the file:
    fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

    // Check whether the sink's 'auxSDPLine()' is ready:
    checkForAuxSDPLine(this);
  }

  envir().taskScheduler().doEventLoop(&fDoneFlag);

  return fAuxSDPLine;
}

FramedSource* H26XVideoLiveServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  codec_reset(mServer->aenc, CODEC_CLEAR_INBUF|CODEC_CLEAR_OUTBUF|CODEC_REQUEST_IDR);
  codec_reset(mServer->venc, CODEC_CLEAR_INBUF|CODEC_CLEAR_OUTBUF|CODEC_REQUEST_IDR);
  codec_start(mServer->aenc, 1);
  codec_start(mServer->venc, 1);
  adev_start (mServer->adev, 1);
  vdev_start (mServer->vdev, 1);

  // Create the video source:
  H26XLiveFramedSource* source = H26XLiveFramedSource::createNew(envir(), mServer);
  if (source == NULL) return NULL;

  // Create a framer for the Video Elementary Stream:
  if (strcmp(mServer->venc->name, "h264enc") == 0) {
    return H264VideoStreamFramer::createNew(envir(), source);
  } else if (strcmp(mServer->venc->name, "h265enc") == 0) {
    return H265VideoStreamFramer::createNew(envir(), source);
  } else {
    return NULL;
  }
}

RTPSink* H26XVideoLiveServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* /*inputSource*/) {
  if (strcmp(mServer->venc->name, "h264enc") == 0) {
    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  } else if (strcmp(mServer->venc->name, "h265enc") == 0) {
    return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  } else {
    return NULL;
  }
}

void H26XVideoLiveServerMediaSubsession::startStream(unsigned clientSessionId, void* streamToken,
			TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
			unsigned short& rtpSeqNum, unsigned& rtpTimestamp,
			ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
            void* serverRequestAlternativeByteHandlerClientData) {
  mServer->running_streams++;
  codec_reset(mServer->venc, CODEC_REQUEST_IDR);
  OnDemandServerMediaSubsession::startStream(clientSessionId, streamToken, rtcpRRHandler, rtcpRRHandlerClientData, rtpSeqNum, rtpTimestamp,
    serverRequestAlternativeByteHandler, serverRequestAlternativeByteHandlerClientData);
}


void H26XVideoLiveServerMediaSubsession::deleteStream(unsigned clientSessionId, void*& streamToken) {
  mServer->running_streams--;
  if (mServer->running_streams == 0) {
    codec_start(mServer->aenc, 0);
    codec_start(mServer->venc, 0);
    adev_start (mServer->adev, 0);
    vdev_start (mServer->vdev, 0);
  }
  OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
}
