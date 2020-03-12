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
// A AAC audio live source
// C++ header

#ifndef _AAC_FRAMED_LIVE_SOURCE_HH
#define _AAC_FRAMED_LIVE_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

class AACLiveFramedSource: public FramedSource {
public:
  static AACLiveFramedSource* createNew(UsageEnvironment& env, RTSPSERVER* server, unsigned char* aacconfig);
  unsigned samplingFrequency() const { return fSamplingFrequency; }
  unsigned numChannels() const { return fNumChannels; }
  char const* configStr() const { return fConfigStr; }
  virtual unsigned maxFrameSize() const { return mMaxFrameSize; }

protected:
  AACLiveFramedSource(UsageEnvironment& env, RTSPSERVER* server, unsigned char* aacconfig);
  virtual ~AACLiveFramedSource();

private:
  RTSPSERVER* mServer;
  unsigned fSamplingFrequency;
  unsigned fNumChannels;
  unsigned fuSecsPerFrame;
  char fConfigStr[5];
  unsigned mMaxFrameSize;

private:
  virtual void doGetNextFrame();
};

#endif
