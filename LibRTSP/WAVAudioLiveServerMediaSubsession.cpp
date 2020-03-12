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
// on demand, from an WAV audio live.
// Implementation

#include "WAVAudioLiveServerMediaSubsession.hh"
#include "WAVLiveFramedSource.hh"
#include "SimpleRTPSink.hh"

WAVAudioLiveServerMediaSubsession*
WAVAudioLiveServerMediaSubsession::createNew(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource) {
  return new WAVAudioLiveServerMediaSubsession(env, server, reuseFirstSource);
}

WAVAudioLiveServerMediaSubsession
::WAVAudioLiveServerMediaSubsession(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource)
  : OnDemandServerMediaSubsession(env, reuseFirstSource), mServer(server) {
}

WAVAudioLiveServerMediaSubsession::~WAVAudioLiveServerMediaSubsession() {
}

FramedSource* WAVAudioLiveServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  WAVLiveFramedSource *source = WAVLiveFramedSource::createNew(envir(), mServer);
  return source;
}

RTPSink* WAVAudioLiveServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
                   unsigned char rtpPayloadTypeIfDynamic,
                   FramedSource* inputSource) {
  WAVLiveFramedSource *source = (WAVLiveFramedSource*)inputSource;
  char const* mimeType;
  unsigned char payloadFormatCode = rtpPayloadTypeIfDynamic;
  if (source->audioFormat() == WA_PCMA) {
    mimeType = "PCMA";
    if (source->samplingFrequency() == 8000 && source->numChannels() == 1) {
      payloadFormatCode = 8;
    }
  }
  return SimpleRTPSink::createNew(envir(), rtpGroupsock,
                    payloadFormatCode, source->samplingFrequency(),
                    "audio", mimeType, source->numChannels());
}
