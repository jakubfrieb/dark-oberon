/*
 * Minimal C declarations for FMOD Ex 4.x (libfmodex.so) — Dark Oberon audio backend.
 * Layout of FMOD_CREATESOUNDEXINFO must match the installed libfmodex ABI (4.44).
 */
#ifndef DARK_OBERON_FMODEX_API_H
#define DARK_OBERON_FMODEX_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int FMOD_RESULT;
typedef int FMOD_BOOL;
typedef unsigned int FMOD_MODE;
typedef unsigned int FMOD_INITFLAGS;
typedef int FMOD_CHANNELINDEX;
typedef int FMOD_SOUND_FORMAT;
typedef int FMOD_DSP_RESAMPLER;

#define FMOD_SOUND_FORMAT_PCM16 2
#define FMOD_DSP_RESAMPLER_DEFAULT 0

#define FMOD_OK 0
#define FMOD_FALSE 0
#define FMOD_TRUE 1

#define FMOD_INIT_NORMAL 0x00000000

#define FMOD_DEFAULT 0x00000000
#define FMOD_LOOP_OFF 0x00000001
#define FMOD_LOOP_NORMAL 0x00000002
#define FMOD_2D 0x00000008
#define FMOD_CREATESTREAM 0x00000080
#define FMOD_CREATESAMPLE 0x00000100
#define FMOD_OPENMEMORY 0x00000800

#define FMOD_CHANNEL_FREE (-1)

#define FMOD_SOUND_FORMAT_NONE 0

typedef struct FMOD_SYSTEM FMOD_SYSTEM;
typedef struct FMOD_SOUND FMOD_SOUND;
typedef struct FMOD_CHANNEL FMOD_CHANNEL;
typedef struct FMOD_CHANNELGROUP FMOD_CHANNELGROUP;

typedef struct FMOD_SOUNDGROUP FMOD_SOUNDGROUP;

/*
 * Must match the real FMOD Ex 4.44 layout (216 bytes on LP64).
 * Fields we don't use are typed as void* / int to keep alignment correct.
 */
typedef struct FMOD_CREATESOUNDEXINFO {
  int            cbsize;                /* 0 */
  unsigned int   length;               /* 4 */
  unsigned int   fileoffset;           /* 8 */
  int            numchannels;          /* 12 */
  int            defaultfrequency;     /* 16 */
  int            format;               /* 20  FMOD_SOUND_FORMAT */
  unsigned int   decodebuffersize;     /* 24 */
  int            initialsubsound;      /* 28 */
  int            numsubsounds;         /* 32 */
  int           *inclusionlist;        /* 40  (pad at 36) */
  int            inclusionlistnum;     /* 48 */
  void          *pcmreadcallback;      /* 56  (pad at 52) */
  void          *pcmsetposcallback;    /* 64 */
  void          *nonblockcallback;     /* 72 */
  const char    *dlsname;              /* 80 */
  const char    *encryptionkey;        /* 88 */
  int            maxpolyphony;         /* 96 */
  void          *userdata;             /* 104 (pad at 100) */
  int            suggestedsoundtype;   /* 112 FMOD_SOUND_TYPE */
  void          *useropen;             /* 120 (pad at 116) */
  void          *userclose;            /* 128 */
  void          *userread;             /* 136 */
  void          *userseek;             /* 144 */
  void          *userasyncread;        /* 152 */
  void          *userasynccancel;      /* 160 */
  int            speakermap;           /* 168 FMOD_SPEAKERMAPTYPE */
  FMOD_SOUNDGROUP *initialsoundgroup;  /* 176 (pad at 172) */
  unsigned int   initialseekposition;  /* 184 */
  unsigned int   initialseekpostype;   /* 188 FMOD_TIMEUNIT */
  int            ignoresetfilesystem;  /* 192 */
  int            cddaforceaspi;        /* 196 */
  unsigned int   audioqueuepolicy;     /* 200 */
  unsigned int   minmidigranularity;   /* 204 */
  int            nonblockthreadid;     /* 208 */
} FMOD_CREATESOUNDEXINFO;

FMOD_RESULT FMOD_System_Create(FMOD_SYSTEM **system);
FMOD_RESULT FMOD_System_Release(FMOD_SYSTEM *system);
FMOD_RESULT FMOD_System_SetSoftwareFormat(FMOD_SYSTEM *system, int samplerate,
                                          FMOD_SOUND_FORMAT format,
                                          int numoutputchannels,
                                          int maxinputchannels,
                                          FMOD_DSP_RESAMPLER resamplemethod);
FMOD_RESULT FMOD_System_Init(FMOD_SYSTEM *system, int maxchannels,
                             FMOD_INITFLAGS flags, void *extradriverdata);
FMOD_RESULT FMOD_System_Update(FMOD_SYSTEM *system);
FMOD_RESULT FMOD_System_GetVersion(FMOD_SYSTEM *system, unsigned int *version);
FMOD_RESULT FMOD_System_CreateSound(FMOD_SYSTEM *system, const char *name_or_data,
                                    FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo,
                                    FMOD_SOUND **sound);
FMOD_RESULT FMOD_System_CreateStream(FMOD_SYSTEM *system, const char *name_or_data,
                                     FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo,
                                     FMOD_SOUND **sound);
FMOD_RESULT FMOD_System_PlaySound(FMOD_SYSTEM *system, FMOD_CHANNELINDEX channelid,
                                  FMOD_SOUND *sound, FMOD_BOOL paused, FMOD_CHANNEL **channel);
FMOD_RESULT FMOD_System_GetMasterChannelGroup(FMOD_SYSTEM *system,
                                              FMOD_CHANNELGROUP **channelgroup);

FMOD_RESULT FMOD_Sound_Release(FMOD_SOUND *sound);
FMOD_RESULT FMOD_Sound_SetMode(FMOD_SOUND *sound, FMOD_MODE mode);
FMOD_RESULT FMOD_Sound_GetMode(FMOD_SOUND *sound, FMOD_MODE *mode);

FMOD_RESULT FMOD_Channel_Stop(FMOD_CHANNEL *channel);
FMOD_RESULT FMOD_Channel_SetVolume(FMOD_CHANNEL *channel, float volume);
FMOD_RESULT FMOD_Channel_IsPlaying(FMOD_CHANNEL *channel, FMOD_BOOL *isplaying);

FMOD_RESULT FMOD_ChannelGroup_SetVolume(FMOD_CHANNELGROUP *channelgroup, float volume);

#ifdef __cplusplus
}
#endif

#endif
