/*
 * Dark Oberon — audio implementation (FMOD Ex 4.x / libfmodex).
 */

#include "cfg.h"
#include "doalloc.h"
#include "dologs.h"
#include "dosound.h"

#if SOUND

#include <string.h>
#include <stdio.h>

#include "doconfig.h"
#include "fmodex_api.h"

static FMOD_SYSTEM *fmod_sys = NULL;
/* Matches FSOUND_SetSFXMasterVolume — only applied to TSAMPLE (SFX), not streams / music. */
static float fmod_sfx_master = 1.0f;

#define FMOD_MIN_VERSION_HEX 0x00044400u

static void apply_loop_mode(FMOD_SOUND *snd, bool lp)
{
  FMOD_MODE m = FMOD_DEFAULT;

  if (!snd)
    return;
  if (FMOD_Sound_GetMode(snd, &m) != FMOD_OK)
    return;
  m &= ~(FMOD_MODE)(FMOD_LOOP_NORMAL | FMOD_LOOP_OFF);
  m |= lp ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
  FMOD_Sound_SetMode(snd, m);
}


bool InitSound(void)
{
  unsigned int ver = 0;
  FMOD_RESULT r;

  r = FMOD_System_Create(&fmod_sys);
  if (r != FMOD_OK || !fmod_sys) {
    Critical("FMOD_System_Create failed");
    return false;
  }

  r = FMOD_System_GetVersion(fmod_sys, &ver);
  if (r != FMOD_OK || ver < FMOD_MIN_VERSION_HEX) {
    char buf[128];
    snprintf(buf, sizeof(buf), "FMOD Ex too old or missing (version 0x%08x, need >= 0x%08x)",
             ver, FMOD_MIN_VERSION_HEX);
    Critical(buf);
    FMOD_System_Release(fmod_sys);
    fmod_sys = NULL;
    return false;
  }

  r = FMOD_System_SetSoftwareFormat(fmod_sys, SND_MIXRATE, FMOD_SOUND_FORMAT_PCM16,
                                    0, 0, FMOD_DSP_RESAMPLER_DEFAULT);
  if (r != FMOD_OK) {
    Warning("FMOD_System_SetSoftwareFormat failed (continuing)");
  }

  r = FMOD_System_Init(fmod_sys, SND_MAX_CHANNELS, FMOD_INIT_NORMAL, NULL);
  if (r != FMOD_OK) {
    char buf[128];
    snprintf(buf, sizeof(buf), "FMOD_System_Init failed (result=%d)", (int)r);
    Critical(buf);
    FMOD_System_Release(fmod_sys);
    fmod_sys = NULL;
    return false;
  }

  Info(LogMsg("FMOD Ex init OK (version=0x%08x, sizeof(exinfo)=%d)", ver, (int)sizeof(FMOD_CREATESOUNDEXINFO)));

  {
    FMOD_CHANNELGROUP *master = NULL;
    if (FMOD_System_GetMasterChannelGroup(fmod_sys, &master) == FMOD_OK && master)
      FMOD_ChannelGroup_SetVolume(master, 1.0f);
  }

  return true;
}


void FmodUpdate(void)
{
  if (fmod_sys)
    FMOD_System_Update(fmod_sys);
}


void FmodShutdown(void)
{
  int i;

  if (!fmod_sys)
    return;
  /* Let the mixer drain (streams / channels) before release. */
  for (i = 0; i < 8; i++)
    FMOD_System_Update(fmod_sys);
  FMOD_System_Release(fmod_sys);
  fmod_sys = NULL;
}


void FmodApplySfxMasterVolume(T_BYTE vol)
{
  T_BYTE scaled;
  float fv;

  if (!fmod_sys)
    return;

  scaled = (T_BYTE)(vol * 2.55f);
  fv = ((float)scaled * (float)config.snd_master_volume) / (100.0f * 255.0f);
  if (fv < 0.0f)
    fv = 0.0f;
  if (fv > 1.0f)
    fv = 1.0f;

  /* Like FSOUND_SetSFXMasterVolume: SFX only; streams/music use their own channel volume. */
  fmod_sfx_master = fv;
}


void TCHANNEL::_SetVolume(void)
{
  if (!fch || vol_type == VT_NONE)
    return;
  FMOD_Channel_SetVolume(fch, (float)volume / 255.0f);
}


void TSAMPLE::_SetVolume(void)
{
  if (!fch || vol_type == VT_NONE)
    return;
  FMOD_Channel_SetVolume(fch, ((float)volume / 255.0f) * fmod_sfx_master);
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
  FMOD_BOOL p = 0;

  if (!fch)
    return false;
  if (FMOD_Channel_IsPlaying(fch, &p) != FMOD_OK)
    return false;
  return p != 0;
}


TSAMPLE::~TSAMPLE(void)
{
  Stop();
  if (sample) {
    FMOD_Sound_Release(sample);
    sample = NULL;
  }
}


bool TSAMPLE::Load(char *data, int size)
{
  FMOD_CREATESOUNDEXINFO ex;
  FMOD_RESULT r;

  if (!fmod_sys || !data || size <= 0)
    return false;

  memset(&ex, 0, sizeof(ex));
  ex.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
  ex.length = (unsigned int)size;

  if (sample) {
    FMOD_Sound_Release(sample);
    sample = NULL;
  }
  Stop();

  r = FMOD_System_CreateSound(fmod_sys, data,
                              FMOD_OPENMEMORY | FMOD_CREATESAMPLE | FMOD_2D,
                              &ex, &sample);
  if (r != FMOD_OK || !sample) {
    Warning(LogMsg("TSAMPLE::Load CreateSound failed (r=%d, size=%d)", (int)r, size));
    return false;
  }

  apply_loop_mode(sample, loop);
  return true;
}


void TSAMPLE::SetMaxPlaybacks(int max)
{
  (void)max;
  /* FMOD Ex 4: would need per-sound SoundGroup + SetMaxAudible; not mapped. */
}


void TSAMPLE::Play(void)
{
  FMOD_RESULT r;

  if (!sample || !fmod_sys)
    return;

  if (fch) {
    FMOD_Channel_Stop(fch);
    fch = NULL;
  }

  r = FMOD_System_PlaySound(fmod_sys, FMOD_CHANNEL_FREE, sample, 0, &fch);
  if (r != FMOD_OK)
    Warning(LogMsg("TSAMPLE::Play PlaySound failed (r=%d)", (int)r));
  else
    _SetVolume();
}


void TSAMPLE::Stop(void)
{
  if (fch) {
    FMOD_Channel_Stop(fch);
    fch = NULL;
  }
}


void TSAMPLE::SetLoop(bool lp)
{
  if (loop == lp)
    return;
  loop = lp;
  apply_loop_mode(sample, lp);
}


TSTREAM::~TSTREAM(void)
{
  Stop();
  if (stream) {
    FMOD_Sound_Release(stream);
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
  FMOD_CREATESOUNDEXINFO ex;
  FMOD_RESULT r;
  size_t n;

  if (!fmod_sys || !file_name || size <= 0)
    return false;

  Stop();
  if (stream) {
    FMOD_Sound_Release(stream);
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

  memset(&ex, 0, sizeof(ex));
  ex.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
  ex.length = (unsigned int)size;

  r = FMOD_System_CreateStream(fmod_sys, stream_mem,
                               FMOD_OPENMEMORY | FMOD_2D | FMOD_CREATESTREAM,
                               &ex, &stream);
  if (r != FMOD_OK || !stream) {
    Warning(LogMsg("TSTREAM::Load CreateStream failed (r=%d, file=%s, seek=%d, size=%d)",
                   (int)r, file_name, seek, size));
    delete[] stream_mem;
    stream_mem = NULL;
    stream_mem_size = 0;
    return false;
  }

  apply_loop_mode(stream, loop);
  return true;
}


void TSTREAM::Play(void)
{
  FMOD_RESULT r;

  if (!stream || !fmod_sys)
    return;

  if (fch) {
    FMOD_Channel_Stop(fch);
    fch = NULL;
  }

  r = FMOD_System_PlaySound(fmod_sys, FMOD_CHANNEL_FREE, stream, 0, &fch);
  if (r != FMOD_OK) {
    Warning(LogMsg("TSTREAM::Play PlaySound failed (r=%d)", (int)r));
  } else {
    Info(LogMsg("TSTREAM::Play OK (vol=%d, vol_type=%d)", (int)volume, (int)vol_type));
    _SetVolume();
  }
}


void TSTREAM::Stop(void)
{
  if (fch) {
    FMOD_Channel_Stop(fch);
    fch = NULL;
  }
}


void TSTREAM::SetLoop(bool lp)
{
  if (loop == lp)
    return;
  loop = lp;
  apply_loop_mode(stream, lp);
}


TMODULE::~TMODULE(void)
{
  Stop();
  if (mod) {
    FMOD_Sound_Release(mod);
    mod = NULL;
  }
}


bool TMODULE::Load(char *data, int size)
{
  FMOD_CREATESOUNDEXINFO ex;
  FMOD_RESULT r;

  if (!fmod_sys || !data || size <= 0)
    return false;

  memset(&ex, 0, sizeof(ex));
  ex.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
  ex.length = (unsigned int)size;

  if (mod) {
    FMOD_Sound_Release(mod);
    mod = NULL;
  }
  Stop();

  r = FMOD_System_CreateSound(fmod_sys, data, FMOD_OPENMEMORY | FMOD_2D, &ex, &mod);
  if (r != FMOD_OK || !mod) {
    Warning(LogMsg("TMODULE::Load CreateSound failed (r=%d, size=%d)", (int)r, size));
    return false;
  }

  apply_loop_mode(mod, false);
  return true;
}


void TMODULE::Play(void)
{
  if (!mod || !fmod_sys)
    return;

  if (fch) {
    FMOD_Channel_Stop(fch);
    fch = NULL;
  }

  if (FMOD_System_PlaySound(fmod_sys, FMOD_CHANNEL_FREE, mod, 0, &fch) == FMOD_OK && fch)
    FMOD_Channel_SetVolume(fch, (float)volume / 255.0f);
}


void TMODULE::Stop(void)
{
  if (fch) {
    FMOD_Channel_Stop(fch);
    fch = NULL;
  }
}


void TMODULE::SetVolume(T_BYTE vol)
{
  volume = vol;
  if (fch)
    FMOD_Channel_SetVolume(fch, (float)vol / 255.0f);
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
  apply_loop_mode(mod, lp);
}


bool TMODULE::IsPlaying(void)
{
  FMOD_BOOL p = 0;

  if (!mod || !fch)
    return false;
  if (FMOD_Channel_IsPlaying(fch, &p) != FMOD_OK)
    return false;
  return p != 0;
}

#endif
