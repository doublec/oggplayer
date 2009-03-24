// Copyright (C) 2009, Chris Double. All Rights Reserved.
// See the license at the end of this file.
#include <cstring>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <oggplay/oggplay.h>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>

extern "C" {
#include <sydney_audio.h>
}

using namespace std;
using namespace boost;

// Helper function to make creating and assigning shared pointers less verbose
template <class T>
shared_ptr<T> msp(T* t) {
  return shared_ptr<T>(t);
}

class Track {
  public:
    shared_ptr<OggPlay> mPlayer;
    OggzStreamContent mType;
    int mIndex;

  public:
    Track(shared_ptr<OggPlay> player, OggzStreamContent type, int index) : 
      mPlayer(player), mType(type), mIndex(index) { }

    // Convert the track to a one line string summary for display
    virtual string toString() const = 0;

    // Set the track to be active
    void setActive(bool active = true) { 
      int r = oggplay_set_track_active(mPlayer.get(), mIndex);
      assert(r == E_OGGPLAY_OK);
    }
};

class TheoraTrack : public Track {
  public:
    double mFramerate;

  public:
    TheoraTrack(shared_ptr<OggPlay> player, int index, double framerate) : 
      Track(player, OGGZ_CONTENT_THEORA, index), mFramerate(framerate) { }

    virtual string toString() const {
      ostringstream str;
      str << mIndex << ": Theora " << mFramerate << " fps";
      return str.str();
    }
};

class VorbisTrack : public Track {
  public:
    int mRate;
    int mChannels;

  public:
    VorbisTrack(shared_ptr<OggPlay> player, int index, int rate, int channels) : 
      Track(player, OGGZ_CONTENT_VORBIS, index), mRate(rate), mChannels(channels) { }

    virtual string toString() const {
      ostringstream str;
      str << mIndex << ": Vorbis " << mRate << " KHz " << mChannels << " channels";
      return str.str();
    }
};

class UnknownTrack : public Track {
  public:
    UnknownTrack(shared_ptr<OggPlay> player, OggzStreamContent type, int index) : 
      Track(player, type, index) { }

    virtual string toString() const {
      ostringstream str;
      str << mIndex << ": Unknown type " << oggplay_get_track_typename(mPlayer.get(), mIndex);
      return str.str();
    }
};

shared_ptr<Track> handle_theora_metadata(shared_ptr<OggPlay> player, int index) {
  int denom, num;
  int r = oggplay_get_video_fps(player.get(), index, &denom, &num);
  assert(r == E_OGGPLAY_OK);
  return msp(new TheoraTrack(player, index, static_cast<float>(denom) / num));
}

shared_ptr<Track> handle_vorbis_metadata(shared_ptr<OggPlay> player, int index) {
  int rate, channels;
  int r = oggplay_get_audio_samplerate(player.get(), index, &rate);
  assert(r == E_OGGPLAY_OK);
  r = oggplay_get_audio_channels(player.get(), index, &channels);
  assert(r == E_OGGPLAY_OK);

  return msp(new VorbisTrack(player,index, rate, channels));
}

shared_ptr<Track> handle_unknown_metadata(shared_ptr<OggPlay> player, OggzStreamContent type, int index) {
  return msp(new UnknownTrack(player, type, index));
}

template <class OutputIterator>
void load_metadata(shared_ptr<OggPlay> player, OutputIterator out) {
  int num_tracks = oggplay_get_num_tracks(player.get());

  for (int i=0; i < num_tracks; ++i) {
    OggzStreamContent type = oggplay_get_track_type(player.get(), i);
    switch (type) {
      case OGGZ_CONTENT_THEORA: 
        *out++ = handle_theora_metadata(player, i);
        break;

      case OGGZ_CONTENT_VORBIS:
        *out++ = handle_vorbis_metadata(player, i);
        break;

      default:
        *out++ = handle_unknown_metadata(player, type, i);
        break;
    }
  }
}

void dump_track(shared_ptr<Track> track) {
  cout << track->toString() << endl;
}

// Return the first video track in the range of tracks
template <class InputIterator>
shared_ptr<TheoraTrack> first_video_track(InputIterator first, InputIterator last) {
  while (first != last) {
    shared_ptr<TheoraTrack> video(dynamic_pointer_cast<TheoraTrack>(*first++));
    if (video)
      return video;
  }
  return shared_ptr<TheoraTrack>();
}

// Return the first audio track in the range of tracks
template <class InputIterator>
shared_ptr<VorbisTrack> first_audio_track(InputIterator first, InputIterator last) {
  while (first != last) {
    shared_ptr<VorbisTrack> video(dynamic_pointer_cast<VorbisTrack>(*first++));
    if (video)
      return video;
  }
  return shared_ptr<VorbisTrack>();
}

// Process the audio data provided by liboggplay. 'count' is the number of
// floats contained within 'data'.
void handle_audio_data(shared_ptr<sa_stream_t> sound, OggPlayAudioData* data, int count) {
  // Convert float data to S16 LE
  scoped_array<short> dest(new short[count]);
  float* source = reinterpret_cast<float*>(data);
  for (int i=0; i < count; ++i) {
    float scaled = floorf(0.5 + 32768 * source[i]);
    if (source[i] < 0.0)
      dest[i] = scaled < -32768.0 ? -32768 : static_cast<short>(scaled);
    else
      dest[i] = scaled > 32767.0 ? 32767 : static_cast<short>(scaled);
  }

  int sr = sa_stream_write(sound.get(), dest.get(), count * sizeof(short));
  assert(sr == SA_SUCCESS);
}

// Play the tracks. Exits when the longest track has completed playing
void play(shared_ptr<VorbisTrack> audio, shared_ptr<TheoraTrack> video) {
  // For now we need an audio track to exist
  assert(audio);
  shared_ptr<OggPlay> player(audio->mPlayer);

  // Open an audio stream
  sa_stream* s;
  int sr = sa_stream_create_pcm(&s,
                                NULL,
                                SA_MODE_WRONLY,
                                SA_PCM_FORMAT_S16_NE,
                                audio->mRate,
                                audio->mChannels);
  assert(sr == SA_SUCCESS);
  shared_ptr<sa_stream_t> sound(s, sa_stream_destroy);
  s = NULL;

  sr = sa_stream_open(sound.get());
  assert(sr == SA_SUCCESS);

  int r = oggplay_use_buffer(player.get(), 20);
  assert(r == E_OGGPLAY_OK);

  // E_OGGPLAY_CONTINUE       = One frame decoded and put in buffer list
  // E_OGGPLAY_USER_INTERRUPT = One frame decoded, buffer list is now full
  // E_OGGPLAY_TIMEOUT        = No frames decoded, timed out
  for (r = oggplay_step_decoding(player.get());
       r == E_OGGPLAY_TIMEOUT ||
       r == E_OGGPLAY_USER_INTERRUPT ||
       r == E_OGGPLAY_CONTINUE;
       r = oggplay_step_decoding(player.get())) {
    OggPlayCallbackInfo** info = oggplay_buffer_retrieve_next(player.get());
    if (!info)
     return;

    int num_tracks = oggplay_get_num_tracks(player.get());
    assert(audio->mIndex < num_tracks);
    assert(video->mIndex < num_tracks);

    if (oggplay_callback_info_get_type(info[audio->mIndex]) == OGGPLAY_FLOATS_AUDIO) {
      OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[audio->mIndex]);
      double time = oggplay_callback_info_get_presentation_time(headers[0]) / 1000.0;
      int required = oggplay_callback_info_get_required(info[audio->mIndex]);
      for (int i=0; i<required;++i) {
        int size = oggplay_callback_info_get_record_size(headers[i]);
        OggPlayAudioData* data = oggplay_callback_info_get_audio_data(headers[i]);
        handle_audio_data(sound, data, size * audio->mChannels);
      }
    }

    oggplay_buffer_release(player.get(), info);
  } 
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cout << "Usage: oggplayer <filename>" << endl;
    return -1;
  }

  OggPlayReader* reader = 0;
  if (strncmp(argv[1], "http://", 7) == 0) 
    reader = oggplay_tcp_reader_new(argv[1], NULL, 0);
  else
    reader = oggplay_file_reader_new(argv[1]);

  shared_ptr<OggPlay> player(oggplay_open_with_reader(reader), oggplay_close);

  vector<shared_ptr<Track> > tracks;
  load_metadata(player, back_inserter(tracks));
  for_each(tracks.begin(), tracks.end(), dump_track);

  shared_ptr<TheoraTrack> video(first_video_track(tracks.begin(), tracks.end()));
  shared_ptr<VorbisTrack> audio(first_audio_track(tracks.begin(), tracks.end()));

  cout << "Using the following tracks: " << endl;
  if (video) {
    video->setActive();
    oggplay_set_callback_num_frames(player.get(), video->mIndex, 1);
    cout << "  " << video->toString() << endl;
  }

  if (audio) {
    audio->setActive();
    if (!video)
      oggplay_set_callback_num_frames(player.get(), audio->mIndex, 2048);

    cout << "  " << audio->toString() << endl;
  }

  play(audio, video);

  return 0;
}
// Copyright (C) 2009 Chris Double. All Rights Reserved.
// The original author of this code can be contacted at: chris.double@double.co.nz
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
