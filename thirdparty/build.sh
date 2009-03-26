#! /bin/sh
set -e
PREFIX=`pwd`/..
export PKG_CONFIG_PATH="$PREFIX/local/lib/pkgconfig"
cd libogg
./autogen.sh --prefix=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libvorbis
./autogen.sh --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libtheora
./autogen.sh --prefix=$PREFIX/local --with-ogg=$PREFIX/local --with-vorbis=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../liboggz
./autogen.sh
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libfishsound
./autogen.sh
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/local/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/local/lib -lvorbis" ./configure --prefix=$PREFIX/local --disable-speex --disable-flac --disable-encode --disable-shared --enable-static
make && make install
cd ../libkate
OGG_CFLAGS=-I$PREFIX/local/include OGG_LIBS="-L$PREFIX/local/lib -logg" ./autogen.sh --prefix=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libtiger
./autogen.sh
KATE_CFLAGS=-I$PREFIX/local/include KATE_LIBS="-L$PREFIX/local/lib -lkate" ./autogen.sh --prefix=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../liboggplay
./autogen.sh
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/local/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/local/lib -lvorbis" THEORA_CFLAGS=-I$PREFIX/local/include THEORA_LIBS="-L$PREFIX/local/lib -ltheora" FISHSOUND_CFLAGS=-I$PREFIX/local/include FISHSOUND_LIBS="-L$PREFIX/local/lib -lfishsound" KATE_CFLAGS=-I$PREFIX/local/include KATE_LIBS="-L$PREFIX/local/lib -lkate" TIGER_CFLAGS=-I$PREFIX/local/include TIGER_LIBS="-L$PREFIX/local/lib -ltiger" ./configure --prefix=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libsydneyaudio
./autogen.sh
SOUND_BACKEND=--with-alsa
if [ $(uname -s) = "Darwin" ]; then
  SOUND_BACKEND=""
fi
./configure --prefix=$PREFIX/local --disable-shared --enable-static "$SOUND_BACKEND"
make && make install
