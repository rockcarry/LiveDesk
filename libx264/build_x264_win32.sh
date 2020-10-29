#!/bin/bash

./configure \
--prefix=$PWD/_install \
--disable-cli \
--enable-shared \
--enable-static \
--disable-interlaced \
--bit-depth=8 \
--chroma-format=420 \
--enable-lto \
--enable-strip \
--enable-pic \
--disable-avs \
--disable-swscale \
--disable-lavf \
--disable-ffms \
--disable-gpac \
--disable-lsmash

make -j2 && make install

cd $PWD/_install/lib
ar -x libx264.a
gcc -o x264.dll -shared -flto *.o libx264.a && strip x264.dll
rm *.o
dlltool -l x264.lib -d x264.def
cd -
