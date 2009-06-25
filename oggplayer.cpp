// Copyright (C) 2009, Chris Double. All Rights Reserved.
// See the license at the end of this file.
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <oggplay/oggplay.h>
#include <oggplay/oggplay_tools.h>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <SDL/SDL.h>

extern "C" {
#include <sydney_audio.h>
}

#define UNSELECTED -2

using namespace std;
using namespace boost;
using namespace boost::posix_time;

// Helper function to make creating and assigning shared pointers less verbose
template <class T>
shared_ptr<T> msp(T* t) {
  return shared_ptr<T>(t);
}

// Wrap some of the SDL functionality to help manage resources
class SDL {
  public:
    SDL(unsigned long flags = 0) : init_flags(flags), initialized(false), use_sdl_yuv(false) { 
    }

    ~SDL() {
      if (initialized) {
        yuv_surface.reset();
        SDL_Quit();
      }
    }

    shared_ptr<SDL_Surface> setVideoMode(int width,
                                         int height,
                                         unsigned long flags) {
      int r = SDL_Init(SDL_INIT_VIDEO | init_flags);
      assert(r == 0);
      initialized = true;
      return shared_ptr<SDL_Surface>(SDL_SetVideoMode(width, height, 32, flags),
                                     SDL_FreeSurface);
    }

    bool use_sdl_yuv;
    shared_ptr<SDL_Overlay> yuv_surface;

  private:
    unsigned long init_flags;
    bool initialized;
};

// The SDL routines are accessible globally
SDL gSDL;

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

class KateTrack : public Track {
  public:
    std::string mLanguage;
    std::string mCategory;

  public:
    KateTrack(shared_ptr<OggPlay> player, int index, const std::string &language, const std::string &category) : 
      Track(player, OGGZ_CONTENT_KATE, index), mLanguage(language), mCategory(category) { }

    virtual string toString() const {
      ostringstream str;
      str << mIndex << ": Kate language \"" << mLanguage << "\" category \"" << mCategory << "\"";
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
  return msp(new TheoraTrack(player, index, static_cast<float>(num) / denom));
}

shared_ptr<Track> handle_vorbis_metadata(shared_ptr<OggPlay> player, int index) {
  int rate, channels;
  int r = oggplay_get_audio_samplerate(player.get(), index, &rate);
  assert(r == E_OGGPLAY_OK);
  r = oggplay_get_audio_channels(player.get(), index, &channels);
  assert(r == E_OGGPLAY_OK);

  // What does this do??? Seems to be needed for audio to sync with system clock
  // and the value comes from the oggplay examples.
  oggplay_set_offset(player.get(), index, 250);

  return msp(new VorbisTrack(player,index, rate, channels));
}

shared_ptr<Track> handle_kate_metadata(shared_ptr<OggPlay> player, int index) {
  const char *language = "", *category = "";
  int r = oggplay_get_kate_language(player.get(), index, &language);
  assert(r == E_OGGPLAY_OK);
  r = oggplay_get_kate_category(player.get(), index, &category);
  assert(r == E_OGGPLAY_OK);
  return msp(new KateTrack(player, index, language, category));
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

      case OGGZ_CONTENT_KATE:
        *out++ = handle_kate_metadata(player, i);
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

// Return a particular track by index
template <class TrackType, class InputIterator>
shared_ptr<TrackType> get_track(int trackidx, InputIterator first, InputIterator last) {
  while (first != last) {
    shared_ptr<TrackType> track(dynamic_pointer_cast<TrackType>(*first));
    if (trackidx == UNSELECTED) {
      if (track != 0) return track; /* unselected ? use the first one we find */
    }
    else if (trackidx >= 0) {
      if (!trackidx--) return track; /* use a track by index - returns NULL if not the correct type */
    }
    ++first;
  }
  return shared_ptr<TrackType>();
}

int decode_thread(void* p);

// Encapsulates the decode thread and seeking operations.
class Decoder {
public:
  Decoder(shared_ptr<OggPlay> player)
    : mThread(0),
      mPlayer(player),
      mCompleted(false)
  {
  }
  
  ~Decoder() {
  }
  
  bool start() {
    assert(!mThread);
    setCompleted(false);
    mThread = SDL_CreateThread(decode_thread, this);
    return mThread != 0;  
  }
  
  bool stop() {
    setCompleted(true);
    // We need to release a buffer, as oggplay_step_decode() could be blocked
    // waiting for a free buffer.
    OggPlayCallbackInfo** info = oggplay_buffer_retrieve_next(mPlayer.get());
    if (info) {
      oggplay_buffer_release(mPlayer.get(), info);
    }
    SDL_WaitThread(mThread, NULL);
    mThread = 0;
  }

  OggPlay* getPlayer() {
    return mPlayer.get();
  }

  bool isCompleted() {
    return mCompleted;
  }

  void seek(long target) {  
    stop();
    oggplay_seek(mPlayer.get(), target);
    start();
    mJustSeeked = true;
  }
  

  void setCompleted(bool c) {
    mCompleted = c;
  }

  bool justSeeked() {
    if (mJustSeeked) {
      mJustSeeked = false;
      return true;
    }
    return false;
  }

private:
  SDL_Thread* mThread;
  shared_ptr<OggPlay> mPlayer;
  bool mCompleted;
  bool mJustSeeked;
};

// Decoding thread. Running the decode loop in a seperate thread avoids the
// issue where some frames take longer to decode than the time for a frame
// to be displayed (HD video for example).
int decode_thread(void* p) {
  Decoder* d = (Decoder*)p;
  OggPlay* player = d->getPlayer();

  // E_OGGPLAY_CONTINUE       = One frame decoded and put in buffer list
  // E_OGGPLAY_USER_INTERRUPT = One frame decoded, buffer list is now full
  // E_OGGPLAY_TIMEOUT        = No frames decoded, timed out
  int r = E_OGGPLAY_TIMEOUT;
  while (!d->isCompleted() &&
         (r == E_OGGPLAY_TIMEOUT ||
         r == E_OGGPLAY_USER_INTERRUPT ||
         r == E_OGGPLAY_CONTINUE)) {
    r = oggplay_step_decoding(player);
  }
  d->setCompleted(true);
  return 0;
}

// Displays onscreen a bar which indicates how far through the media playback
// has reached. You can click on the seek bar to seek. Seek bar is visible when
// the mouse hovers over it, or for a number of seconds after last mouse motion.
class SeekBar {
public:
  SeekBar(shared_ptr<OggPlay> player,
          Decoder& decoder,
          time_duration visibleDuration,
          int height,
          int padding,
          int border)
    : mPlayer(player),
      mDecoder(decoder),
      mStartTimeMs(0),
      mEndTimeMs(-1),
      mCurrentTimeMs(0),
      mVisibleDuration(visibleDuration),
      mHeight(height),
      mPadding(padding),
      mBorder(border)
  {
    updateHideTime();
  }
  
  // Sets the time of the most recently played frame.
  void setCurrentTime(int64_t timeMs) {
    mCurrentTimeMs = timeMs;
  }
  
  void setStartTime(int64_t timeMs) {
    mStartTimeMs = timeMs;
  }
  
  void draw(shared_ptr<SDL_Surface>& screen) {
    if (!isVisible(screen) || !screen) {
      return;
    }
    
    SDL_Rect border = getBorderRect(screen);
    unsigned white = SDL_MapRGB(screen->format, 255, 255, 255);
    int err = SDL_FillRect(screen.get(), &border, white);
    assert(err == 0);
    
    SDL_Rect background = getBackgroundRect(screen);
    unsigned black = SDL_MapRGB(screen->format, 0, 0, 0);
    err = SDL_FillRect(screen.get(), &background, black);
    assert(err == 0);
    
    SDL_Rect progress = getProgressRect(screen);
    unsigned gray = SDL_MapRGB(screen->format, 0xd6, 0xd6, 0xd6);
    err = SDL_FillRect(screen.get(), &progress, gray);
    
    assert(err == 0);
  }
  
  // Returns true if handles/consumes the event, otherwise false.
  bool handleEvent(shared_ptr<SDL_Surface> screen, SDL_Event const& event) {
    if (event.type == SDL_MOUSEMOTION) {
      updateHideTime();
      return true;
    } else if (event.type == SDL_MOUSEBUTTONDOWN &&
               event.button.button == SDL_BUTTON_LEFT) {
      int x = event.button.x;
      int y = event.button.y;
      SDL_Rect background = getBackgroundRect(screen);
      SDL_Rect progress = getProgressRect(screen);
      if (isInside(x, y, background)) {
        double progressWidth = background.w - 2 * mBorder;
        double proportion = (x - progress.x) / progressWidth;
        double duration = mEndTimeMs - mStartTimeMs;
        double seekTime = duration * proportion;
        int64_t seekTimeMs = mStartTimeMs + (int64_t)seekTime;
        mDecoder.seek(seekTimeMs);
        return true;
      }
      return false;
    }
    return false;
  }  
  
private:

  SDL_Rect getBorderRect(shared_ptr<SDL_Surface> screen) {
    SDL_Rect border;
    border.x = mPadding;
    border.y = screen->h - mPadding - mHeight;
    border.w = screen->w - mPadding * 2;
    border.h = mHeight;
    return border;
  }

  SDL_Rect getBackgroundRect(shared_ptr<SDL_Surface> screen) {
    SDL_Rect background;
    background.x = mPadding + mBorder;
    background.y = screen->h - mPadding - mHeight + mBorder;
    background.w = screen->w - 2 * mPadding - 2 * mBorder;
    background.h = mHeight - 2 * mBorder;
    return background;
  }

  SDL_Rect getProgressRect(shared_ptr<SDL_Surface> screen) {
    if (mEndTimeMs == -1) {
      // Due to a bug in liboggplay, we can't call this before the frames
      // start coming in, else we'll sometimes deadlock on some files.
      mEndTimeMs = oggplay_get_duration(mPlayer.get());
    }
    double duration = mEndTimeMs - mStartTimeMs;
    double position = mCurrentTimeMs - mStartTimeMs;
    SDL_Rect background = getBackgroundRect(screen);
    double maxWidth = background.w - 2 * mBorder;
    SDL_Rect progress;
    progress.x = background.x + mBorder;
    progress.y = background.y + mBorder;
    progress.h = background.h - 2 * mBorder;
    progress.w = max(1, (int)(maxWidth * position / duration));
    return progress;
  }

  bool isInside(int x, int y, const SDL_Rect& rect) {
    return x > rect.x &&
           x < rect.x + rect.w &&
           y > rect.y &&
           y < rect.y + rect.h;
  }

  bool isVisible(shared_ptr<SDL_Surface> screen) {
    int x=0, y=0;
    SDL_GetMouseState(&x, &y);
    SDL_Rect background = getBackgroundRect(screen);
    return second_clock::local_time() < mHideTime ||
           isInside(x, y, background);
  }

  void updateHideTime() {
    mHideTime = second_clock::local_time() + mVisibleDuration;
  }

  shared_ptr<OggPlay> mPlayer;
  Decoder& mDecoder;

  int64_t mStartTimeMs;
  int64_t mEndTimeMs;
  int64_t mCurrentTimeMs;

  // Height of the seek bar, in pixels, including borders, background,
  // and progress bar.
  int mHeight;
  
  // Number of pixels between bottom and sides of screen and the seek bar.
  int mPadding;
  
  // Thickness of borders in pixels.
  int mBorder;
  
  ptime mHideTime;
  time_duration mVisibleDuration;
  
};

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

// Process the video data provided by liboggplay. Currently using liboggplay's
// yuv2rgb routines to test the speed. I'll later provide a switch to use
// SDL's routines to compare.
void handle_video_data(shared_ptr<SDL_Surface>& screen, 
                       SeekBar& seekBar,
                       shared_ptr<Track> video, 
                       OggPlayDataHeader* header) {
  shared_ptr<OggPlay> player(video->mPlayer);
  int y_width, y_height;
  int r = oggplay_get_video_y_size(player.get(), video->mIndex, &y_width, &y_height);
  assert(r == E_OGGPLAY_OK);

  int uv_width, uv_height;
  r = oggplay_get_video_uv_size(player.get(), video->mIndex, &uv_width, &uv_height);
  assert(r == E_OGGPLAY_OK);

  if (!screen) {
    screen = gSDL.setVideoMode(y_width, y_height, SDL_DOUBLEBUF);
    assert(screen);
  }

  OggPlayVideoData* data = oggplay_callback_info_get_video_data(header);

  if (gSDL.use_sdl_yuv) {
    if (!gSDL.yuv_surface) {
      gSDL.yuv_surface = shared_ptr<SDL_Overlay>(
                                                 SDL_CreateYUVOverlay(y_width,
                                                                      y_height,
                                                                      SDL_YV12_OVERLAY,
                                                                      screen.get()),
                                                 SDL_FreeYUVOverlay);
      assert(gSDL.yuv_surface);
    }

    r = SDL_LockYUVOverlay(gSDL.yuv_surface.get());
    assert(r == 0);

    memcpy(gSDL.yuv_surface->pixels[0], data->y, gSDL.yuv_surface->pitches[0] * y_height);
    memcpy(gSDL.yuv_surface->pixels[2], data->u, gSDL.yuv_surface->pitches[2] * uv_height);
    memcpy(gSDL.yuv_surface->pixels[1], data->v, gSDL.yuv_surface->pitches[1] * uv_height);

    SDL_UnlockYUVOverlay(gSDL.yuv_surface.get());

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = y_width;
    rect.h = y_height;
    SDL_DisplayYUVOverlay(gSDL.yuv_surface.get(), &rect);
  } else {
    OggPlayYUVChannels yuv;
    yuv.ptry = data->y;
    yuv.ptru = data->u;
    yuv.ptrv = data->v;
    yuv.uv_width = uv_width;
    yuv.uv_height = uv_height;
    yuv.y_width = y_width;
    yuv.y_height = y_height;

    int size = y_width * y_height * 4;

    // The array is being allocated here on each frame. Should really do this once and
    // reallocate when it changes.
    scoped_array<unsigned char> buffer(new unsigned char[size]);
    assert(buffer);

    OggPlayRGBChannels rgb;
    rgb.ptro = buffer.get();
    rgb.rgb_width = y_width;
    rgb.rgb_height = y_height;

#if SDL_BYTE_ORDER == SDL_BIG_ENDIAN
    oggplay_yuv2argb(&yuv, &rgb);
#else
    oggplay_yuv2bgra(&yuv, &rgb);
#endif

    shared_ptr<SDL_Surface> rgb_surface( 
                                        SDL_CreateRGBSurfaceFrom(buffer.get(),
                                                                 y_width,
                                                                 y_height,
                                                                 32,
                                                                 4 * y_width,
                                                                 0, 0, 0, 0),
                                        SDL_FreeSurface);
    assert(rgb_surface);

    r = SDL_BlitSurface(rgb_surface.get(), 
                        NULL,
                        screen.get(),
                        NULL);
    assert(r == 0);
  }

  seekBar.draw(screen);

  r = SDL_Flip(screen.get());
  assert(r == 0);
}

// Process the RGB(A) video data provided by liboggplay.
void handle_overlay_data(shared_ptr<SDL_Surface>& screen, 
                         SeekBar& seekBar,
                         shared_ptr<Track> video, 
                         OggPlayDataHeader* header) {
  shared_ptr<OggPlay> player(video->mPlayer);
  OggPlayOverlayData* data = oggplay_callback_info_get_overlay_data(header);

  int width = data->width, height = data->height;

  if (!screen) {
    screen = gSDL.setVideoMode(width, height, SDL_DOUBLEBUF);
    assert(screen);
  }

  void *buffer = data->rgb ? data->rgb : data->rgba;
  shared_ptr<SDL_Surface> rgb_surface( 
    SDL_CreateRGBSurfaceFrom(buffer,
                             width,
                             height,
                             32,
                             4 * width,
                             0, 0, 0, 0),
    SDL_FreeSurface);
  assert(rgb_surface);

  int r = SDL_BlitSurface(rgb_surface.get(), 
                      NULL,
                      screen.get(),
                      NULL);
  assert(r == 0);

  seekBar.draw(screen);

  r = SDL_Flip(screen.get());
  assert(r == 0);
}

// Process the text from a Kate stream (when not already overlaid on video).
void handle_text_data( shared_ptr<KateTrack> kate, 
                       OggPlayTextData* header) {
  cout << (const char*)header << endl;
}

// Handle key events. Return 'false' to exit the
// play loop.
bool handle_key_press(shared_ptr<SDL_Surface> screen, SDL_Event const& event) {
  if (event.key.keysym.sym == SDLK_ESCAPE)
    return false;
  else if(event.key.keysym.sym == SDLK_SPACE) 
    SDL_WM_ToggleFullScreen(screen.get());

  return true;
}

// Handle any SDL events. Returning 'false' will
// exit the play loop.
bool handle_sdl_event(shared_ptr<SDL_Surface> screen, SDL_Event const& event) {
  switch (event.type) {
    case SDL_KEYDOWN:
      return handle_key_press(screen, event);
    case SDL_QUIT:
      // Application window was closed.
      return false;
    default:
      return true;
  }
}

// Play the tracks. Exits when the longest track has completed playing
void play(shared_ptr<OggPlay> player, shared_ptr<VorbisTrack> audio, shared_ptr<TheoraTrack> video,
          shared_ptr<KateTrack> kate) {
  // Video Surface. We delay creating it until we've decoded some of the
  // video stream so we can get the width/height.
  shared_ptr<SDL_Surface> screen;
  bool have_sound = false;

  // Open an audio stream
  shared_ptr<sa_stream_t> sound(static_cast<sa_stream_t*>(NULL), sa_stream_destroy);

  if (audio) {
    sa_stream_t* s;
    int sr = sa_stream_create_pcm(&s,
                                  NULL,
                                  SA_MODE_WRONLY,
                                  SA_PCM_FORMAT_S16_NE,
                                  audio->mRate,
                                  audio->mChannels);
    assert(sr == SA_SUCCESS);
    sound.reset(s, sa_stream_destroy);

    sr = sa_stream_open(sound.get());
    //assert(sr == SA_SUCCESS);
    if (sr == SA_SUCCESS) {
      have_sound = true;
    }
    else {
      cerr << "Failed to open sound" << endl;
    }
  }

  int r = oggplay_use_buffer(player.get(), 20);
  assert(r == E_OGGPLAY_OK);

  // Event object for SDL
  SDL_Event event;

  // The time that we started playing - used for synching a/v against
  // the system clock.
  // TODO: sync vs audio clock
  ptime start(microsec_clock::universal_time());

  // Start the decoding loop in a background thread. The thread must
  // be stopped before this function is exited so that the player
  // object is not being used when it is deleted.
  Decoder decoder(player);

  SeekBar seekBar(player, decoder, seconds(5), 10, 10, 1);
  long first_frame_time = -1;

  if (!decoder.start())
    return;

  while (!decoder.isCompleted()) {
    while (SDL_PollEvent(&event) == 1) {
      if (!seekBar.handleEvent(screen, event) &&
          !handle_sdl_event(screen, event)) {
        decoder.setCompleted(true);
        break;
      }
    }
    if (decoder.isCompleted())
      break;

    OggPlayCallbackInfo** info = oggplay_buffer_retrieve_next(player.get());
    if (!info)
     continue;

    int num_tracks = oggplay_get_num_tracks(player.get());
    assert(!audio || audio && audio->mIndex < num_tracks);
    assert(!video || video && video->mIndex < num_tracks);
    assert(!kate || kate && kate->mIndex < num_tracks);

    if (have_sound && audio && oggplay_callback_info_get_type(info[audio->mIndex]) == OGGPLAY_FLOATS_AUDIO) {
      OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[audio->mIndex]);
      double time = oggplay_callback_info_get_presentation_time(headers[0]) / 1000.0;
      int required = oggplay_callback_info_get_required(info[audio->mIndex]);
      for (int i=0; i<required;++i) {
        int size = oggplay_callback_info_get_record_size(headers[i]);
        OggPlayAudioData* data = oggplay_callback_info_get_audio_data(headers[i]);
        handle_audio_data(sound, data, size * audio->mChannels);
      }
    }
    
    if (video || kate) {
      int idx = video ? video->mIndex : kate->mIndex;
      int required = oggplay_callback_info_get_required(info[idx]);
      if (required > 0) {
        int type = oggplay_callback_info_get_type(info[idx]);
        if (type == OGGPLAY_YUV_VIDEO || type == OGGPLAY_RGBA_VIDEO) {
          OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[idx]);
          long video_ms = oggplay_callback_info_get_presentation_time(headers[0]);

          if (first_frame_time == -1) {
            first_frame_time = video_ms;
            seekBar.setStartTime(first_frame_time);
          }
          seekBar.setCurrentTime(video_ms);

          if (decoder.justSeeked()) {
            first_frame_time = video_ms;
            start = microsec_clock::universal_time();
          }

          ptime now(microsec_clock::universal_time());
          time_duration duration(now - start);
          long system_ms = duration.total_milliseconds();
          long diff = video_ms - first_frame_time - system_ms;

          if (diff > 0) {
            // Need to pause for a bit until it's time for the video frame to appear
            SDL_Delay(diff);
          }

          // Note that we pass the screen by reference here to allow it to be changed if the
          // video changes size.
          shared_ptr<Track> track = video;
          if (!track) track = kate;
          if (type == OGGPLAY_YUV_VIDEO) {
            handle_video_data(screen, seekBar, track, headers[0]);
          }
          else if (type == OGGPLAY_RGBA_VIDEO) {
            printf("handle_overlay_data()\n");
            handle_overlay_data(screen, seekBar, track, headers[0]);
          }
        }
      }
    }

    if (kate && oggplay_callback_info_get_type(info[kate->mIndex]) == OGGPLAY_KATE) {
      OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[kate->mIndex]);
      double time = oggplay_callback_info_get_presentation_time(headers[0]) / 1000.0;
      int required = oggplay_callback_info_get_required(info[kate->mIndex]);
      for (int i=0; i<required;++i) {
        OggPlayTextData* data = oggplay_callback_info_get_text_data(headers[i]);
        handle_text_data(kate, data);
      }
    }
    
    oggplay_buffer_release(player.get(), info);
  } 
 
  // The decoding thread can be blocked in the call to oggplay_step_decoding.
  // The following call will cause the thread blocked on that function to unblock
  // and exit the decoding loop. We then join to the thread to ensure it has
  // completed before we return so that player object can safely be deleted.
  oggplay_prepare_for_close(player.get());
  decoder.stop();
}

void usage() {
    cout << "Usage: oggplayer [options] <filename>" << endl;
    cout << "  --sdl-yuv            Use SDL's YUV conversion routines" << endl;
    cout << "  --video-track <n>    Select which video track to use (-1 to disable)" << endl;
    cout << "  --audio-track <n>    Select which audio track to use (-1 to disable)" << endl;
    cout << "  --kate-track <n>     Select which kate track to use (-1 to disable)" << endl;
    exit(EXIT_FAILURE);
}

static int parse_track_index_parameter(int argc, const char *argv[], int &n, const char *name, int &idx)
{
  if (strcmp(argv[n], name) == 0) {
    if (idx == UNSELECTED) {
      char *end = NULL;
      if (n == argc-1 || (idx=strtol(argv[n+1], &end, 10), *end)) usage(); else ++n;
      if (idx < 0) idx = -1;
    }
    else {
      usage();
    }
    return 0;
  }
  return 1;
}

int main(int argc, const char* argv[]) {
  int video_track = UNSELECTED, audio_track = UNSELECTED, kate_track = UNSELECTED;

  if (argc < 2) {
    usage();
  }

  char* path = NULL;
  for (int n=1; n<argc; ++n) {
    if (argv[n][0] == '-') {
      if (strcmp(argv[n], "--sdl-yuv") == 0) {
        gSDL.use_sdl_yuv = true;
      }
      else if (!parse_track_index_parameter(argc, argv, n, "--video-track", video_track)) {
      }
      else if (!parse_track_index_parameter(argc, argv, n, "--audio-track", audio_track)) {
      }
      else if (!parse_track_index_parameter(argc, argv, n, "--kate-track", kate_track)) {
      }
      else {
        usage();
      }
    }
    else {
      if (path) {
        cerr << "Only one stream may be specified" << endl;
      }
      else {
        path = (char*)argv[n]; // TODO: liboggplay bug doesn't take const char*
      }
    }
  }

  if (!path) {
    usage();
  }

  OggPlayReader* reader = 0;
  if (strncmp(path, "http://", 7) == 0) 
    reader = oggplay_tcp_reader_new(path, NULL, 0);
  else
    reader = oggplay_file_reader_new(path);

  assert(reader);

  shared_ptr<OggPlay> player(oggplay_open_with_reader(reader), oggplay_close);
  assert(player);

  vector<shared_ptr<Track> > tracks;
  load_metadata(player, back_inserter(tracks));
  for_each(tracks.begin(), tracks.end(), dump_track);

  shared_ptr<TheoraTrack> video(get_track<TheoraTrack>(video_track, tracks.begin(), tracks.end()));
  shared_ptr<VorbisTrack> audio(get_track<VorbisTrack>(audio_track, tracks.begin(), tracks.end()));
  shared_ptr<KateTrack> kate(get_track<KateTrack>(kate_track, tracks.begin(), tracks.end()));

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

  if (kate) {
    kate->setActive();
    if (video) {
      oggplay_convert_video_to_rgb(player.get(), video->mIndex, 1, 0);
      oggplay_overlay_kate_track_on_video(player.get(), kate->mIndex, video->mIndex);
    }
    else {
      oggplay_set_kate_tiger_rendering(player.get(), kate->mIndex, 1, 0, 640, 480);
    }
    if (!audio && !video)
      oggplay_set_callback_period(player.get(), kate->mIndex, 40);

    cout << "  " << kate->toString() << endl;
  }


  play(player, audio, video, kate);

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
