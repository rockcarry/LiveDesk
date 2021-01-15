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
// on demand, from an AAC audio live.
// Implementation

#include "AACAudioLiveServerMediaSubsession.hh"
#include "AACLiveFramedSource.hh"
#include "MPEG4GenericRTPSink.hh"

AACAudioLiveServerMediaSubsession*
AACAudioLiveServerMediaSubsession::createNew(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource) {
  return new AACAudioLiveServerMediaSubsession(env, server, reuseFirstSource);
}

AACAudioLiveServerMediaSubsession
::AACAudioLiveServerMediaSubsession(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource)
  : OnDemandServerMediaSubsession(env, reuseFirstSource), mServer(server) {
}

AACAudioLiveServerMediaSubsession::~AACAudioLiveServerMediaSubsession() {
}

FramedSource* AACAudioLiveServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  AACLiveFramedSource *source = AACLiveFramedSource::createNew(envir(), mServer, mServer->aenc->aacinfo);
  return source;
}

RTPSink* AACAudioLiveServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
                   unsigned char rtpPayloadTypeIfDynamic,
                   FramedSource* inputSource) {
  AACLiveFramedSource *source = (AACLiveFramedSource*)inputSource;
  return MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
                    rtpPayloadTypeIfDynamic,
                    source->samplingFrequency(),
                    "audio", "AAC-hbr", source->configStr(),
                    source->numChannels());
}
