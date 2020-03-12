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
// Framed File Sources
// C++ header

#ifndef _H26X_FRAMED_LIVE_SOURCE_HH
#define _H26X_FRAMED_LIVE_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

class H26XLiveFramedSource: public FramedSource {
public:
  static H26XLiveFramedSource* createNew(UsageEnvironment& env, RTSPSERVER* server);
  virtual unsigned maxFrameSize() const { return mMaxFrameSize; }

protected:
  H26XLiveFramedSource(UsageEnvironment& env, RTSPSERVER* server); // abstract base class
  virtual ~H26XLiveFramedSource();

private:
  RTSPSERVER* mServer;
  unsigned mMaxFrameSize;

private:
  virtual void doGetNextFrame();
};

#endif
