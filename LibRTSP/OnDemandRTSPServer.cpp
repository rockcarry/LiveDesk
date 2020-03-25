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
// Copyright (c) 1996-2018, Live Networks, Inc.  All rights reserved
// A test program that demonstrates how to stream - via unicast RTP
// - various kinds of file on demand, using a built-in RTSP server.
// main program

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "H26XVideoLiveServerMediaSubsession.hh"
#include "WAVAudioLiveServerMediaSubsession.hh"
#include "AACAudioLiveServerMediaSubsession.hh"
#include "OnDemandRTSPServer.h"

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
static Boolean reuseFirstSource = True;

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms, char const* streamName) {
  char* url = rtspServer->rtspURL(sms);
  UsageEnvironment& env = rtspServer->envir();
  env << "\n'" << streamName << "' stream\n";
  env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;
}

int rtsp_servermain(RTSPSERVER *server, char *pexit) {
  OutPacketBuffer::maxSize = 512 * 1024;

  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

  UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
  // To implement client access control to the RTSP server, do the following:
  authDB = new UserAuthenticationDatabase;
  authDB->addUserRecord("username1", "password1"); // replace these with real strings
  // Repeat the above with each <username>, <password> that you wish to allow
  // access to the server.
#endif

  // Create the RTSP server:
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 554, authDB, 10);
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }

  char const* descriptionString
    = "Session streamed by \"LiveDeskRtspServer\"";

  // Set up each of the possible streams that can be served by the
  // RTSP server.  Each such stream is implemented using a
  // "ServerMediaSession" object, plus one or more
  // "ServerMediaSubsession" objects for each audio/video substream.

  // A H264/H265 + G711a/AAC video elementary stream:
  {
    char const* streamName = "livedesk";
    ServerMediaSession* sms= ServerMediaSession::createNew(*env, streamName, streamName, descriptionString);
    switch (server->audio_enctype) {
    case 0: sms->addSubsession(WAVAudioLiveServerMediaSubsession::createNew(*env, server, reuseFirstSource)); break;
    case 1: sms->addSubsession(AACAudioLiveServerMediaSubsession::createNew(*env, server, reuseFirstSource)); break;
    }
    sms->addSubsession(H26XVideoLiveServerMediaSubsession::createNew(*env, server, reuseFirstSource));
    rtspServer->addServerMediaSession(sms);
    announceStream(rtspServer, sms, streamName);
  }

  env->taskScheduler().doEventLoop(pexit); // does not return

  if (rtspServer) delete rtspServer;
  if (authDB    ) delete authDB;
  if (env       ) env->reclaim();
  if (scheduler ) delete scheduler;
  return 0; // only to prevent compiler warning
}

