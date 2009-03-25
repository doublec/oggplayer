PREFIX=`pwd`/..
cd libogg
./autogen.sh
./configure --prefix=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libvorbis
./autogen.sh
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libtheora
./autogen.sh
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --with-vorbis=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../liboggz
./autogen.sh
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libfishsound
./autogen.sh
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/local/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/local/lib -lvorbis" ./configure --prefix=$PREFIX/local --disable-speex --disable-flac --disable-encode --disable-shared --enable-static
make && make install
cd ../liboggplay
./autogen.sh
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/local/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/local/lib -lvorbis" THEORA_CFLAGS=-I$PREFIX/local/include THEORA_LIBS="-L$PREFIX/local/lib -ltheora" FISHSOUND_CFLAGS=-I$PREFIX/local/include FISHSOUND_LIBS="-L$PREFIX/local/lib -lfishsound" ./configure --prefix=$PREFIX/local --disable-speex --disable-shared --enable-static
make && make install
cd ../libsydneyaudio
./autogen.sh
./configure --prefix=$PREFIX/local --disable-shared --enable-static --with-alsa
make && make install
cd ..
