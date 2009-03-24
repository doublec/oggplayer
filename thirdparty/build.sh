PREFIX=/home/chris/src/oggplayer
cd libogg
./autogen.sh
./configure --prefix=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libvorbis
./autogen.sh
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-statuc
make && make install
cd ../libtheora
./autogen.sh
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --with-vorbis=$PREFIX/local --disable-shared --enable-static
make && make install
cd liboggz
./autogensh
./configure --prefix=$PREFIX/local --with-ogg=$PREFIX/local --disable-shared --enable-static
make && make install
cd ../libfishsound
./autogen.sh
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/lib -lvorbis" ./configure --prefix=$PREFIX/local --disable-speex --disable-flac --disable-encode --disable-shared --enable-static
make && make install
cd ../liboggplay
./autogen.sh
OGGZ_CFLAGS=-I$PREFIX/local/include OGGZ_LIBS="-L$PREFIX/lib -loggz" VORBIS_CFLAGS=-I$PREFIX/local/include VORBIS_LIBS="-L$PREFIX/lib -lvorbis" THEORA=-I$PREFIX/local/include THEORA="-L$PREFIX/lib -ltheora" FISHSOUND=-I$PREFIX/local/include FISHSOUND="-L$PREFIX/lib -lfishsound" ./configure --prefix=$PREFIX/local --disable-speex --disable-shared --enable-static
make && make install
cd ..

