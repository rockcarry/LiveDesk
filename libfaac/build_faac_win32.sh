#!/bin/sh

set -e

case "$1" in
"")
    if [ ! -e $PWD/Makefile ]; then
        ./configure \
        --prefix=$PWD/_install \
        --enable-static \
        --disable-shared \
        --disable-drm \
        --with-gnu-ld
        touch aclocal.m4
        touch configure
        touch Makefile.in
    fi
    make -j8
    make install
    cd $PWD/_install/lib/
    ar x libfaac.a
    gcc -o faac.dll -shared -flto *.o libfaac.a && strip faac.dll
    rm *.o
    dlltool -l faac.lib -d faac.def
    cd -
    ;;
clean)
    make clean
    ;;
distclean)
    make distclean
    rm -rf $PWD/_install
    rm -rf $PWD/Makefile
    ;;
esac

