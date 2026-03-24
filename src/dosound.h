/*
 * -------------
 *  Dark Oberon
 * -------------
 *
 * Copyright (C) 2002 - 2005 Valeria Sventova, Jiri Krejsa, Peter Knut,
 *                           Martin Kosalko, Marian Cerny, Michal Kral
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (see docs/gpl.txt) as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */

/**
 *  @file dosound.h
 *
 *  Sound declarations and methods (FMOD Ex 4.x backend).
 */

#ifndef __dosound_h__
#define __dosound_h__


#include "cfg.h"
#include "doalloc.h"

#include "dosimpletypes.h"


//=========================================================================
// Definitions
//=========================================================================

#define SND_MAX_PLAYBACKS   2
#define SND_MAX_CHANNELS    64
#define SND_MIXRATE         32000


enum TSOUND_FORMAT {
  SF_WAV,
  SF_MP2,
  SF_MP3,
  SF_OGG,
  SF_RAW,
  SF_MOD,
  SF_S3M,
  SF_XM,
  SF_IT,
  SF_MID,
  SF_RMI,
  SF_SGT
};


enum TVOLUME_TYPE {
  VT_NONE,
  VT_NORMAL,
  VT_ABSOLUTE
};


//=========================================================================
// Sounds
//=========================================================================

#if SOUND

struct FMOD_SOUND;
struct FMOD_CHANNEL;

class TSOUND {
public:
  char *id;
  TSOUND_FORMAT format;
  T_BYTE volume;
  TVOLUME_TYPE vol_type;
  bool loop;

  virtual void Play() = 0;
  virtual void Stop() = 0;

  virtual void SetVolume(T_BYTE vol) = 0;
  virtual void SetVolumeAbsolute(T_BYTE vol) = 0;
  virtual void SetLoop(bool lp) = 0;

  virtual bool IsPlaying() = 0;

  TSOUND() { id = NULL; format = SF_WAV;  volume = 0; vol_type = VT_NONE; loop = false; }
  virtual ~TSOUND() { if (id) delete[] id; }
};


class TCHANNEL : public TSOUND {
public:
  FMOD_CHANNEL *fch;

  virtual void SetVolume(T_BYTE vol);
  virtual void SetVolumeAbsolute(T_BYTE vol);

  virtual bool IsPlaying();

  TCHANNEL():TSOUND() { fch = NULL; }
  virtual ~TCHANNEL() {};

protected:
  virtual void _SetVolume();
};


class TSAMPLE : public TCHANNEL {
public:
  FMOD_SOUND *sample;

  bool Load(char *data, int size);
  void SetMaxPlaybacks(int max);

  virtual void Play();
  virtual void Stop();
  virtual void SetLoop(bool lp);

  TSAMPLE():TCHANNEL() { sample = NULL; }
  virtual ~TSAMPLE();

protected:
  void _SetVolume();
};


class TSTREAM : public TCHANNEL {
public:
  FMOD_SOUND *stream;
  char *stream_mem;
  int stream_mem_size;

  bool Load(const char *file_name, int seek, int size);

  virtual void Play();
  virtual void Stop();
  virtual void SetLoop(bool lp);

  TSTREAM():TCHANNEL() { stream = NULL; stream_mem = NULL; stream_mem_size = 0; }
  virtual ~TSTREAM();
};


class TMODULE : public TSOUND {
public:
  FMOD_SOUND *mod;
  FMOD_CHANNEL *fch;

  bool Load(char *data, int size);

  virtual void Play();
  virtual void Stop();

  virtual void SetVolume(T_BYTE vol);
  virtual void SetVolumeAbsolute(T_BYTE vol);
  virtual void SetLoop(bool lp);

  virtual bool IsPlaying();

  TMODULE():TSOUND() { mod = NULL; fch = NULL; }
  virtual ~TMODULE();
};


bool InitSound(void);
void FmodUpdate(void);
void FmodShutdown(void);
void FmodApplySfxMasterVolume(T_BYTE vol_menu_0_100);

#endif // #if SOUND

#endif // __dosound_h__
