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
// on demand, from a H264/H265 Elementary Stream video live.
// C++ header

#ifndef _H26X_VIDEO_LIVE_SERVER_MEDIA_SUBSESSION_HH
#define _H26X_VIDEO_LIVE_SERVER_MEDIA_SUBSESSION_HH

#include "OnDemandRTSPServer.h"
#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_HH
#include "OnDemandServerMediaSubsession.hh"
#endif

class H26XVideoLiveServerMediaSubsession: public OnDemandServerMediaSubsession {
public:
  static H26XVideoLiveServerMediaSubsession* createNew(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource);

  // Used to implement "getAuxSDPLine()":
  void checkForAuxSDPLine1();
  void afterPlayingDummy1();

protected:
  H26XVideoLiveServerMediaSubsession(UsageEnvironment& env, RTSPSERVER* server, Boolean reuseFirstSource);
  virtual ~H26XVideoLiveServerMediaSubsession();

  void setDoneFlag() { fDoneFlag = ~0; }

protected: // redefined virtual functions
  virtual char const* getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource);
  virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                                    unsigned char rtpPayloadTypeIfDynamic,
                                    FramedSource* inputSource);

  virtual void startStream(unsigned clientSessionId, void* streamToken,
                           TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
                           unsigned short& rtpSeqNum, unsigned& rtpTimestamp,
                           ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
                           void* serverRequestAlternativeByteHandlerClientData);

  virtual void deleteStream(unsigned clientSessionId, void*& streamToken);

private:
  char* fAuxSDPLine;
  char fDoneFlag; // used when setting up "fAuxSDPLine"
  RTPSink* fDummyRTPSink; // ditto
  RTSPSERVER* mServer;
};

#endif
