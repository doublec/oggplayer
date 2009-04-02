#! /bin/sh
set -e
PREFIX=`pwd`/..
export PKG_CONFIG_PATH="$PREFIX/local/lib/pkgconfig"
cd libogg
./autogen.sh --prefix=$PREFIX/local --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../libvorbis
./autogen.sh --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../libtheora
./autogen.sh --prefix=$PREFIX/local --with-ogg=$PREFIX/local --with-vorbis=$PREFIX/local --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../liboggz
./autogen.sh || exit 1
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../libfishsound
./autogen.sh || exit 1
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/local/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/local/lib -lvorbis" ./configure --prefix=$PREFIX/local --disable-speex --disable-flac --disable-encode --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../libkate
OGG_CFLAGS=-I$PREFIX/local/include OGG_LIBS="-L$PREFIX/local/lib -logg" ./autogen.sh --prefix=$PREFIX/local --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../libtiger
./autogen.sh || exit 1
KATE_CFLAGS=-I$PREFIX/local/include KATE_LIBS="-L$PREFIX/local/lib -lkate" ./autogen.sh --prefix=$PREFIX/local --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../liboggplay
./autogen.sh || exit 1
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/local/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/local/lib -lvorbis" THEORA_CFLAGS=-I$PREFIX/local/include THEORA_LIBS="-L$PREFIX/local/lib -ltheora" FISHSOUND_CFLAGS=-I$PREFIX/local/include FISHSOUND_LIBS="-L$PREFIX/local/lib -lfishsound" KATE_CFLAGS=-I$PREFIX/local/include KATE_LIBS="-L$PREFIX/local/lib -lkate" TIGER_CFLAGS=-I$PREFIX/local/include TIGER_LIBS="-L$PREFIX/local/lib -ltiger" ./configure --prefix=$PREFIX/local --disable-shared --enable-static || exit 1
make && make install || exit 1
cd ../libsydneyaudio
./autogen.sh || exit 1
SOUND_BACKEND=--with-alsa
if [ $(uname -s) = "Darwin" ]; then
  SOUND_BACKEND=""
fi
./configure --prefix=$PREFIX/local --disable-shared --enable-static "$SOUND_BACKEND" || exit 1
make && make install || exit 1
