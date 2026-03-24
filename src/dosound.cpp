/*
 * Dark Oberon — audio implementation (SDL2_mixer).
 */

#include "cfg.h"
#include "doalloc.h"
#include "dologs.h"
#include "dosound.h"

#if SOUND

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <string.h>
#include <stdio.h>

#include "doconfig.h"

/* Only applied to TSAMPLE (SFX), not music streams/modules. */
static float s_sfx_master = 1.0f;
static bool s_audio_ready = false;

/* Single SDL_mixer music track (menu / game / mod). */
static Mix_Music *s_active_music = NULL;

static int vol_byte_to_mix(T_BYTE v)
{
  int x = (int)((unsigned)v * 128 / 255);
  if (x < 0) x = 0;
  if (x > MIX_MAX_VOLUME) x = MIX_MAX_VOLUME;
  return x;
}

static void music_halt_if(Mix_Music *which)
{
  if (s_active_music == which) {
    Mix_HaltMusic();
    s_active_music = NULL;
  }
}

static void music_play(Mix_Music *mus, bool loop)
{
  if (!mus)
    return;
  Mix_HaltMusic();
  s_active_music = mus;
  if (Mix_PlayMusic(mus, loop ? -1 : 0) != 0) {
    Warning(LogMsg("Mix_PlayMusic failed: %s", Mix_GetError()));
    s_active_music = NULL;
  }
}


bool InitSound(void)
{
  unsigned mix_flags;

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    Critical(LogMsg("SDL audio init failed: %s", SDL_GetError()));
    return false;
  }

  mix_flags = (unsigned)MIX_INIT_MP3 | (unsigned)MIX_INIT_OGG | (unsigned)MIX_INIT_MOD;
  if (((unsigned)Mix_Init((int)mix_flags) & mix_flags) != mix_flags)
    Warning(LogMsg("Mix_Init: %s (some compressed/mod music formats may be unavailable)", Mix_GetError()));

  if (Mix_OpenAudio(SND_MIXRATE, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
    Critical(LogMsg("Mix_OpenAudio failed: %s", Mix_GetError()));
    Mix_Quit();
    return false;
  }

  Mix_AllocateChannels(SND_MAX_CHANNELS);
  s_audio_ready = true;
  Info(LogMsg("SDL2_mixer init OK (rate=%d, channels=%d)", SND_MIXRATE, SND_MAX_CHANNELS));
  return true;
}


void FmodUpdate(void)
{
  /* SDL_mixer does not need a periodic update like FMOD_System_Update. */
}


void FmodShutdown(void)
{
  if (!s_audio_ready)
    return;

  Mix_HaltChannel(-1);
  Mix_HaltMusic();
  s_active_music = NULL;
  Mix_CloseAudio();
  Mix_Quit();
  s_audio_ready = false;
}


void FmodApplySfxMasterVolume(T_BYTE vol)
{
  T_BYTE scaled;
  float fv;

  if (!s_audio_ready)
    return;

  scaled = (T_BYTE)(vol * 2.55f);
  fv = ((float)scaled * (float)config.snd_master_volume) / (100.0f * 255.0f);
  if (fv < 0.0f)
    fv = 0.0f;
  if (fv > 1.0f)
    fv = 1.0f;

  s_sfx_master = fv;
}


void TCHANNEL::_SetVolume(void)
{
  if (snd_ch < 0 || vol_type == VT_NONE)
    return;
  Mix_Volume(snd_ch, vol_byte_to_mix(volume));
}


void TSAMPLE::_SetVolume(void)
{
  int mixv;
  int base;

  if (snd_ch < 0)
    return;
  /* Data file does not set per-sample volume; VT_NONE must still obey SFX master. */
  base = (vol_type == VT_NONE) ? vol_byte_to_mix((T_BYTE)255) : vol_byte_to_mix(volume);
  mixv = (int)((float)base * s_sfx_master);
  if (mixv < 0) mixv = 0;
  if (mixv > MIX_MAX_VOLUME) mixv = MIX_MAX_VOLUME;
  Mix_Volume(snd_ch, mixv);
}


void TSAMPLE::RefreshSfxVolume(void)
{
  if (snd_ch >= 0 && Mix_Playing(snd_ch))
    _SetVolume();
}


void TCHANNEL::SetVolume(T_BYTE vol)
{
  volume = vol;
  vol_type = VT_NORMAL;
  _SetVolume();
}


void TCHANNEL::SetVolumeAbsolute(T_BYTE vol)
{
  volume = vol;
  vol_type = VT_ABSOLUTE;
  _SetVolume();
}


bool TCHANNEL::IsPlaying(void)
{
  if (snd_ch < 0)
    return false;
  return Mix_Playing(snd_ch) != 0;
}


TSAMPLE::~TSAMPLE(void)
{
  Stop();
  if (sample) {
    Mix_FreeChunk(sample);
    sample = NULL;
  }
}


bool TSAMPLE::Load(char *data, int size)
{
  SDL_RWops *rw;

  if (!s_audio_ready || !data || size <= 0)
    return false;

  if (sample) {
    Mix_FreeChunk(sample);
    sample = NULL;
  }
  Stop();

  rw = SDL_RWFromMem(data, size);
  if (!rw)
    return false;
  sample = Mix_LoadWAV_RW(rw, 1);
  if (!sample) {
    Warning(LogMsg("TSAMPLE::Load Mix_LoadWAV_RW failed: %s (size=%d)", Mix_GetError(), size));
    return false;
  }

  return true;
}


void TSAMPLE::SetMaxPlaybacks(int max)
{
  (void)max;
}


void TSAMPLE::Play(void)
{
  int ch;
  int loops;

  if (!sample || !s_audio_ready)
    return;

  Stop();

  loops = loop ? -1 : 0;
  ch = Mix_PlayChannel(-1, sample, loops);
  if (ch < 0) {
    Warning(LogMsg("TSAMPLE::Play Mix_PlayChannel failed: %s", Mix_GetError()));
    snd_ch = SND_CH_NONE;
    return;
  }

  snd_ch = ch;
  _SetVolume();
}


void TSAMPLE::Stop(void)
{
  if (snd_ch >= 0) {
    Mix_HaltChannel(snd_ch);
    snd_ch = SND_CH_NONE;
  }
}


void TSAMPLE::SetLoop(bool lp)
{
  loop = lp;
  /* Loop count is fixed at Play(); restart if playing. */
  if (snd_ch >= 0 && Mix_Playing(snd_ch)) {
    T_BYTE sv = volume;
    TVOLUME_TYPE st = vol_type;
    Play();
    volume = sv;
    vol_type = st;
    _SetVolume();
  }
}


TSTREAM::~TSTREAM(void)
{
  Stop();
  if (stream) {
    Mix_FreeMusic(stream);
    stream = NULL;
  }
  if (stream_mem) {
    delete[] stream_mem;
    stream_mem = NULL;
    stream_mem_size = 0;
  }
}


bool TSTREAM::Load(const char *file_name, int seek, int size)
{
  FILE *fp;
  size_t n;
  SDL_RWops *rw;

  if (!s_audio_ready || !file_name || size <= 0)
    return false;

  Stop();
  if (stream) {
    Mix_FreeMusic(stream);
    stream = NULL;
  }
  if (stream_mem) {
    delete[] stream_mem;
    stream_mem = NULL;
    stream_mem_size = 0;
  }

  fp = fopen(file_name, "rb");
  if (!fp)
    return false;
  stream_mem = NEW char[size];
  if (!stream_mem) {
    fclose(fp);
    return false;
  }
  if (fseek(fp, seek, SEEK_SET) != 0) {
    fclose(fp);
    delete[] stream_mem;
    stream_mem = NULL;
    return false;
  }
  n = fread(stream_mem, 1, (size_t)size, fp);
  fclose(fp);
  if (n != (size_t)size) {
    delete[] stream_mem;
    stream_mem = NULL;
    return false;
  }
  stream_mem_size = size;

  rw = SDL_RWFromMem(stream_mem, stream_mem_size);
  if (!rw) {
    delete[] stream_mem;
    stream_mem = NULL;
    stream_mem_size = 0;
    return false;
  }
  stream = Mix_LoadMUS_RW(rw, 1);
  if (!stream) {
    Warning(LogMsg("TSTREAM::Load Mix_LoadMUS_RW failed: %s (file=%s, size=%d)", Mix_GetError(), file_name, size));
    delete[] stream_mem;
    stream_mem = NULL;
    stream_mem_size = 0;
    return false;
  }

  return true;
}


void TSTREAM::_SetVolume(void)
{
  if (vol_type == VT_NONE)
    return;
  if (s_active_music == stream)
    Mix_VolumeMusic(vol_byte_to_mix(volume));
}


void TSTREAM::Play(void)
{
  if (!stream || !s_audio_ready)
    return;

  Stop();
  music_play(stream, loop);
  _SetVolume();
}


void TSTREAM::Stop(void)
{
  music_halt_if(stream);
}


bool TSTREAM::IsPlaying(void)
{
  return stream && s_active_music == stream && Mix_PlayingMusic() != 0;
}


void TSTREAM::SetLoop(bool lp)
{
  loop = lp;
  if (IsPlaying()) {
    T_BYTE sv = volume;
    TVOLUME_TYPE st = vol_type;
    Play();
    volume = sv;
    vol_type = st;
  }
}


TMODULE::~TMODULE(void)
{
  Stop();
  if (mod) {
    Mix_FreeMusic(mod);
    mod = NULL;
  }
  if (mod_mem) {
    delete[] mod_mem;
    mod_mem = NULL;
    mod_mem_size = 0;
  }
}


bool TMODULE::Load(char *data, int size)
{
  SDL_RWops *rw;

  if (!s_audio_ready || !data || size <= 0)
    return false;

  Stop();
  if (mod) {
    Mix_FreeMusic(mod);
    mod = NULL;
  }
  if (mod_mem) {
    delete[] mod_mem;
    mod_mem = NULL;
    mod_mem_size = 0;
  }

  mod_mem = NEW char[size];
  if (!mod_mem)
    return false;
  memcpy(mod_mem, data, (size_t)size);
  mod_mem_size = size;

  rw = SDL_RWFromMem(mod_mem, mod_mem_size);
  if (!rw) {
    delete[] mod_mem;
    mod_mem = NULL;
    mod_mem_size = 0;
    return false;
  }
  mod = Mix_LoadMUS_RW(rw, 1);
  if (!mod) {
    Warning(LogMsg("TMODULE::Load Mix_LoadMUS_RW failed: %s (size=%d)", Mix_GetError(), size));
    delete[] mod_mem;
    mod_mem = NULL;
    mod_mem_size = 0;
    return false;
  }

  return true;
}


void TMODULE::Play(void)
{
  if (!mod || !s_audio_ready)
    return;

  Stop();
  music_play(mod, loop);
  if (s_active_music == mod)
    Mix_VolumeMusic(vol_byte_to_mix(volume));
}


void TMODULE::Stop(void)
{
  music_halt_if(mod);
}


void TMODULE::SetVolume(T_BYTE vol)
{
  volume = vol;
  if (s_active_music == mod)
    Mix_VolumeMusic(vol_byte_to_mix(volume));
}


void TMODULE::SetVolumeAbsolute(T_BYTE vol)
{
  SetVolume(vol);
}


void TMODULE::SetLoop(bool lp)
{
  if (loop == lp)
    return;
  loop = lp;
  if (IsPlaying()) {
    T_BYTE sv = volume;
    Play();
    volume = sv;
  }
}


bool TMODULE::IsPlaying(void)
{
  return mod && s_active_music == mod && Mix_PlayingMusic() != 0;
}

#endif
