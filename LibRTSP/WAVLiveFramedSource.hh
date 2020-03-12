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
// C++ header

#ifndef _WAV_FRAMED_LIVE_SOURCE_HH
#define _WAV_FRAMED_LIVE_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

typedef enum {
  WA_PCM = 0x01,
  WA_PCMA = 0x06,
  WA_PCMU = 0x07,
  WA_IMA_ADPCM = 0x11,
  WA_UNKNOWN
} WAV_AUDIO_FORMAT;

class WAVLiveFramedSource: public FramedSource {
public:
  static WAVLiveFramedSource* createNew(UsageEnvironment& env, RTSPSERVER* server);
  unsigned char audioFormat() const { return fAudioFormat; }
  unsigned char bitsPerSample() const { return fBitsPerSample; }
  unsigned char numChannels() const { return fNumChannels; }
  unsigned samplingFrequency() const { return fSamplingFrequency; }
  virtual unsigned maxFrameSize() const { return mMaxFrameSize; }

protected:
  WAVLiveFramedSource(UsageEnvironment& env, RTSPSERVER* server);
  virtual ~WAVLiveFramedSource();

private:
  RTSPSERVER* mServer;
  unsigned char fAudioFormat, fBitsPerSample, fNumChannels;
  unsigned fSamplingFrequency;
  unsigned mMaxFrameSize;

private:
  virtual void doGetNextFrame();
};

#endif
