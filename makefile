all: oggplayer

oggplayer.o: oggplayer.cpp
	g++ -c -Ilocal/include -o oggplayer.o oggplayer.cpp

oggplayer: oggplayer.o
	g++ -o oggplayer oggplayer.o  local/lib/liboggplay.a local/lib/libfishsound.a local/lib/liboggz.a local/lib/libtheora.a local/lib/libvorbis.a local/lib/libtiger.a local/lib/libkate.a local/lib/libogg.a local/lib/libsydneyaudio.a `pkg-config --libs pangocairo` -lpthread -lasound -lSDL


clean: 
	rm *.o oggplayer
