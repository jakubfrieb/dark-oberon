/*
 * -------------
 *  Dark Oberon
 * -------------
 * 
 * An advanced strategy game.
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
 *  @file doengine.cpp
 *
 *  Menu and game engine + creating GUI and callback functions.
 *
 *  @author Peter Knut
 *  @author Martin Kosalko
 *  @author Marian Cerny
 *
 *  @date 2004, 2005
 */


//========================================================================
// Included files
//========================================================================

#include "cfg.h"
#include "build_info.h"
#include <stdint.h>
#include <cstdio>

#ifdef WINDOWS
 #include <io.h>
#else // on UNIX
 #include <sys/types.h>
 #include <dirent.h>
 #if HEADLESS
 #include <poll.h>
 #include <unistd.h>
 #endif
#endif


#include "donet.h"
#include "doglfw_sdl.h"

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <set>

#include "dosdl.h"
#include "dotime.h"

#include "dofollower.h"
#include "doengine.h"
#include "dodraw.h"
#include "dodevcheat.h"
#include "domap.h"
#include "domouse.h"
#include "doplayers.h"
#include "doraces.h"
#include "doai.h"
#include "doleader.h"
#include "doselection.h"
#include "dosimpletypes.h"
#include "doevents.h"
#include "glgui.h"
#include "dopool.h"
#include "doipc.h"

#if SOUND
#include "dosound.h"
#endif

using std::string;


//========================================================================
// Constants
//========================================================================

// main menu keys
#define MNU_MAIN              0
#define MNU_PLAY              1
#define MNU_OPTIONS           2
#define MNU_CREDITS           3
#define MNU_QUIT              4
#define MNU_QUICK_PLAY        5

// play menu keys
#define MNU_RESUME            6
#define MNU_CREATE            7
#define MNU_CONNECT           8
#define MNU_DISCONNECT        9

// ip menu
#define MNU_CREATE2           10
#define MNU_CONNECT2          11
#define MNU_IP                12
#define MNU_PLAYER_NAME       13

// game menu
#define MNU_PLAY2             15
#define MNU_MAP_LIST          16
#define MNU_KILL_PLAYER       17
#define MNU_ADD_COMPUTER      18

#define MNU_MAP_EDITOR        14

#define MNU_EDITOR_BACK       501
#define MNU_EDITOR_OPEN       502
#define MNU_EDITOR_NEW        503
#define MNU_EDITOR_MAP_LIST   504
#define MNU_EDITOR_FRAG_LIST  505
#define MNU_EDITOR_SAVE       506
#define MNU_EDITOR_EXIT       507
#define MNU_EDITOR_FRAG_BASE  1000
#define MNU_EDITOR_OBJ_BASE   2000
#define MNU_EDITOR_NAV_BACK   2500
#define MNU_EDITOR_NAV_TERRAIN 2501
#define MNU_EDITOR_NAV_OBJECTS 2502
#define MNU_EDITOR_NAV_PLAYER  2503
#define MNU_EDITOR_NAV_PLY_BLD 2504
#define MNU_EDITOR_NAV_PLY_UNI 2505
#define MNU_EDITOR_GROUP_BASE 2600
#define MNU_EDITOR_SOURCE_BASE 3000
#define MNU_EDITOR_SCHEME_UNIT_BASE 3100
#define MNU_EDITOR_SCHEME_BLD_BASE 3200
#define MNU_EDITOR_STARTPOS_BASE 3300
#define MNU_EDITOR_PLAYER_SELECT_BASE 3400
#define MNU_EDITOR_PLAYER_BLD_BASE 3500
#define MNU_EDITOR_PLAYER_UNIT_BASE 3600
#define MNU_EDITOR_ADD_PLAYER       3700

// options menu keys
#define MNU_VIDEO             21
#define MNU_AUDIO             22

// video menu key
#define MNU_FULLSCREEN        30
#define MNU_VERT_SYNC         31
#define MNU_640               32
#define MNU_800               33
#define MNU_1024              34
#define MNU_1152              35
#define MNU_1280              36
#define MNU_1600              37
#define MNU_1920              38

// filters keys
#define MNU_TF_NEAREST        40
#define MNU_TF_LINEAR         41
#define MNU_MF_NONE           42
#define MNU_MF_NEAREST        43
#define MNU_MF_LINEAR         44

// audio menu key
#define MNU_MASTER_VOL        50
#define MNU_MENU_MUSIC        51
#define MNU_MENU_SOUND_VOL    52
#define MNU_MENU_MUSIC_VOL    53
#define MNU_GAME_MUSIC        54
#define MNU_GAME_SOUND_VOL    55
#define MNU_GAME_MUSIC_VOL    56
#define MNU_UNIT_SPEECH       57

// panel keys
#define MNU_TOGGLE_PANEL      100
#define MNU_TOGGLE_RADAR      101

#define MNU_VIEW_SEGMENT      102

// action buttons
#define MNU_ACTION_STAY       110
#define MNU_ACTION_MOVE       111
#define MNU_ACTION_ATTACK     112
#define MNU_ACTION_MINE       113
#define MNU_ACTION_REPAIR     114
#define MNU_ACTION_BUILD      115

// guard
#define MNU_GUARD_BUTTON      300     // other guard buttons have incremental keys (+1, +2, ...)


#define MAX_VID_MODES         100
#define GAME_PANEL_ALPHA      0.9f
#define TOOLTIP_ALPHA         0.7f


//========================================================================
// Macros
//========================================================================

#define SetMenuPanel() \
do { \
  panel->SetColor(0, 0, 0); \
  panel->SetAlpha(0.7f); \
  panel->SetPadding(0); \
  panel->SetVisible(false); \
  panel->SetOnDraw(MenuPanelOnDraw); \
} while (0)


#define SetMenuButton(enabled) \
do { \
  button->SetFaceColor(0.7f, 0.6f, 0.4f); \
  button->SetHoverColor(1, 0.93f, 0.82f); \
  button->SetOnMouseClick(MenuButtonOnClick); \
  button->SetEnabled(enabled); \
} while (0)


#define SetGamePanel(visible) \
do { \
  panel->SetColor(0.3f, 0.3f, 0.3f); \
  panel->SetAlpha(GAME_PANEL_ALPHA); \
  panel->SetPadding(0); \
  panel->SetOnMouseUp(GameOnMouseUp); \
  panel->SetVisible(visible); \
} while (0)


#define SetGameButton(tooltip) \
do { \
  button->SetFaceColor(0.7f, 0.6f, 0.4f); \
  button->SetHoverColor(1, 0.93f, 0.82f); \
  button->SetOnMouseUp(GameOnMouseUp); \
  button->SetOnMouseDown(GameButtonOnMouseDown); \
  button->SetOnMouseClick(GameButtonOnClick); \
  button->SetCanFocus(false); \
  button->SetTooltipText(tooltip); \
} while (0)


#define SetBuildButton() \
do { \
  button->SetFaceColor(0.7f, 0.7f, 0.7f); \
  button->SetHoverColor(1, 1, 1); \
  button->SetOnMouseUp(GameOnMouseUp); \
  button->SetOnMouseClick(GameBuildOnClick); \
  button->SetOnDraw(GameBuildOnDraw); \
  button->SetOnShowTooltip(GameBuildOnTooltip); \
  button->SetTooltipBox(build_tooltip); \
} while (0)


#define SetStayButton() \
do { \
  button->SetFaceColor(0.7f, 0.7f, 0.7f); \
  button->SetHoverColor(1, 1, 1); \
  button->SetOnMouseUp(GameOnMouseUp); \
  button->SetOnMouseClick(GameStayOnClick); \
  button->SetOnDraw(GameBuildOnDraw); \
} while (0)


#define SetProduceButton() \
do { \
  button->SetFaceColor(0.7f, 0.7f, 0.7f); \
  button->SetHoverColor(1, 1, 1); \
  button->SetOnMouseUp(GameOnMouseUp); \
  button->SetOnMouseClick(GameProduceOnClick); \
  button->SetOnShowTooltip(GameBuildOnTooltip); \
  button->SetTooltipBox(build_tooltip); \
} while (0)


#define SetOrderButton(txt) \
do { \
  button->SetFaceColor(0.7f, 0.7f, 0.7f); \
  button->SetHoverColor(1, 1, 1); \
  button->SetOnMouseUp(GameOnMouseUp); \
  button->SetOnMouseDown(GameOrderOnMouseDown); \
  button->SetCaption(txt); \
} while (0)


//========================================================================
// Forward declarations
//========================================================================

class TBUILD_TOOLTIP;
void StopGame();
void BackgroundResolveFinished ();
void SetActiveMenu(TGUI_PANEL *menu);
void UpdateGameMenu();
void MenuUpdateMapInfo (string map_name, uint32_t remote_map_hash);
bool Disconnect();

static void ProcessNetEvent (TNET_MESSAGE *msg);
static void ProcessPlayerArray (TNET_MESSAGE *msg);
static void ProcessHello (TNET_MESSAGE *msg);
static void ProcessPingRequest (TNET_MESSAGE *msg);
static void ProcessPingReply (TNET_MESSAGE *msg);
static void ProcessConnectRequest (TNET_MESSAGE *msg);
static void ProcessChangeRace (TNET_MESSAGE *msg);
static void ProcessRequestAddComputer (TNET_MESSAGE *msg);
static void ProcessChatMessage (TNET_MESSAGE *msg);
static void ProcessSynchronise (TNET_MESSAGE *msg);
static void ProcessAllowProcessFunction (TNET_MESSAGE *msg);
static void ProcessDisconnect (TNET_MESSAGE *msg);

static void ProcessDisconnect (int player_id);
static void OnDisconnect (in_addr address, in_port_t port);

#if !HEADLESS
void EditorMenuOnShow(TGUI_BOX *sender);
void EditorMapListOnChange(TGUI_BOX *sender, int item_index);
void EditorFragListOnChange(TGUI_BOX *sender, int item_index);
void EditorFragButtonOnClick(TGUI_BOX *sender);
void EditorObjButtonOnClick(TGUI_BOX *sender);
void EditorSourceButtonOnClick(TGUI_BOX *sender);
void EditorSchemeUnitButtonOnClick(TGUI_BOX *sender);
void EditorSchemeBldButtonOnClick(TGUI_BOX *sender);
void EditorStartPosButtonOnClick(TGUI_BOX *sender);
void EditorPlayerSelectOnClick(TGUI_BOX *sender);
void EditorPlayerBldButtonOnClick(TGUI_BOX *sender);
void EditorPlayerUnitButtonOnClick(TGUI_BOX *sender);
void EditorNavButtonOnClick(TGUI_BOX *sender);
void EditorBuildFragGroups(int sid);
void EditorRebuildPalette(void);
void EditorCaptureMapHead(const char *basename_no_ext);
bool EditorBootstrap(const char *basename_no_ext);
void EditorShutdown(void);
void CreateEditorGUI(void);
void EditorOnMouseDown(TGUI_BOX *sender, GLfloat x, GLfloat y, int button);
void EditorOnMouseUp(TGUI_BOX *sender, GLfloat x, GLfloat y, int button);
void EditorOnKeyDown(int key);

static string EditorMapBaseId(const char *id)
{
  if (!id || !*id)
    return string();
  string s(id);
  if (s.size() > 4 && s.compare(s.size() - 4, 4, ".map") == 0)
    s.resize(s.size() - 4);
  return s;
}
#endif

//========================================================================
// Variables
//========================================================================

std::string app_path;
std::string user_dir;

TPANEL_INFO panel_info;             //!< Group of variables for drawing unit and player info on main panel.
TGAME_STATE state = ST_MAIN_MENU;
TGAME_ERROR error = ERR_NONE;

TBUILD_TOOLTIP  *build_tooltip = NULL;

string selected_map_name;

/** Update thread (game simulation). */
SDL_Thread *process_thread = NULL;
/** Menu "connect to server" background thread. */
SDL_Thread *connecting_thread = NULL;
static std::atomic<bool> connecting_thread_finished{true};

// menus
TGUI_PANEL *main_menu = NULL;
TGUI_PANEL *play_menu = NULL;
TGUI_PANEL *ip_menu = NULL;
TGUI_PANEL *game_menu = NULL;
TGUI_PANEL *options_menu = NULL;
TGUI_PANEL *video_menu = NULL;
TGUI_PANEL *audio_menu = NULL;
TGUI_PANEL *credits_menu = NULL;
TGUI_PANEL *active_menu = NULL;

// game panels
TGUI_PANEL *main_panel = NULL;
TGUI_PANEL *little_panel = NULL;
TGUI_PANEL *chat_panel = NULL;
TGUI_PANEL *dev_console_panel = NULL;
TGUI_PANEL *radar_panel = NULL;
bool main_panel_visible = true;

TGUI_EDIT_BOX *chat_edit = NULL;
TGUI_EDIT_BOX *dev_console_edit = NULL;

// menu objects
TGUI_EDIT_BOX *ip_edit = NULL;
TGUI_LABEL    *ip_label = NULL;
TGUI_BUTTON   *connect_button = NULL;
TGUI_BUTTON   *create_button = NULL;

TGUI_LIST_BOX *map_list = NULL;
TGUI_LABEL    *map_label = NULL;
TGUI_BUTTON   *play_button = NULL;
TGUI_BUTTON   *disconn_button = NULL;

TGUI_BUTTON *resume_button = NULL;
TGUI_BUTTON *disconnect_button = NULL;

TBASIC_ITEM *last_item = NULL;

// actions for message box
int action_key = 0;
bool action_force = false;

// map info
TGUI_LABEL  *map_scheme_label = NULL;
TGUI_LABEL  *map_size_label = NULL;
TGUI_LABEL  *map_players_label = NULL;
TGUI_LABEL  *map_races_label = NULL;
TGUI_LABEL  *map_author_label = NULL;
TGUI_SCROLL_BOX *map_info_scroll = NULL;

// players info
TGUI_LABEL     *pl_name_label[PL_MAX_PLAYERS];
TGUI_COMBO_BOX *pl_race_combo[PL_MAX_PLAYERS];
TGUI_BUTTON    *pl_kill_button[PL_MAX_PLAYERS];
TGUI_BUTTON    *add_comp_button = NULL;

// loading game
TGUI_PANEL     *load_panel = NULL;

#if !HEADLESS
TGUI_PANEL     *editor_menu = NULL;
TGUI_LIST_BOX  *editor_map_list = NULL;
TGUI_EDIT_BOX  *editor_new_name_edit = NULL;
TGUI_COMBO_BOX *editor_size_combo = NULL;
TGUI_LIST_BOX  *editor_frag_list = NULL;
#endif

bool in_editor_mode = false;
std::string g_editor_saved_map_prologue;
#if !HEADLESS
static std::string editor_entry_basename;
static int editor_selected_fid = 0;
static int editor_selected_oid = -1;
static int editor_paint_segment = 1;
static int editor_object_segment = 1;

enum EditorTool {
  ET_TERRAIN,
  ET_OBJECTS,
  ET_SOURCE,
  ET_SCHEME_UNIT,
  ET_SCHEME_BUILDING,
  ET_PLAYER_BUILDING,
  ET_PLAYER_UNIT,
  ET_START_POS,
  ET_ERASE
};
static EditorTool editor_tool = ET_TERRAIN;

enum EditorPaletteLevel {
  EP_ROOT,
  EP_TERRAIN_TYPES,
  EP_TERRAIN_VARIANTS,
  EP_OBJECTS,
  EP_PLAYER_LIST,
  EP_PLAYER_CATEGORY,
  EP_PLAYER_BUILDINGS,
  EP_PLAYER_UNITS
};
static EditorPaletteLevel editor_palette_level = EP_ROOT;
static int editor_palette_group = -1;
static int editor_selected_pid = 1;
static int editor_selected_source_idx = -1;
static int editor_selected_scheme_uid = -1;
static int editor_selected_scheme_bid = -1;
static int editor_selected_player_bid = -1;
static int editor_selected_player_uid = -1;
static int editor_selected_start_point = -1;

struct EditorFragGroup {
  std::string name;
  std::vector<int> frag_ids;
  int representative_fid;
};
static std::vector<EditorFragGroup> editor_frag_groups;

static TGUI_SCROLL_BOX *editor_palette_sbox = NULL;
static TGUI_PANEL      *editor_right_panel = NULL;
static TGUI_LABEL      *editor_palette_title = NULL;
static TGUI_BUTTON     *editor_add_player_btn = NULL;
/** Non-hyper player slots in map editor (index 0 is hyper). Max FFA size 3. */
static const int EDITOR_MAX_NON_HYPER_PLAYERS = 3;
#endif

// menu lists
TMAP_INFO_LIST map_info_list; //!< List of maps info in menu.

// some networking variables
THOST *host = NULL;
bool connected = false;
bool started = false;
bool allowed_to_start_process_function = true;
bool won_lose = false;

/** Specifies, whether leader loaded a map and created all needed game
 *  structures. This is used by synchronisation of start of the game. */
bool leader_ready;

/**
 *  Specifies, whether we need to redraw the screen. This saves a lot of
 *  processor time. This is used only in menu. The reason, why it is not used in
 *  the game, is that the game is too dynamic and this variable will almost
 *  allways be set to true (because of animations, etc.).
 */
TSAFE_BOOL_SWITCH *need_redraw = NULL;


//========================================================================
// Thread for connecting in menu
//========================================================================

static void trim_string (string &s) {
  while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
    s.erase (0, 1);
  while (!s.empty() && (s.back () == ' ' || s.back () == '\t'))
    s.pop_back ();
}

/** "host:17000" uses that port; otherwise @p default_port (from config). IPv4 host:port supported. */
static void parse_server_address (const string &server, in_port_t default_port,
                                  string &host_out, in_port_t &port_out) {
  host_out = server;
  port_out = default_port;
  size_t colon = server.find_last_of (':');
  if (colon == string::npos || colon + 1 >= server.size ())
    return;
  const string tail = server.substr (colon + 1);
  for (unsigned char c : tail) {
    if (!std::isdigit (c))
      return;
  }
  int p = atoi (tail.c_str ());
  if (p < 1024 || p > 65535)
    return;
  host_out = server.substr (0, colon);
  trim_string (host_out);
  if (host_out.empty ())
    return;
  port_out = static_cast<in_port_t> (p);
}

struct TCONNECT_DATA {
  string server_name;
  in_port_t port;
};

static void ProcessRequestStart (TNET_MESSAGE *msg);
bool StartGame (double stime);

static void connecting_in_menu_thread_impl (void *_data) {
  TCONNECT_DATA *data = static_cast<TCONNECT_DATA *>(_data);
  string server_name = data->server_name;
  in_port_t port = data->port;
  delete data;

  TNET_RESOLVER resolver;

  try {
    string message;

    Debug ("Phase 1: before resolve");

    message = string ("Resolving host ") + server_name + "...";
    action_key = MNU_CONNECT2;
    gui->ShowMessageBox (message.c_str (), GUI_MB_CANCEL);

    in_addr ip_address = resolver.Resolve (server_name);
    string ip_address_string = resolver.NetworkToAscii (ip_address);

    Debug ("Phase 2: before connect");

    TFOLLOWER *follower;

    if (server_name == ip_address_string)
      message = string ("Connecting to ") + ip_address_string + "...";
    else
      message = string ("Connecting to ") + server_name + " (" + ip_address_string + ")...";

    action_key = MNU_CONNECT2;
    gui->ShowMessageBox (message.c_str (), GUI_MB_CANCEL);

    /* Port 0 = ephemeral TCP listen port (avoids EADDRINUSE vs leader on same host:17000). */
    host = follower = NEW TFOLLOWER (follower_in_queue_size, 0, follower_out_queue_size, ip_address, port);
    connected = true;

    gui->HideMessageBox ();

    host->RegisterExtendedFunction (net_protocol_event, ProcessNetEvent);
    host->RegisterExtendedFunction (net_protocol_hello, ProcessHello);
    host->RegisterExtendedFunction (net_protocol_player_array, ProcessPlayerArray);
    host->RegisterExtendedFunction (net_protocol_chat_message, ProcessChatMessage);
    host->RegisterExtendedFunction (net_protocol_synchronise, ProcessAllowProcessFunction);
    host->RegisterExtendedFunction (net_protocol_ping, ProcessPingReply);
    host->RegisterExtendedFunction (net_protocol_disconnect, ProcessDisconnect);

    host->RegisterOnDisconnect (OnDisconnect);

    player_array.RunningOnFollower ();

    follower->Connect (config.player_name);

    Debug ("Phase 3: before ping - background");

    double wait_interval = 0.05;
    for (int i = 0; i < 50; i++) {
      follower->SendPingRequest ();
      AppSleepSeconds(wait_interval);
      wait_interval *= 1.04;
    }

    Debug ("Phase 4: Finished");

  } catch (TNET_RESOLVER::ResolveException &) {
    string message = string ("Error:\nCould not look up host ") + server_name;
    action_key = MNU_DISCONNECT;
    gui->ShowMessageBox (message.c_str (), GUI_MB_OK);
    
  } catch (TNET_ADDRESS::ConnectingErrorException &) {
    string message = string ("Error:\nError connecting to host ") + server_name;
    action_key = MNU_DISCONNECT;
    gui->ShowMessageBox (message.c_str (), GUI_MB_OK);

  } catch (...) {
    Debug ("some other exception");
  }
}

static int SDLCALL connecting_in_menu_thread_sdl (void *_data)
{
  connecting_in_menu_thread_impl (_data);
  connecting_thread_finished.store (true);
  return 0;
}


//========================================================================
// Sounds
//========================================================================

#if SOUND

// vol [0..100]
void ChangeSoundVolume(T_BYTE vol) {
  FmodApplySfxMasterVolume(vol);
  sounds_table.RefreshAllSfxVolumes();
}

// vol [0..100]
void SetMenuMusicVolume(T_BYTE vol) {
  config.snd_menu_music_volume = vol;

  vol = (T_BYTE)(vol * 2.55);

  sounds_table.sounds[DAT_SID_MENU_MUSIC]->SetVolumeAbsolute((vol * config.snd_master_volume) / 100);
}


// vol [0..100]
void SetGameMusicVolume(T_BYTE vol) {
  config.snd_game_music_volume = vol;

  vol = (T_BYTE)(vol * 2.55);

  sounds_table.sounds[DAT_SID_GAME_MUSIC]->SetVolumeAbsolute((vol * config.snd_master_volume) / 100);
}


void SetMasterVolume(T_BYTE vol) {
  config.snd_master_volume = vol;

  SetMenuMusicVolume(config.snd_menu_music_volume);
  SetGameMusicVolume(config.snd_game_music_volume);

  if (state == ST_GAME) ChangeSoundVolume(config.snd_game_sound_volume);
  else ChangeSoundVolume(config.snd_menu_sound_volume);
}


void PrepareSounds()
{
  sounds_table.sounds[DAT_SID_MENU_MUSIC]->SetLoop(true);
  sounds_table.sounds[DAT_SID_GAME_MUSIC]->SetLoop(true);

  SetMasterVolume(config.snd_master_volume);
}

#endif


//========================================================================
// TBUILD_TOOLTIP
//========================================================================

class TBUILD_TOOLTIP: public TGUI_PANEL {
private:
  int mat_count;
  TGUI_LABEL *text;
  TGUI_LABEL *info_pic[SCH_MAX_MATERIALS_COUNT + 2];
  TGUI_LABEL *info_num[SCH_MAX_MATERIALS_COUNT + 2];
  TGUI_LABEL *text2;

  TGUI_BOX_ENVELOPE ch_envelope;

public:
  TBUILD_TOOLTIP():TGUI_PANEL(NULL, 0, 0, 0, 100, 100)
  {
    int i;
    GLfloat line_h = 15.0f;

    SetPadding(3);
    mat_count = scheme.materials_count;

    SetHeight((mat_count + 4) * line_h + 2 * padding);

    text = AddLabel(0, 0, (mat_count + 3) * line_h, "");
    text2 = AddLabel(0, 0, -4, "");

    for (i = 0; i < mat_count + 2; i++) {
      info_pic[i] = AddLabel(0, 0, (mat_count - i + 2) * line_h, 15, 12);
      info_pic[i]->SetColor(1, 1, 1);
      info_num[i] = AddLabel(0, 20, (mat_count - i + 2) * line_h - 2, "");
    }

    // materials
    for (i = 0; i < mat_count; i++) {
      info_pic[i]->SetTexture(scheme.tex_table.GetTexture(scheme.materials[i]->tg_id, 0));
    }

    // food, energy
    info_pic[mat_count + 0]->SetTexture(myself->race->tex_table.GetTexture(myself->race->tg_food_id, 0));
    info_pic[mat_count + 1]->SetTexture(myself->race->tex_table.GetTexture(myself->race->tg_energy_id, 0));
  }


  void SetText(char *txt)
  {
    text->SetCaption(txt);
  }


  void SetText2(char *txt2)
  {
    text2->SetCaption(txt2);
  }

  TGUI_LABEL *GetText2(void)
  {
    return text2;
  }

  void SetInfo(T_BYTE id, int num)
  {
    char txt[10];
    sprintf(txt, "%d", num);
    info_num[id]->SetCaption(txt);
  }


  virtual void RecalculateChildren(TGUI_BOX *sender)
  {
    TGUI_BOX *box;

    ch_envelope.Reset();

    for (box = last_child; box; box = box->GetPrev()) {
      ch_envelope.TestBox(box);
    }

    SetWidth(ch_envelope.GetMaxX() + 2 * padding);
    SetHeight(ch_envelope.GetMaxY() + 2 * padding);
  }
};


//=========================================================================
// Structures of map list used in menu
//=========================================================================

/** FNV-1a 32-bit over raw .map file bytes (for network map sync). */
static uint32_t ComputeMapFileHash (const char *file_name)
{
  TFILE_NAME path;
  snprintf (path, sizeof path, "%s%s", MAP_PATH, file_name);
  FILE *f = fopen (path, "rb");
  if (!f)
    return 0;
  uint32_t h = 2166136261u;
  int c;
  while ((c = fgetc (f)) != EOF) {
    h ^= (uint32_t)(unsigned char)c;
    h *= 16777619u;
  }
  fclose (f);
  return h;
}

/**
 *  Fills list of maps from directory MAP_PATH.
 */
bool TMAP_INFO_LIST::LoadMapInfo(bool basic, const char *file_name){
  
  TFILE_NAME mapname, racname;
  TCONF_FILE *cf, *cf_rac, *cf_sch;
  TFILE_LINE pom;
  bool ok = true, ok_scheme, ok_race, ok_race_all, warn_race;;
  TMAP_BASIC_INFO_NODE * basic_node = NULL;
  TMAP_RAC_INFO_NODE * rac_node = NULL;
  int rac_count = 0, i;
  char id_name[1024], full_name[1024], schemes_name[1024]; //buffer for name, fullname and schemes ids

  sprintf(mapname, "%s%s", MAP_PATH, file_name);
  if (!(cf = OpenConfFile(mapname)))
    return false;
  
  if (basic){ // fill list of basic info of map
    // creating new instance of basic map info
    if (!(basic_node = NEW TMAP_BASIC_INFO_NODE)){
      Critical("Can not allocate memory for menu structures.");
      return false;
    }

    
    // set variables
    strcpy(basic_node->id_name, file_name);
    if (ok) {
      ok = cf->ReadStr(basic_node->name, "name", "", true);
    }
    if (ok) { // put new instance to list
      basic_node->next = map_list;
      map_list = basic_node;
    }
    else delete basic_node;

  }
  else { // fill extended info of map
 
    // extended map information
    if (ok) cf->ReadStr(map_ext_info.author, "author", "", true); //author is not mandatory
    if (ok) ok = cf->ReadSimpleRange( &(map_ext_info.width), "width", 1, MAP_MAX_SIZE, 1);
    if (ok) ok = cf->ReadSimpleRange( &(map_ext_info.height), "height", 1, MAP_MAX_SIZE, 1);

    // scheme name
    if (ok) ok = cf->ReadStr(map_ext_info.scheme_id_name, "scheme", "", true);
    if (ok) {
      char pom[1024];
      sprintf(pom, "%s%s%s", SCH_PATH, map_ext_info.scheme_id_name, ".sch");
      if ((cf_sch = OpenConfFile(pom))) {
        cf_sch->ReadStr(map_ext_info.scheme_name, "name", map_ext_info.scheme_id_name, false);
        CloseConfFile(cf_sch);
        cf_sch = NULL;
      }
    }

    // loading information about races from map
    // clear list of races info if necesary
    ClearRacList();

    if (ok) cf->SelectSection("Players", true);
    if (ok) ok = cf->ReadIntGE(&(map_ext_info.max_players), "max_count", 0, 0);
    if (ok){
      // Maximum count of players not including hyper player.
      int max_players = PL_MAX_PLAYERS - 1;

      if (map_ext_info.max_players > max_players) {
        map_ext_info.max_players = max_players;
        Warning(LogMsg("Maximal count players in map %s is greater than '%d'.", file_name, max_players));
      }
    }

    // reading start points
    if (ok) {
      ok = cf->SelectSection("Start Points", true);

      int start_points_count;

      if (ok) ok = cf->ReadIntGE(&start_points_count, "count", 1, 1);
      if (ok) start_points_count = MIN(start_points_count, PL_MAX_START_POINTS);  // if there is more then PL_MAX_START_POINTS, PL_MAX_START_POINTS is used

      if (ok) player_array.SetStartPointsCount (start_points_count);

      cf->UnselectSection();
    }

    if (ok) ok = cf->SelectSection("Races", true);
    
    if (ok) ok = cf->ReadIntGE(&rac_count, "count", 1, 1);
    
    ok_race_all = false;
    for (i = 0; ok && i < rac_count; i++) {
      //loading each race info
      sprintf(pom, "Race %d", i);
      if (ok) ok = cf->SelectSection(pom, true);
   
      if (ok) ok = cf->ReadStr(id_name, "name", "", true);
        
      if (ok){
        warn_race = false;
        ok_race = true;
        
        // find out if file race.rac exists and read full name and scheme from it
        sprintf(racname, "%s%s/%s.rac", RAC_PATH, id_name, id_name);
        if (!(cf_rac = OpenConfFile(racname))){
          ok_race = false;
          warn_race = true;
        }
        
        // check if race is compatible vith map scheme
        if (ok_race){
          cf_rac->ReadStr(full_name, "name", id_name, true);
          
          ok_scheme = false;

          do {
            if (ok_race) ok_race = cf_rac->ReadStr(schemes_name, "schemes", "", false);
            if ((ok_race) && (*schemes_name != 0)){
              if (!(strcmp(schemes_name, map_ext_info.scheme_id_name))){
                ok_scheme = true;
              }
            }
          } while (*schemes_name != 0);

          if (!ok_scheme) {
            cf_rac->ReadStr(schemes_name, "schemes", "", true); //only for log with line number
            Warning(LogMsg("Map '%s' and race '%s' are not scheme compatible", file_name, id_name));
            warn_race = true;
          }

          ok_race = ok_scheme;
        }

             
        // creating new TMAP_RAC_INFO_NODE node
        if (ok_race){
    
          if (!(rac_node = NEW TMAP_RAC_INFO_NODE)){
            Critical("Can not allocate memory for menu structures.");
            return false;
          }
    
          // set variables
          strcpy(rac_node->id_name, id_name);
          strcpy(rac_node->name, full_name);
        
          // put new instance to list
          rac_node->next = rac_list;
          rac_list = rac_node;

          if (!ok_race_all) ok_race_all = true; //at least one race was readed
        }
        else if (!warn_race) Warning(LogMsg("Race '%s' is corrupted.", id_name));  //error reading race

      
        CloseConfFile(cf_rac);
      }
      
      


      cf->UnselectSection();  // Race X
    }

    if (ok) ok = ok_race_all;
    
    cf->UnselectSection();  //Races
    cf->UnselectSection();  //Players

    SortRacList ();
  }

  CloseConfFile(cf);

  if (!basic) {
    if (ok)
      map_ext_info.file_hash = ComputeMapFileHash (file_name);
    else
      map_ext_info.file_hash = 0;
  }

  if (!ok) Warning(LogMsg("Map '%s' is not complet or is corrupted.", file_name));
  
  return ok;
}


/**
 *  Delete list of maps.
 */
void TMAP_INFO_LIST::ClearMapList(void){

  TMAP_BASIC_INFO_NODE *act, *next;

  if (map_list){
    for (act = map_list, next = act->next; next != NULL; act = next, next = next->next){
      delete act;
      act = NULL;
    }
    delete act;
    map_list = NULL;
  }
}

/**
 *  Delete list of races.
 */
void TMAP_INFO_LIST::ClearRacList(void){

  TMAP_RAC_INFO_NODE *act, *next;

  if (rac_list){
    for (act = rac_list, next = act->next; next != NULL; act = next, next = next->next){
      delete act;
    }
    delete act;
    rac_list = NULL;
  }
}

/**
 *  Sorts list of maps.
 */
void TMAP_INFO_LIST::SortMapList(void) {
  TMAP_BASIC_INFO_NODE *p;
  bool change_made = true;

  if (!map_list)
    return; /* Nothing to sort. */

  /* We are using bubble sort here. */
  while (change_made) {
    change_made = false;

    for (p = map_list; p != NULL; p = p->next) {
      TMAP_BASIC_INFO_NODE *next = p->next;
      TMAP_NAME temp_name;
      TMAP_FILENAME temp_id_name;

      /* If the next item exists and should be before the actual one, SWAP them.
       */
      if (next && strcmp (p->name, next->name) > 0) {
        change_made = true;

        strcpy (temp_name, p->name);
        strcpy (temp_id_name, p->id_name);

        strcpy (p->name, next->name);
        strcpy (p->id_name, next->id_name);

        strcpy (next->name, temp_name);
        strcpy (next->id_name, temp_id_name);
      }
    }
  }
}

/**
 *  Sorts list of races.
 */
void TMAP_INFO_LIST::SortRacList(void) {
  /* XXX: THIS IS UGLY COPY AND PASTE FROM SortMapList(). :-( But it's not my
   *      fault, it's the design fault. [jojolaser] */
  TMAP_RAC_INFO_NODE *p;
  bool change_made = true;

  if (!rac_list)
    return; /* Nothing to sort. */

  /* We are using bubble sort here. */
  while (change_made) {
    change_made = false;

    for (p = rac_list; p != NULL; p = p->next) {
      TMAP_RAC_INFO_NODE *next = p->next;
      TRAC_NAME temp_name;
      TRAC_FILENAME temp_id_name;

      /* If the next item exists and should be before the actual one, SWAP them.
       */
      if (next && strcmp (p->name, next->name) > 0) {
        change_made = true;

        strcpy (temp_name, p->name);
        strcpy (temp_id_name, p->id_name);

        strcpy (p->name, next->name);
        strcpy (p->id_name, next->id_name);

        strcpy (next->name, temp_name);
        strcpy (next->id_name, temp_id_name);
      }
    }
  }
}

/**
 *  Finds out the id_name of the race from the @p name of the race.
 *
 *  @return String containing the id_name of the map.
 *
 *  @throw NotFoundException
 */
string TMAP_INFO_LIST::GetRacIdName (string name) {
  TMAP_RAC_INFO_NODE *p;

  for (p = rac_list; p != NULL; p = p->next)
    if (name == p->name)
      return p->id_name;

  throw NotFoundException ();
}

/**
 *  Finds out the name of the race from the @p id_name of the race.
 *
 *  @return String containing the name of the map.
 *
 *  @throw NotFoundException
 */
string TMAP_INFO_LIST::GetRacName (string id_name) {
  TMAP_RAC_INFO_NODE *p;

  for (p = rac_list; p != NULL; p = p->next)
    if (id_name == p->id_name)
      return p->name;

  throw NotFoundException ();
}

/**
 *  Finds out the name of the mape from the @p id_name of the map.
 *
 *  @return String containing the name of the map or @c NULL when no race with
 *          specified @p id_name exists.
 */
string TMAP_INFO_LIST::GetMapName (string id_name) {
  TMAP_BASIC_INFO_NODE *p;

  for (p = map_list; p != NULL; p = p->next) {
    if (id_name == p->id_name)
      return p->name;
  }

  return "";
  // XXX: throw MapNotFoundException(); [majo]
}

/**
 *  Fills list of maps from directory MAP_PATH.
 */
bool TMAP_INFO_LIST::LoadMapList() {

  bool ok = true;
  char * extension;

  ClearMapList(); // if exists any list of maps, clears it

#ifdef WINDOWS  // on WINDOWS systems
  _finddata_t file;         // file in directory 
  long file_handler;        // handler to first find file in directory
  bool next_file = true;
#else  // on UNIX systems
  DIR *dir;
  struct dirent *entry;
#endif


#ifdef WINDOWS  // on WINDOWS systems
  if ((file_handler = _findfirst((string(MAP_PATH) + "*.map").c_str(), &file)) == -1L) {  // gets handler to first file with mask "*.map"
    _findclose(file_handler); // no file exists
    return ok;
  }

  while (ok && next_file) { // loop over all files and directories id MAP_PATH diectory
    extension = strrchr(file.name, '.');
    if (!(strcmp(extension, ".map"))) // filter in _findfirst is not correct (accepts files *.map*)
      ok = LoadMapInfo(true, file.name);  // loads map info for each *.map file
    next_file = (!_findnext(file_handler, &file));
  }

  _findclose(file_handler);

#else  // on UNIX systems
  if (!(dir = opendir (MAP_PATH))) {
    Critical( LogMsg ("%s%s%s", "Error opening directory '", MAP_PATH, "'"));
    return false;
  }

  while (ok && ((entry = readdir(dir)) != NULL)) {
    if (entry->d_type != DT_DIR)
    {
      extension = strrchr(entry->d_name, '.');
      /** XXX: THIS WILL PROBABLY CRASH if there is a file without an extension **/
      if (!(strcmp(extension, ".map")))
        ok = LoadMapInfo(true, entry->d_name); // loads map info for each *.map file
    }
  }
  
  closedir(dir);
#endif

  SortMapList ();

  return ok;
}

char * TMAP_INFO_LIST::GetRandomMap(){
  TMAP_BASIC_INFO_NODE * act;
  int count;
  int i;
  
  if (map_list) {

    for (act = map_list, count = 0; act != NULL; act = act->next, count++);
    for (act = map_list, i = 0; i < GetRandomInt(count); act = act->next, i++);
    
    return act->id_name;
  }
  else return NULL;
}


//========================================================================
// Useful Methods
//========================================================================

void ToggleMainPanel()
{
  main_panel->ToggleVisible();
  main_panel_visible = main_panel->IsVisible();

  if (chat_panel->IsVisible()) {
    chat_panel->SetAlpha(main_panel->IsVisible() ? GAME_PANEL_ALPHA : 0);
  }
  if (dev_console_panel && dev_console_panel->IsVisible()) {
    dev_console_panel->SetAlpha(main_panel->IsVisible() ? GAME_PANEL_ALPHA : 0);
  }
  if (!chat_panel->IsVisible() && !(dev_console_panel && dev_console_panel->IsVisible())) {
    little_panel->ToggleVisible();
  }

  if (radar.IsHideable()) radar_panel->SetVisible(main_panel->IsVisible());
  if (!radar_panel->IsVisible() && radar.GetMoving()) radar.SetMoving(false);
}


void ToggleRadarPanel()
{
  radar.ToggleHideable();

  if (radar.IsHideable() && !main_panel->IsVisible()) {
    radar.SetMoving(false);
    radar_panel->Hide();
  }
}


void ToggleChatPanel();

void ToggleDevConsole()
{
  if (!dev_console_panel)
    return;

  if (!dev_console_panel->IsVisible()) {
    if (chat_panel->IsVisible())
      ToggleChatPanel();
    little_panel->Hide();
    dev_console_panel->SetAlpha(main_panel->IsVisible() ? GAME_PANEL_ALPHA : 0);
    dev_console_panel->Show();
    dev_console_edit->Focus();
  }
  else {
    little_panel->SetVisible(main_panel->IsVisible());
    dev_console_panel->Hide();
    dev_console_edit->Unfocus();
    dev_console_edit->SetText(NULL);
  }
}


void ToggleChatPanel()
{
  if (!chat_panel->IsVisible()) {
    if (dev_console_panel && dev_console_panel->IsVisible())
      ToggleDevConsole();
    little_panel->Hide();
    chat_panel->SetAlpha(main_panel->IsVisible() ? GAME_PANEL_ALPHA : 0);
    chat_panel->Show();
    chat_edit->Focus();
  }
  else {
    little_panel->SetVisible(main_panel->IsVisible());
    chat_panel->Hide();
    chat_edit->Unfocus();
    chat_edit->SetText(NULL);
  }
}


/**
 *  Updates myself information on little panel.
 */
void UpdateMyselfInfo()
{
  if (state != ST_GAME) {
    myself->update_info = false;
    return;
  }

  int i;

  char txt[100];

  // materials
  for (i = 0; i < scheme.materials_count; i++) {
    sprintf(txt, "%.0f", myself->GetStoredMaterial(i));
    panel_info.material_label[i]->SetCaption(txt);
  }

  // food, energy
  sprintf(txt, "%d/%d", myself->GetOutFood(), myself->GetInFood());
  panel_info.food_label->SetCaption(txt);
  if (myself->GetOutFood() > myself->GetInFood()) panel_info.food_label->SetFontColor(1, 0.4f, 0.4f);
  else panel_info.food_label->SetFontColor(1, 0.93f, 0.82f);

  sprintf(txt, "%d/%d", myself->GetOutEnergy(), myself->GetInEnergy());
  panel_info.energy_label->SetCaption(txt);
  if (myself->GetOutEnergy() > myself->GetInEnergy()) panel_info.energy_label->SetFontColor(1, 0.4f, 0.4f);
  else panel_info.energy_label->SetFontColor(1, 0.93f, 0.82f);

  myself->update_info = false;
}


void SetActiveMenu(TGUI_PANEL *menu)
{
  static SDL_mutex *mutexicek = SDL_CreateMutex ();

  SDL_LockMutex (mutexicek);

  active_menu->SetVisible(false);
  menu->SetVisible(true);
  active_menu = menu;

  SDL_UnlockMutex (mutexicek);
}


/**
 *  Synchronises player_array with game menu.
 */
void SynchronisePlayersAndMenu () {
  player_array.Lock ();

  /* For each player we synchronise player's race selected in combo box and
   * race set in players_array. */
  for (int i = 1; i < player_array.GetCount (); i++) {
    string selected_race_id;

    /* Find out, which race is selected in player's combo box. */
    try {
      const char *text = pl_race_combo[i]->GetText ();
      selected_race_id = map_info_list.GetRacIdName (text);
    } catch (...) {
      /* Do nothing if no race was found. */
      continue;
    }

    try {
      string player_race_id = player_array.GetRaceIdName (i);
      string player_race_name = map_info_list.GetRacName (player_race_id.c_str ());

      /* The race of player i exists. So we check out, if the same race is
       * selected also in the combo box and when not, correct the combo
       * box. */
      if (player_race_id != selected_race_id)
        pl_race_combo[i]->SetItem (player_race_name.c_str ());
    } catch (TMAP_INFO_LIST::NotFoundException &) {
      /* The race of player i does not exist. We change his race to those
       * which is selected in his combo box. */
      player_array.SetRaceIdName (i, selected_race_id);
    }
  }

  player_array.Unlock ();
}


void UpdateGameMenu()
{
  giant->Lock ();

  bool none = host == NULL;
  bool leader = !none && host->GetType () == THOST::ht_leader;
  bool follower = !none && host->GetType () == THOST::ht_follower;

  player_array.Lock ();

  map_list->SetVisible(leader);
  map_label->SetVisible(follower);
  map_info_scroll->SetVisible (!none);

  play_button->SetVisible(leader || follower);

  if (leader) disconn_button->SetPos(20, 15);
  if (follower || none) disconn_button->SetPos(157, 15);

  int i;

  /* Update GUI according to connected players. */
  for (i = 1; i < player_array.GetCount (); i++) {
    string player_name = player_array.GetPlayerName (i);
    pl_name_label[i]->SetCaption (player_name.c_str ());
    pl_name_label[i]->SetVisible (true);
    pl_race_combo[i]->SetVisible (true);
    pl_race_combo[i]->SetEnabled (leader || (follower && !player_array.IsRemote (i)));

    /* First player (leader) can never be killed. */
    pl_kill_button[i]->SetVisible((i == 1) ? false : leader);
  }

  /* Disable not connected players from GUI. */
  for (; i < PL_MAX_PLAYERS; i++) {
    pl_name_label[i]->SetVisible (false);
    pl_race_combo[i]->SetVisible (false);
    pl_kill_button[i]->SetVisible (false);
  }

  if (add_comp_button)
    add_comp_button->SetVisible (leader || follower);

  /* Something could change. */
  need_redraw->SetTrue ();

  player_array.Unlock ();

  giant->Unlock ();
}

/**
 *  Synchronises player_array with game menu and updates the menu.
 */
void UpdatePlayersAndMenu () {
  player_array.Lock ();

#if !HEADLESS
  SynchronisePlayersAndMenu ();
  UpdateGameMenu ();
#endif

  giant->Lock ();
  if (host != NULL && host->GetType () == THOST::ht_leader)
    dynamic_cast<TLEADER *>(host)->SendPlayerArray (selected_map_name, false);
  giant->Unlock ();

  player_array.Unlock ();
}


//========================================================================
// Engine Methods
//========================================================================

bool Disconnect() {
  /* Join or detach connecting thread if it is running. */
  if (connecting_thread) {
    if (connecting_thread_finished.load ()) {
      SDL_WaitThread (connecting_thread, NULL);
    } else {
      Debug ("disconnect: detaching unfinished connecting thread");
      SDL_DetachThread (connecting_thread);
    }
    connecting_thread = NULL;
  }

  /* one never knows... (if the process thread is not still waiting :-) */
  allowed_to_start_process_function = true;

  if (!connected) {
    action_force = false;
    return true;
  }

  // show message box first
  if (config.show_disconnect_warning && !action_force) {
#if !HEADLESS
    gui->ShowMessageBox("Do you really want to disconnect?", GUI_MB_YES | GUI_MB_NO);
    return false;
#endif
    /* HEADLESS: no confirmation dialog */
  }

  action_force = false;

  if (started) {
    for (int i = 0; i < player_array.GetCount(); i++){
      if (players[i]->active && !player_array.IsRemote(i)){
        players[i]->active = false;
      }
    }
    StopGame();  // have to be called before play player_array.Clear()
  }

  player_array.Clear();
  connected = false;

  THOST *to_delete = NULL;

  giant->Lock ();
  if (host) {
    to_delete = host;
    host = NULL; 
  }
  giant->Unlock ();

  if (to_delete != NULL) {
    to_delete->RegisterOnDisconnect (NULL);
    delete to_delete;
  }

  return true;
}

bool CreateGame()
{
  // Tear down any prior session before starting a new game. Disconnect()
  // returns false on a real network teardown error; "no session active"
  // is treated as success internally so the early-out below is safe to
  // reach from a fresh start. If you change Disconnect() semantics, audit
  // every CreateGame() caller.
  if (!Disconnect())
    return false;

  giant->Lock ();

#if !HEADLESS
  player_array.AddLocalPlayer (config.player_name);
#endif

  // start leader
  host = NEW TLEADER (leader_in_queue_size, config.net_server_port,
                      leader_out_queue_size);

  host->RegisterExtendedFunction (net_protocol_event, ProcessNetEvent);
  host->RegisterExtendedFunction (net_protocol_connect, ProcessConnectRequest);
  host->RegisterExtendedFunction (net_protocol_change_race, ProcessChangeRace);
  host->RegisterExtendedFunction (net_protocol_chat_message, ProcessChatMessage);
  host->RegisterExtendedFunction (net_protocol_synchronise, ProcessSynchronise);
  host->RegisterExtendedFunction (net_protocol_ping, ProcessPingRequest);
  host->RegisterExtendedFunction (net_protocol_disconnect, ProcessDisconnect);
  host->RegisterExtendedFunction (net_protocol_request_start, ProcessRequestStart);
  host->RegisterExtendedFunction (net_protocol_request_add_computer, ProcessRequestAddComputer);

  host->RegisterOnDisconnect (OnDisconnect);

  leader_ready = false;

#if !HEADLESS
  host->AddEmptyAddress (); /* player on leader */
#endif
  host->AddEmptyAddress (); /* hyper player */

  // XXX: check whether the server started successfully
  connected = true;

  giant->Unlock ();

  return true;
}

bool Connect (string server)
{
  if (!Disconnect ())
    return false;

  trim_string (server);
  string hostpart;
  in_port_t remote_port;
  parse_server_address (server, static_cast<in_port_t> (config.net_server_port), hostpart, remote_port);

  TCONNECT_DATA *data = NEW TCONNECT_DATA;
  data->port = remote_port;
  data->server_name = hostpart;

  connecting_thread_finished.store (false);
  connecting_thread = SDL_CreateThread (connecting_in_menu_thread_sdl, "menu_connect", data);
  if (connecting_thread == NULL)
    Critical ("Error creating thread");
  Debug ("connect: created background connecting thread");

  return true;
}


bool Quit() {
  if (!Disconnect())
    return false;

  state = ST_QUIT;
  return true;
}


//========================================================================
// Menu Callbacks
//========================================================================

/**
 *  Callback function that is called everytime a menu button is clicked.
 *
 *  @param key Key of the button that has called this function.
 */
void MenuButtonOnClickKey(intptr_t key, TGUI_BOX *sender = NULL)
{
#if SOUND
  if (!action_force) sounds_table.sounds[DAT_SID_MENU_BUTTON]->Play();
#endif

  switch (key) {
  case GUI_MB_OK:
  case GUI_MB_YES:
    if (action_key) {
      action_force = true;
      MenuButtonOnClickKey(action_key);
    }
    gui->HideMessageBox();
    break;

  case GUI_MB_NO:
  case GUI_MB_CANCEL:
    /* If user clicked on Cancel when "Waiting for other players..." */
    if (action_key == MNU_PLAY2) {
      action_force = true;
      state = ST_MAIN_MENU;
      Disconnect();
    }

    /* If user clicked on Cancel when connecting */
    if (action_key == MNU_CONNECT2) {
      if (connecting_thread) {
        if (connecting_thread_finished.load ()) {
          SDL_WaitThread (connecting_thread, NULL);
        } else {
          Debug ("cancel connect: detaching connecting thread");
          SDL_DetachThread (connecting_thread);
        }
        connecting_thread = NULL;
      }
      action_force = true;
      MenuButtonOnClickKey(MNU_DISCONNECT, sender);
    }

    action_key = 0;
    gui->HideMessageBox();
    break;

  case MNU_PLAY:
    SetActiveMenu(play_menu);
    state = ST_PLAY_MENU;
    break;

  case MNU_QUIT:
    action_key = MNU_QUIT;

    Quit();
    break;

  case MNU_QUICK_PLAY:
    action_key = MNU_QUICK_PLAY;

    allowed_to_start_process_function = true;

    if (!CreateGame())
      break;

    selected_map_name = map_info_list.GetRandomMap();
    if (!map_info_list.LoadMapInfo(false, selected_map_name.c_str ())) {
      action_key = 0;
      gui->ShowMessageBox ((string ("Error loading map '") + selected_map_name + "'").c_str (), GUI_MB_OK);
      break;
    }

    /* Set race for hyper player. */
    player_array.SetRaceIdName(0, map_info_list.map_ext_info.scheme_id_name);

    {
      int max_players = map_info_list.map_ext_info.max_players;

      if (max_players >= 1) {
        player_array.SetRaceIdName(1, map_info_list.rac_list->id_name);
      }
      if (max_players >= 2) {
        player_array.AddComputerPlayer ();
        host->AddEmptyAddress (); // computer_player
        player_array.SetRaceIdName(2, map_info_list.rac_list->next->id_name);
      }
    }

    state = ST_GAME;
    break;
  
  case MNU_CREDITS:       SetActiveMenu(credits_menu); break;

#if !HEADLESS
  case MNU_MAP_EDITOR:
    /* The editor runs its own session (EditorBootstrap -> CreateGame), so a running
       game has to be closed here, while the confirmation box can still be shown. */
    action_key = MNU_MAP_EDITOR;

    if (Disconnect()) {
      resume_button->SetEnabled(false);
      disconnect_button->SetEnabled(false);
      map_info_list.ClearRacList();
      SetActiveMenu(editor_menu);
    }
    break;

  case MNU_EDITOR_BACK:
    SetActiveMenu(main_menu);
    break;

  case MNU_EDITOR_OPEN: {
    if (!editor_map_list || !map_info_list.map_list) {
      gui->ShowMessageBox("No maps available", GUI_MB_OK);
      break;
    }
    /* List selection is often -1 until the user clicks a row; default to first map. */
    int idx = editor_map_list->GetItemIndex();
    if (idx < 0 && editor_map_list->GetItemsCount() > 0) {
      char *first_line = editor_map_list->GetItem(0);
      if (first_line)
        editor_map_list->SetItem(first_line);
      idx = editor_map_list->GetItemIndex();
    }
    if (idx < 0) {
      gui->ShowMessageBox("Select a map in the list", GUI_MB_OK);
      break;
    }
    TMAP_BASIC_INFO_NODE *act;
    int i;
    for (act = map_info_list.map_list, i = 0; act != NULL && i < idx; act = act->next, i++) {}
    if (act == NULL) {
      gui->ShowMessageBox("Map list out of sync; try again", GUI_MB_OK);
      break;
    }
    editor_entry_basename = EditorMapBaseId(act->id_name);
    if (editor_entry_basename.empty()) {
      gui->ShowMessageBox("Invalid map id", GUI_MB_OK);
      break;
    }
    selected_map_name = editor_entry_basename + ".map";
    state = ST_EDITOR;
    break;
  }

  case MNU_EDITOR_NEW: {
    const char *raw = editor_new_name_edit->GetText();
    if (raw == NULL || !*raw) {
      gui->ShowMessageBox("Enter a map id (letters, digits, _ -)", GUI_MB_OK);
      break;
    }
    string id = raw;
    bool ok_id = !id.empty() && id.size() < MAP_MAX_NAME_LENGTH;
    for (size_t j = 0; ok_id && j < id.size(); j++) {
      unsigned char c = (unsigned char)id[j];
      if (!isalnum(c) && id[j] != '_' && id[j] != '-')
        ok_id = false;
    }
    if (!ok_id) {
      gui->ShowMessageBox("Invalid map id", GUI_MB_OK);
      break;
    }
    int cidx = editor_size_combo->GetItemIndex();
    if (cidx < 0)
      cidx = 0;
    char *szline = editor_size_combo->GetItem(cidx);
    if (!szline)
      break;
    string szs = szline;
    int w = atoi(szs.c_str());
    if (w != 80 && w != 128 && w != 160) {
      gui->ShowMessageBox("Pick size 80, 128, or 160", GUI_MB_OK);
      break;
    }
    if (!TMAP::EditorWriteBlankPlasticMap(id.c_str(), w, w, id.c_str())) {
      gui->ShowMessageBox("Could not create map file (maps/ writable?)", GUI_MB_OK);
      break;
    }
    map_info_list.LoadMapList();
    editor_entry_basename = id;
    selected_map_name = id + ".map";
    state = ST_EDITOR;
    break;
  }

  case MNU_EDITOR_SAVE:
    if (state == ST_EDITOR && map.width > 0) {
      if (map.SaveMapToFile(map.id_name))
        ost->AddText("Map saved", 3.0, INFO_COLOR_R, INFO_COLOR_G, INFO_COLOR_B);
      else
        gui->ShowMessageBox("Save failed (missing prologue or I/O error)", GUI_MB_OK);
    }
    break;

  case MNU_EDITOR_EXIT:
    state = ST_MAIN_MENU;
    break;

  case MNU_EDITOR_ADD_PLAYER: {
    static const char *race_cycle[] = {"human-red", "human-yellow", "human-blue"};
    int old_count = player_array.GetCount();
    if (old_count >= 1 + EDITOR_MAX_NON_HYPER_PLAYERS) {
      gui->ShowMessageBox("Max players reached (3 per map)", GUI_MB_OK);
      break;
    }

    player_array.Lock();
    player_array.AddComputerPlayer();
    if (host) host->AddEmptyAddress();
    int pid = player_array.GetCount() - 1;

    const char *chosen_race = race_cycle[(pid - 1) % 3];
    player_array.SetRaceIdName(pid, chosen_race);
    player_array.Unlock();

    if (!GrowPlayersRuntime(old_count, player_array.GetCount())) {
      if (host)
        host->RemoveAddress(pid);
      player_array.RemovePlayer(pid);
      gui->ShowMessageBox("Failed to grow players array", GUI_MB_OK);
      break;
    }

    string rn = player_array.GetRaceIdName(pid);
    TRACE *r;
    for (r = races; r; r = r->next)
      if (rn == r->id_name) break;
    if (!r) {
      LoadRace((char *)rn.c_str(), false);
      for (r = races; r; r = r->next)
        if (rn == r->id_name) break;
    }
    if (r) players[pid]->race = r;

    players[pid]->initial_x = map.width / 2;
    players[pid]->initial_y = map.height / 2;

    EditorAddStartPoint((T_SIMPLE)(map.width / 2), (T_SIMPLE)(map.height / 2));
    player_array.SetStartPoint(pid, EditorGetStartPointCount() - 1);

    if (editor_palette_level == EP_PLAYER_LIST)
      EditorRebuildPalette();

    char msg[64];
    sprintf(msg, "Player %d added (%s)", pid, chosen_race);
    ost->AddText(msg, 3.0, INFO_COLOR_R, INFO_COLOR_G, INFO_COLOR_B);

    map_info_list.map_ext_info.max_players = MIN(
        player_array.GetCount() - 1,
        MIN(PL_MAX_PLAYERS - 1, EDITOR_MAX_NON_HYPER_PLAYERS));
    break;
  }
#endif

  case MNU_OPTIONS:       SetActiveMenu(options_menu); break;
  case MNU_VIDEO:
    SetActiveMenu(video_menu);
    state = ST_VIDEO_MENU;
    break;
  case MNU_AUDIO:         SetActiveMenu(audio_menu); break;
  case MNU_RESUME:        state = ST_GAME; break;

  case MNU_CREATE:
    ip_edit->Hide();
    connect_button->Hide();

    ip_label->Show();
    create_button->Show();
    
    SetActiveMenu(ip_menu);
    break;

  case MNU_CONNECT:
    ip_edit->Show();
    connect_button->Show();

    ip_label->Hide();
    create_button->Hide();

    SetActiveMenu(ip_menu);
    ip_edit->Focus();
    break;

  case MNU_KILL_PLAYER:
    player_array.Lock ();

    {
      TGUI_BUTTON *button = (TGUI_BUTTON *)sender;

      for (int i = 1; i < PL_MAX_PLAYERS; i++)
        if (button == pl_kill_button[i]) {
          host->RemoveAddress (i);
          player_array.RemovePlayer (i);
        }
    }

    UpdatePlayersAndMenu ();
    player_array.Unlock ();
    break;

  case MNU_ADD_COMPUTER:
    if (host && host->GetType () == THOST::ht_follower) {
      dynamic_cast<TFOLLOWER *> (host)->SendRequestAddComputer ();
      break;
    }
    if (host && host->GetType () == THOST::ht_leader) {
      player_array.Lock ();
      {
        int max_p = map_info_list.map_ext_info.max_players;
        if (player_array.GetCount () >= max_p + 1) {
#if !HEADLESS
          gui->ShowMessageBox ("Too many players for this map.", GUI_MB_OK);
#endif
        } else {
          player_array.AddComputerPlayer ();
          host->AddEmptyAddress ();
          int idx = player_array.GetCount () - 1;
          string chosen;
          for (TMAP_RAC_INFO_NODE *r = map_info_list.rac_list; r; r = r->next) {
            bool taken = false;
            for (int j = 0; j < player_array.GetCount (); j++) {
              if (player_array.GetRaceIdName (j) == string (r->id_name)) {
                taken = true;
                break;
              }
            }
            if (!taken) {
              chosen = r->id_name;
              break;
            }
          }
          if (!chosen.empty ())
            player_array.SetRaceIdName (idx, chosen);
        }
      }
      player_array.Unlock ();
      UpdatePlayersAndMenu ();
    }
    break;

  case MNU_CREATE2:
    action_key = MNU_CREATE2;

    if (CreateGame()) {
      UpdateGameMenu();
      SetActiveMenu(game_menu);
    }
    break;

  case MNU_CONNECT2:
    action_key = MNU_CONNECT2;

    if (Connect(ip_edit->GetText())) {
      UpdateGameMenu();
      SetActiveMenu(game_menu);
    }
    break;

  case MNU_PLAY2:
    player_array.Lock ();

    allowed_to_start_process_function = player_array.AllPlayersAreLocal ();

    {
      int max_players = map_info_list.map_ext_info.max_players;
      enum { none, too_many_players, not_different_races } error = none;

      if (player_array.GetCount () > max_players + 1)
        error = too_many_players;
      else if (!player_array.EveryPlayerHasDifferentRace ())
        error = not_different_races;

      if (error != none) {
        switch (error) {
        case too_many_players:
          action_key = 0;
          gui->ShowMessageBox ((string ("Too many players! Maximum count\n of players for this map is ") + char(max_players + '0') + ".").c_str(), GUI_MB_OK);
          break;
        case not_different_races:
          action_key = 0;
          gui->ShowMessageBox ("Every player must have different race!", GUI_MB_OK);
          break;
        case none:
          /* Will never happen. */
          break;
        }
        player_array.Unlock ();
        break;
      }

      giant->Lock ();

      if (host->GetType () == THOST::ht_follower) {
        /* Ask leader to start; we enter ST_GAME when ProcessPlayerArray runs. */
        TFOLLOWER *fol = dynamic_cast<TFOLLOWER *>(host);
        fol->SendRequestStartGame ();
        giant->Unlock ();
        player_array.Unlock ();
        break;
      }

      /* Leader will inform all followers that the game is starting. */
      if (host->GetType () == THOST::ht_leader) {
        TLEADER *leader = dynamic_cast<TLEADER *>(host);

        /* XXX: not needed... leader->FillRemoteAddresses (); */
        leader->SendPlayerArray (selected_map_name, true);
      }

      state = ST_GAME;

      giant->Unlock ();

      /* XXX: we still need to block somehow so that no more players can be
       *      added... i.e. something like player_array.NoMoreChanges (); */
    }

    player_array.Unlock ();
    break;

  case MNU_DISCONNECT:
    action_key = MNU_DISCONNECT;

    if (Disconnect()) {
      resume_button->SetEnabled(false);
      disconnect_button->SetEnabled(false);
      SetActiveMenu(play_menu);

      // clear menu structures
      map_info_list.ClearRacList();
    }
    break;

  case MNU_MAIN:          SetActiveMenu(main_menu); break;
  }
}


/**
 *  Callback function that is called everytime a menu button is clicked.
 *
 *  @param sender Button object that has called this function.
 */
void MenuButtonOnClick(TGUI_BOX *sender)
{
    MenuButtonOnClickKey(sender->GetKey(), sender);
}


/**
 *  This function is called when we are in menu (#state == #ST_MENU) and a key
 *  was pressed.
 *
 *  @param key  GLFW key identifier of released key.
 */
void MenuOnKeyDown(int key)
{
  if (gui->KeyDown(key)) return;

  switch (key) {
  // On ESC we end Menu and change to Quit.
  case GLFW_KEY_ESC:
#if !HEADLESS
    if (editor_menu && editor_menu->IsVisible())
      MenuButtonOnClickKey(MNU_EDITOR_BACK);
    else
#endif
    if (main_menu->IsVisible())
      MenuButtonOnClickKey(MNU_QUIT);

    else if (video_menu->IsVisible() || (audio_menu && audio_menu->IsVisible()))
      MenuButtonOnClickKey(MNU_OPTIONS);

    else if (ip_menu->IsVisible())
      MenuButtonOnClickKey(MNU_PLAY);

    else if (game_menu->IsVisible())
      MenuButtonOnClickKey(MNU_DISCONNECT);

    else MenuButtonOnClickKey(MNU_MAIN);

    break;

  case GLFW_KEY_ENTER:
  case GLFW_KEY_KP_ENTER:
    if (ip_menu->IsVisible())
    {
      if (create_button->IsVisible()) MenuButtonOnClickKey(MNU_CREATE2);
      else MenuButtonOnClickKey(MNU_CONNECT2);
    }
    break;
  }
}


/**
 *  Callback function that is called everytime a menu checkbox button is
 *  clicked.
 *
 *  @param key Key of the checkbox button that has called this function.
 */
void MenuCheckBoxOnClick(intptr_t key)
{
#if SOUND
  sounds_table.sounds[DAT_SID_MENU_CONTROL]->Play();
#endif

  switch (key) {
  case MNU_FULLSCREEN:
    config.fullscreen = !config.fullscreen;
    config.file->WriteBool("fullscreen", config.fullscreen);
    break;

  case MNU_VERT_SYNC:
    config.vert_sync = !config.vert_sync;
    config.file->WriteBool("vert_sync", config.vert_sync);
    #if UNIX
      /* Many X11 drivers historically ignored or mishandled vsync; keep off on UNIX. */
      glfwSwapInterval(0);
    #else
      glfwSwapInterval(config.vert_sync ? 1 : 0);
    #endif
    break;

  case MNU_640:
    config.scr_width = 640;
    config.scr_height = 480;
    config.file->WriteStr("resolution", "640x480");
    glfwSetWindowSize(640, 480);
    state = ST_RESET_VIDEO_MENU;
    break;

  case MNU_800:
    config.scr_width = 800;
    config.scr_height = 600;
    config.file->WriteStr("resolution", "800x600");
    glfwSetWindowSize(800, 600);
    state = ST_RESET_VIDEO_MENU;
    break;

  case MNU_1024:
    config.scr_width = 1024;
    config.scr_height = 768;
    config.file->WriteStr("resolution", "1024x768");
    glfwSetWindowSize(1024, 768);
    state = ST_RESET_VIDEO_MENU;
    break;

  case MNU_1152:
    config.scr_width = 1152;
    config.scr_height = 864;
    config.file->WriteStr("resolution", "1152x864");
    glfwSetWindowSize(1152, 864);
    state = ST_RESET_VIDEO_MENU;
    break;

  case MNU_1280:
    config.scr_width = 1280;
    config.scr_height = 1024;
    config.file->WriteStr("resolution", "1280x1024");
    glfwSetWindowSize(1280, 1024);
    state = ST_RESET_VIDEO_MENU;
    break;

  case MNU_1600:
    config.scr_width = 1600;
    config.scr_height = 1200;
    config.file->WriteStr("resolution", "1600x1200");
    glfwSetWindowSize(1600, 1200);
    state = ST_RESET_VIDEO_MENU;
    break;

  case MNU_1920:
    config.scr_width = 1920;
    config.scr_height = 1080;
    config.file->WriteStr("resolution", "1920x1080");
    glfwSetWindowSize(1920, 1080);
    state = ST_RESET_VIDEO_MENU;
    break;

  case MNU_TF_NEAREST:
    config.tex_mag_filter = GL_NEAREST;

    if (config.tex_min_filter == GL_LINEAR) config.tex_min_filter = GL_NEAREST;
    else if (config.tex_min_filter == GL_LINEAR_MIPMAP_NEAREST) config.tex_min_filter = GL_NEAREST_MIPMAP_NEAREST;
    else if (config.tex_min_filter == GL_LINEAR_MIPMAP_LINEAR) config.tex_min_filter = GL_NEAREST_MIPMAP_LINEAR;
    config.file->WriteStr("texture_filter", "nearest");
    break;

  case MNU_TF_LINEAR:
    config.tex_mag_filter = GL_LINEAR;

    if (config.tex_min_filter == GL_NEAREST) config.tex_min_filter = GL_LINEAR;
    else if (config.tex_min_filter == GL_NEAREST_MIPMAP_NEAREST) config.tex_min_filter = GL_LINEAR_MIPMAP_NEAREST;
    else if (config.tex_min_filter == GL_NEAREST_MIPMAP_LINEAR) config.tex_min_filter = GL_LINEAR_MIPMAP_LINEAR;
    config.file->WriteStr("texture_filter", "linear");
    break;

  case MNU_MF_NONE:
    config.tex_min_filter = config.tex_mag_filter;
    config.file->WriteStr("mipmap_filter", "none");
    break;

  case MNU_MF_NEAREST:
    if (config.tex_mag_filter == GL_NEAREST) config.tex_min_filter = GL_NEAREST_MIPMAP_NEAREST;
    else config.tex_min_filter = GL_LINEAR_MIPMAP_NEAREST;
    config.file->WriteStr("mipmap_filter", "nearest");
    break;

  case MNU_MF_LINEAR:
    if (config.tex_mag_filter == GL_NEAREST) config.tex_min_filter = GL_NEAREST_MIPMAP_LINEAR;
    else config.tex_min_filter = GL_LINEAR_MIPMAP_LINEAR;
    config.file->WriteStr("mipmap_filter", "linear");
    break;

#if SOUND

  case MNU_MENU_MUSIC:
    config.snd_menu_music = !config.snd_menu_music;
    config.file->WriteBool("snd_menu_music", config.snd_menu_music);

    if (config.snd_menu_music) sounds_table.sounds[DAT_SID_MENU_MUSIC]->Play();
    else sounds_table.sounds[DAT_SID_MENU_MUSIC]->Stop();
    break;

  case MNU_GAME_MUSIC:
    config.snd_game_music = !config.snd_game_music;
    config.file->WriteBool("snd_game_music", config.snd_game_music);
    break;

  case MNU_UNIT_SPEECH:
    config.snd_unit_speech = !config.snd_unit_speech;
    config.file->WriteBool("snd_unit_speech", config.snd_unit_speech);
    break;

#endif

  }

  config.file->Save();
}


/**
 *  Callback function that is called everytime a menu checkbox button is
 *  clicked.
 *
 *  @param sender Checkbox button object that has called this function.
 */
void MenuCheckBoxOnClick(TGUI_BOX *sender)
{
  MenuCheckBoxOnClick(sender->GetKey());
}


void MenuPanelOnDraw(TGUI_BOX *sender)
{
#if !HEADLESS
  glDisable(GL_TEXTURE_2D);

  glColor3f(0.8f, 0.8f, 0.8f);
  glBegin(GL_LINE_STRIP);
    glVertex2f(0, 0);
    glVertex2f(0, sender->GetHeight() - 1);
    glVertex2f(sender->GetWidth() - 1, sender->GetHeight() - 1);
  glEnd();

  glBegin(GL_LINE_STRIP);
    glVertex2f(sender->GetWidth() - 1, sender->GetHeight());
    glVertex2f(sender->GetWidth() - 1, 0);
    glVertex2f(0, 0);
  glEnd();

  glEnable(GL_TEXTURE_2D);
#else
  (void)sender;
#endif
}

void MenuUpdateMapInfo (string map_name, uint32_t remote_map_hash) {
#if !HEADLESS
  char buff[4096];
  TMAP_RAC_INFO_NODE * act_rac;
  int i, count_rac;
  string races;
#endif

  player_array.Lock ();

  map_info_list.ClearRacList();

  bool loaded = map_info_list.LoadMapInfo (false, map_name.c_str ());
  bool hash_mismatch = false;
  if (loaded && remote_map_hash != 0
      && map_info_list.map_ext_info.file_hash != remote_map_hash) {
    hash_mismatch = true;
    loaded = false;
  }

  if (loaded) { // selected map exists and matches server hash (if server sent one)
    selected_map_name = map_name; // put map id_name to global variable ... if menu quits, this map will be loaded

    player_array.SetRaceIdName(0, map_info_list.map_ext_info.scheme_id_name);

#if !HEADLESS
    map_label->SetCaption (map_info_list.GetMapName (map_name).c_str ());

    play_button->SetEnabled(true);

    // author
    map_author_label->SetCaption(map_info_list.map_ext_info.author);

    // scheme
    map_scheme_label->SetCaption(map_info_list.map_ext_info.scheme_name);
    
    // map size
    sprintf(buff, "%dx%d", map_info_list.map_ext_info.width, map_info_list.map_ext_info.height);
    map_size_label->SetCaption(buff);

    // races
    count_rac = 0;
    for (act_rac=map_info_list.rac_list; act_rac != NULL; act_rac = act_rac->next){
      races += string (act_rac->name) + "\n";
      count_rac++;
    }
    races.erase (races.end () - 1, races.end());   // Erase the last "\n".

    map_races_label->SetCaption(races.c_str ());
    map_races_label->SetPosY(40 - map_races_label->GetHeight() + map_races_label->GetLineHeight());

    sprintf(buff, "%d", MIN(map_info_list.map_ext_info.max_players, count_rac));
    map_players_label->SetCaption(buff);

    for (i = 1; i < PL_MAX_PLAYERS; i++)
      pl_race_combo[i]->SetItems(races.c_str ());

    SynchronisePlayersAndMenu ();
#endif
  }
  else{ // can not find file of selected map, parse error, or hash mismatch with server
    selected_map_name = "";
#if !HEADLESS
    {
      string cap;
      if (hash_mismatch)
        cap = "Map version mismatch with server";
      else
        cap = string ("Map not found: ") + map_name;
      map_label->SetCaption (cap.c_str ());
    }

    // clear global map name and disable play button
    play_button->SetEnabled(false);

    // set empty info map labels
    map_scheme_label->SetCaption("");
    map_size_label->SetCaption("");
    map_author_label->SetCaption("");
    map_players_label->SetCaption("");
    map_races_label->SetCaption("");

    // hide combos and buttons
    for (i = 1; i < PL_MAX_PLAYERS; i++){
      pl_race_combo[i]->Hide();
      pl_kill_button[i]->Hide();
      pl_name_label[i]->Hide();

      pl_race_combo[i]->SetItems("");
    }
#else
    (void)hash_mismatch;
#endif
  }

  player_array.Unlock ();
}

void MenuListOnChange(TGUI_BOX *sender, int item_index)
{
  TMAP_BASIC_INFO_NODE * act;
  int i;

  if (sender->GetKey() == MNU_MAP_LIST) {
    for (act = map_info_list.map_list, i = 0; (act != NULL) && (i < item_index); act = act->next, i++); // finds pointer to item_index.th map

    if (act != NULL) {
      MenuUpdateMapInfo (act->id_name, 0);
      UpdatePlayersAndMenu ();
    }
  }
}


/**
 *  Callback function called everytime a player race has been changed in combo
 *  box. It updates player race in #player_array.
 *
 *  @param sender     Sender of the message which is the list in combo box.
 *  @param item_index Index of item in the list.
 */
void MenuPlayerRaceComboOnChange (TGUI_BOX *sender, int item_index) {
  
  player_array.Lock();

  TGUI_LIST_BOX *list = (TGUI_LIST_BOX *)sender;

  for (int i = 1; i < PL_MAX_PLAYERS; i++)
    if (list == pl_race_combo[i]->GetList ()) {
      string selected_race_name = list->GetItem (item_index);
      string selected_race_id = map_info_list.GetRacIdName (selected_race_name);
      player_array.SetRaceIdName (i, selected_race_id);

      giant->Lock ();

      if (host->GetType () == THOST::ht_follower)
        dynamic_cast<TFOLLOWER *>(host)->ChangeRace (i,
            player_array.GetPlayerName (i), selected_race_id);

      giant->Unlock ();

      break;
    }

  UpdatePlayersAndMenu ();
  
  player_array.Unlock();
}


void MenuPanelOnShow(TGUI_BOX *sender)
{
  giant->Lock ();

  if (sender == game_menu) {
    /* If the actual host is a leader, set map_list listbox content according
     * to map_info_list and read extended information about the first map in
     * the list. */
    if (host && host->GetType () == THOST::ht_leader) {
      TMAP_BASIC_INFO_NODE *act;
      string maps;

      // fill full map names into string separated by 'newline'
      if (map_info_list.map_list){
        for (act = map_info_list.map_list; act != NULL; act = act->next)
          maps += string (act->name) + "\n";
        maps.erase (maps.end () - 1, maps.end());   // Erase the last "\n".
      }
      else
        maps = "No maps";

      map_list->SetItems(maps.c_str ()); // assign string to menu structure
      
      if (map_info_list.map_list) 
        MenuListOnChange(map_list, 0); // read extended information about first map in list
    }
  }

  giant->Unlock ();
}


void MenuEditOnChange(TGUI_BOX *sender, char *text)
{
  switch (sender->GetKey()) {
  case MNU_PLAYER_NAME:
    strcpy(config.player_name, text);
    config.file->WriteStr("player_name", text);
    break;

  case MNU_IP:
    strcpy(config.address, text);
    config.file->WriteStr("address", text);
    connect_button->SetEnabled(*text > 0);
    break;
  }

  config.file->Save();
}


#if SOUND

void MenuSliderOnChange(TGUI_BOX *sender, GLfloat pos)
{
  switch (sender->GetKey()) {
  case MNU_MASTER_VOL:
    SetMasterVolume(T_BYTE(pos * 100));
    config.file->WriteInt("snd_master_volume", config.snd_master_volume);
    break;

  case MNU_MENU_SOUND_VOL:
    config.snd_menu_sound_volume = T_BYTE(pos * 100);
    ChangeSoundVolume(config.snd_menu_sound_volume);
    config.file->WriteInt("snd_menu_sound_volume", config.snd_menu_sound_volume);
    break;

  case MNU_MENU_MUSIC_VOL:
    SetMenuMusicVolume(T_BYTE(pos * 100));
    config.file->WriteInt("snd_menu_music_volume", config.snd_menu_music_volume);
    break;

  case MNU_GAME_SOUND_VOL:
    config.snd_game_sound_volume = T_BYTE(pos * 100);
    config.file->WriteInt("snd_game_sound_volume", config.snd_game_sound_volume);
    break;

  case MNU_GAME_MUSIC_VOL:
    SetGameMusicVolume(T_BYTE(pos * 100));
    config.file->WriteInt("snd_game_music_volume", config.snd_game_music_volume);
    break;
  }

  config.file->Save();
}

#endif


//========================================================================
// Game Mouse Callbacks
//========================================================================

/**
 *   Callback function for events in game.
 */
void GameOnMouseDown(TGUI_BOX *sender, GLfloat x, GLfloat y, int button)
{
  if (!started) return;

  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT:
    mouse.down_x = mouse.x;
    mouse.down_y = mouse.y;

    if (mouse.cursor_id == MC_ARROW || 
      mouse.cursor_id == MC_CAN_MOVE || mouse.cursor_id == MC_CAN_BUILD || 
      mouse.cursor_id == MC_CANT_MOVE || mouse.cursor_id == MC_CANT_BUILD)
    {
      mouse.down_cursor_id = MC_ARROW;
      mouse.down_circle_id = MCC_NONE;
    }
    else {
      mouse.down_cursor_id = MC_SELECT;
      mouse.down_circle_id = MCC_ARROWS_IN;
    }

    mouse.action = UA_NONE;
    myself->build_item = NULL;

    mouse.draw_selection = true;
    break;

  case GLFW_MOUSE_BUTTON_MIDDLE:
    if (!radar.GetMoving()) map.drag_moving = true;
    break;

  case GLFW_MOUSE_BUTTON_RIGHT:
    if (selection->TestCanSetRally() && map.IsInMap(mouse.map_pos.x, mouse.map_pos.y) && !mouse.over_unit) {
      TFACTORY_UNIT *fu = static_cast<TFACTORY_UNIT *>(selection->GetFirstUnit());
      T_BYTE seg = view_segment;
      if (seg == DRW_ALL_SEGMENTS)
        seg = fu->GetPosition().segment;
      TPOSITION_3D goal;
      goal.SetPosition(mouse.map_pos.x, mouse.map_pos.y, seg);
      fu->SetRallyGoalFromLocal(goal);
      mouse.action = UA_NONE;
    } else switch (mouse.cursor_id) {
    case MC_SELECT:
    case MC_CAN_MOVE:
    case MC_CANT_MOVE:

      if (selection->MoveUnits(mouse.map_pos)) 
      {
        mouse.action = UA_NONE;
      }
      break;

    case MC_CAN_ATTACK:
    case MC_CANT_ATTACK:
    case MC_CAN_MINE:
    case MC_CANT_MINE:
    case MC_CAN_REPAIR:
    case MC_CANT_REPAIR:
    case MC_CAN_HIDE:
    case MC_CANT_HIDE:

      if (selection->ReactUnits()) {
        mouse.action = UA_NONE;
      }
      break;

    case MC_CAN_BUILD:
    case MC_CANT_BUILD:
      {
        TBUILDING_UNIT *building;
        TWORKER_UNIT *build_worker = selection->GetBuildWorker();

        if (myself->build_item && build_worker &&
            (building = build_worker->StartBuild(myself->build_item, mouse.map_pos, false))) {

          // other units iterract with new building
          TNODE_OF_UNITS_LIST *ul = selection->GetUnitsList();
          for (; ul; ul = ul->next)
            (ul->unit != mouse.over_unit && ul->unit->SelectReaction(building, UA_REPAIR));

          // reset mouse action
          myself->build_item = NULL;
          mouse.action = UA_NONE;
          UncheckBuildButton();
        }
      }
      break;

    case MC_EJECT:
      mouse.over_unit->EjectUnits();

      mouse.action = UA_NONE;
      break;

    default: break;
    }

#if SOUND
    // error sound
    switch (mouse.cursor_id) {
    case MC_CANT_MOVE:
    case MC_CANT_ATTACK:
    case MC_CANT_MINE:
    case MC_CANT_REPAIR:
    case MC_CANT_BUILD:
      myself->race->snd_error.Play();
      break;

    default: break;
    }
#endif

    break;
  }
}


/**
 *   Callback function for events in game.
 */
void GameOnMouseUp(TGUI_BOX *sender, GLfloat x, GLfloat y, int button)
{
  if (!started) return;

  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT:

    if (mouse.draw_selection) {
      if (mouse.cursor_id != MC_SELECT_PLUS)
        selection->UnselectAll();   // unselects all selected units

      // mouse selection
      if (fabs(x - mouse.down_x) > MC_SELECT_RADIUS || fabs(y - mouse.down_y) > MC_SELECT_RADIUS) {
        mouse.RectSelect();

        if (selection->IsEmpty() && mouse.over_unit)                 // selects all unit in selection rectangle
          selection->SelectUnit(mouse.over_unit);
      }
      else {
        if (mouse.cursor_id == MC_SELECT_PLUS)
          selection->AddDeleteUnit(mouse.over_unit);

        else if (mouse.cursor_id == MC_SELECT)
          selection->SelectUnit(mouse.over_unit);
      }

      mouse.draw_selection = false;       // end of selection rectangle drawing
    }

    // map moving by radar
    if (radar.GetMoving()) radar.SetMoving(false);
    break;

  case GLFW_MOUSE_BUTTON_MIDDLE:
    map.drag_moving = false;
    break;

  case GLFW_MOUSE_BUTTON_RIGHT:
    break;
    }
}


/**
 *  Callback function for click events on some of the game GUI buttons.
 *
 *  @param key Key of the game GUI button that has called this function.
 */
void GameButtonOnClick(intptr_t key)
{
  if (!started) return;

  switch (key) {
  case MNU_TOGGLE_PANEL:
    ToggleMainPanel(); break;

  case MNU_TOGGLE_RADAR:
    ToggleRadarPanel(); break;

  case MNU_QUIT:
    action_key = MNU_QUIT;
    Quit();
    break;

  case MNU_VIEW_SEGMENT:
    view_segment++;
    if (view_segment > DRW_ALL_SEGMENTS) view_segment = 0;
    panel_info.seg_button->SetTexture(GUI_BS_UP, gui_table.GetTexture(DAT_TGID_SEG_BUTTONS, view_segment));    
    break;

  case MNU_ACTION_STAY:
    myself->build_item = NULL;
    mouse.action = UA_NONE;
    UpdateGuardButtons();
    selection->StopUnits();
    selection->UpdateInfo(true);
    if (panel_info.stay_button->IsChecked())
      ChangeActionPanel(MNU_PANEL_STAY);
    break;

  case MNU_ACTION_MOVE:
    myself->build_item = NULL;
    mouse.action = UA_MOVE;
    ChangeActionPanel(MNU_PANEL_MOVE);
    break;

  case MNU_ACTION_ATTACK:
    myself->build_item = NULL;
    mouse.action = UA_ATTACK;
    ChangeActionPanel(MNU_PANEL_ATTAK);
    break;

  case MNU_ACTION_MINE:
    myself->build_item = NULL;
    mouse.action = UA_MINE;
    ChangeActionPanel(MNU_PANEL_MINE);
    break;

  case MNU_ACTION_REPAIR:
    myself->build_item = NULL;
    mouse.action = UA_REPAIR;
    ChangeActionPanel(MNU_PANEL_REPAIR);
    break;

  case MNU_ACTION_BUILD:
    mouse.action = UA_NONE;
    CreateBuildButtons();
    ChangeActionPanel(MNU_PANEL_BUILD);
    break;
  }
}


/**
 *  Callback function for mouse events on some of the game GUI buttons.
 *
 *  @param sender Game GUI button that has called this function.
 */
void GameButtonOnMouseDown(TGUI_BOX *sender, GLfloat, GLfloat, int button)
{
  if (!started) return;
  if (button != GLFW_MOUSE_BUTTON_LEFT) return;

  switch (sender->GetKey()) {
  case MNU_ACTION_STAY:
    if (panel_info.stay_button->IsChecked()) {
      GameButtonOnClick(MNU_ACTION_STAY);
    }
    break;

  case MNU_ACTION_MOVE:
    if (panel_info.move_button->IsChecked()) {
      GameButtonOnClick(MNU_ACTION_MOVE);
    }
    break;

  case MNU_ACTION_ATTACK:
    if (panel_info.attack_button->IsChecked()) {
      GameButtonOnClick(MNU_ACTION_ATTACK);
    }
    break;

  case MNU_ACTION_MINE:
    if (panel_info.mine_button->IsChecked()) {
      GameButtonOnClick(MNU_ACTION_MINE);
    }
    break;

  case MNU_ACTION_REPAIR:
    if (panel_info.repair_button->IsChecked()) {
      GameButtonOnClick(MNU_ACTION_REPAIR);
    }
    break;

  default:
    break;
  }
}


/**
 *  Callback function for click events on some of the game GUI buttons.
 *
 *  @param sender Game GUI button that has called this function.
 */
void GameButtonOnClick(TGUI_BOX *sender)
{
  GameButtonOnClick(sender->GetKey());
}


/**
 *   Callback function for events of build buttons.
 *
 *   @param key   Key of menu object that has called this function.
 */
void GameBuildOnClick(TGUI_BOX *sender)
{
  if (!started) return;

  mouse.action = UA_BUILD;
  myself->build_item = (TBUILDING_ITEM *)sender->GetKey();
  panel_info.act_build_button = (TGUI_BUTTON *)sender;
}


void GameStayOnClick(TGUI_BOX *sender)
{
  if (!started) return;

  selection->SetAggressivity(TAGGRESSIVITY_MODE(sender->GetKey() - MNU_GUARD_BUTTON));
}


void GameProduceOnClick(TGUI_BOX *sender)
{
  if (!started) return;

  static_cast<TFACTORY_UNIT *>(selection->GetFirstUnit())->AddUnitToOrder((TFORCE_ITEM *)sender->GetKey());
  CreateOrderButtons();
}


void GameOrderOnClick(TGUI_BOX *sender)
{
  if (!started) return;

  static_cast<TFACTORY_UNIT *>(selection->GetFirstUnit())->TogglePausedProducing();
}


void GameOrderOnMouseDown(TGUI_BOX *sender, GLfloat x, GLfloat y, int button)
{
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    static_cast<TFACTORY_UNIT *>(selection->GetFirstUnit())->CancelProducing(panel_info.order_panel->GetChildOrder(sender));
    CreateOrderButtons();
  }
}


void GameBuildOnDraw(TGUI_BOX *sender)
{
  if (!started) return;

  TGUI_BUTTON *b = (TGUI_BUTTON *)sender;

  if (!b->IsChecked()) return;

#if !HEADLESS
  glDisable(GL_TEXTURE_2D);

  // frame rectangle
  glColor3f(0.7f, 0.6f, 0.4f);
  glBegin(GL_LINE_STRIP);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, sender->GetHeight() - 1);
    glVertex2f(sender->GetWidth(), sender->GetHeight() - 1);
  glEnd();

  glBegin(GL_LINE_STRIP);
    glVertex2f(sender->GetWidth() - 1, sender->GetHeight());
    glVertex2f(sender->GetWidth() - 1, 0.0f);
    glVertex2f(0.0f, 0.0f);
  glEnd();

  glEnable(GL_TEXTURE_2D);
#else
  (void)b;
#endif
}


void GameBuildOnTooltip(TGUI_BOX *sender)
{
  if (!started) return;

  TBASIC_ITEM *itm = (TBASIC_ITEM *)(sender->GetKey());

  // unit name
  build_tooltip->SetText(itm->name);

  // ancestor of buildings
  build_tooltip->GetText2()->Hide();
  TBUILDING_ITEM *bitm = dynamic_cast<TBUILDING_ITEM *>(itm);
  if (bitm) {
    if (bitm->ancestor) {
      build_tooltip->SetText2(bitm->ancestor->name);
      build_tooltip->GetText2()->Show();
    }
  }

  // materials
  for (int i = 0; i < scheme.materials_count; i++)
    build_tooltip->SetInfo(i, (int)itm->materials[i]);

  // aids
  build_tooltip->SetInfo(scheme.materials_count + 0, itm->food < 0 ? -itm->food : 0);
  build_tooltip->SetInfo(scheme.materials_count + 1, itm->energy < 0 ? -itm->energy : 0);
}


//========================================================================
// Game Key Callbacks
//========================================================================

#if !HEADLESS
static void DevConsoleOstLineSink(void *user, const char *line)
{
  TOST *o = static_cast<TOST *>(user);
  if (o && line)
    o->AddText(line);
}

static void ProcessDevConsoleCommand(const char *raw)
{
  if (!raw)
    return;
  while (*raw == ' ' || *raw == '\t')
    raw++;
  if (!*raw)
    return;

  string line(raw);
  while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
    line.pop_back();
  if (line.empty())
    return;

  string cmd;
  string arg;
  size_t sp = line.find(' ');
  if (sp == string::npos) {
    cmd = line;
  } else {
    cmd = line.substr(0, sp);
    arg = line.substr(sp + 1);
    while (!arg.empty() && (arg[0] == ' ' || arg[0] == '\t'))
      arg.erase(0, 1);
    while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t'))
      arg.pop_back();
  }

  for (size_t i = 0; i < cmd.size(); i++)
    cmd[i] = (char)tolower((unsigned char)cmd[i]);
  for (size_t i = 0; i < arg.size(); i++)
    arg[i] = (char)tolower((unsigned char)arg[i]);

  if (cmd == "help") {
    ost->AddText("Dev: map | map off | resource [all] | speed on | speed off");
    ost->AddText("Dev: logs | logs on | logs off | logs think on | logs think off | logs <cpu_slot>");
    return;
  }
  if (cmd == "map") {
    if (arg == "off") {
      DevCheatsSetRevealMap(false);
      ost->AddText("Dev: war fog restored");
    } else {
      DevCheatsSetRevealMap(true);
      ost->AddText("Dev: full map reveal");
    }
    return;
  }
  if (cmd == "resource") {
    if (arg == "all") {
      int n = player_array.GetCount();
      for (int i = 1; i < n; i++) {
        if (players[i])
          DevCheatsApplyResources(players[i]);
      }
      ost->AddText("Dev: +10000 each material (all players)");
    } else if (arg.empty()) {
      DevCheatsApplyResources(myself);
      ost->AddText("Dev: +10000 each material");
    } else {
      ost->AddText("Dev: resource [all]");
    }
    return;
  }
  if (cmd == "speed") {
    if (arg == "off") {
      dev_fast_timers = false;
      ost->AddText("Dev: normal build/train times");
    } else {
      dev_fast_timers = true;
      ost->AddText("Dev: ~1s build and train steps");
    }
    return;
  }
  if (cmd == "logs") {
    if (arg.empty()) {
      TAI_EmitCpuPlayersListLines(DevConsoleOstLineSink, ost);
      return;
    }
    if (arg == "enable" || arg == "on") {
      TAI_SetPhaseTransitionLogging(true);
      ost->AddText("Dev: AI phase-change log on (stderr)");
      return;
    }
    if (arg == "think" || arg == "think on") {
      TAI_SetThinkTraceLogging(true);
      ost->AddText("Dev: AI think trace on (stderr)");
      return;
    }
    if (arg == "think off") {
      TAI_SetThinkTraceLogging(false);
      ost->AddText("Dev: AI think trace off");
      return;
    }
    if (arg == "off") {
      TAI_SetPhaseTransitionLogging(false);
      TAI_SetThinkTraceLogging(false);
      ost->AddText("Dev: AI phase + think trace off");
      return;
    }
    char *endp = NULL;
    long slot = std::strtol(arg.c_str(), &endp, 10);
    if (endp != arg.c_str() && endp && *endp == '\0' && slot >= 0)
      TAI_EmitPlayerAIDumpLines(static_cast<int>(slot), DevConsoleOstLineSink, ost);
    else
      ost->AddText("Dev: logs | logs on | logs off | logs think on | logs think off | logs <cpu_slot>");
    return;
  }

  ost->AddText("Dev: unknown command (try help)");
}
#endif


/**
 *  This function is called when we are in game (#state == #ST_GAME) and a key
 *  was pressed.
 *
 *  @param key  GLFW key identifier of released key.
 */
void GameOnKeyDown(int key)
{
#if HEADLESS
  (void)key;
  return;
#else
  if (!started) return;

  if (key == '`') {
    if (!dev_console_panel || !dev_console_panel->IsVisible()) {
      ToggleDevConsole();
      return;
    }
    if (TGUI::focus_box == static_cast<TGUI_BOX *>(dev_console_edit)) {
      if (gui->KeyDown(key))
        return;
      return;
    }
    ToggleDevConsole();
    return;
  }

  if (gui->KeyDown(key)) return;

  switch (key) {
 
  case GLFW_KEY_ESC:
    if (dev_console_panel && dev_console_panel->IsVisible())
      ToggleDevConsole();
    else if (chat_panel->IsVisible()) ToggleChatPanel();
    else if (mouse.draw_selection) mouse.draw_selection = false;
    else if (mouse.action == UA_NONE) {
      radar.SetMoving(false);
      state = ST_PLAY_MENU;
    }
    else {
      myself->build_item = NULL;
      mouse.action = UA_NONE;

      // uncheck build button
      UncheckBuildButton();

      selection->UpdateAction();
    }
    break;

  case GLFW_KEY_ENTER:
  case GLFW_KEY_KP_ENTER:
    if (dev_console_panel && dev_console_panel->IsVisible()) {
      ProcessDevConsoleCommand(dev_console_edit->GetText());
      dev_console_edit->SetText(NULL);
    }
    else if (chat_panel->IsVisible()) {
      char txt[1024];
      sprintf(txt, "%s: %s", myself->name, chat_edit->GetText());
      host->SendChatMessage(chat_edit->GetText());
      ost->AddText(txt);
      ToggleChatPanel();
    }
    break;

  case GLFW_KEY_TAB:
    ToggleMainPanel();
    break;

  // On 'Q' we change the state of the game to Quit.
  case 'Q':
    GameButtonOnClick(MNU_QUIT);
    break;
  
  case 'Y':
  case 'U':
    ToggleChatPanel();
    break;

  case 'S':
    if (panel_info.stay_button->IsEnabled()) {
      panel_info.stay_button->SetChecked(true);
      GameButtonOnClick(MNU_ACTION_STAY);
    }
    break;

  case 'M':
    if (panel_info.move_button->IsEnabled()) {
      panel_info.move_button->SetChecked(true);
      GameButtonOnClick(MNU_ACTION_MOVE);
    }
    break;

  case 'A':
    if (panel_info.attack_button->IsEnabled()) {
      panel_info.attack_button->SetChecked(true);
      GameButtonOnClick(MNU_ACTION_ATTACK);
    }
    break;

  case 'I':
    if (panel_info.mine_button->IsEnabled()) {
      panel_info.mine_button->SetChecked(true);
      GameButtonOnClick(MNU_ACTION_MINE);
    }
    break;

  case 'R':
    if (panel_info.repair_button->IsEnabled()) {
      panel_info.repair_button->SetChecked(true);
      GameButtonOnClick(MNU_ACTION_REPAIR);
    }
    break;

  case 'B':
    if (panel_info.build_button->IsEnabled()) {
      panel_info.build_button->SetChecked(true);
      GameButtonOnClick(MNU_ACTION_BUILD);
    }
    break;

  case 'G':
    reduced_drawing = !reduced_drawing;
    break;

  /*
   * RIGHT, LEFT, DOWN and UP starts to move the map in the selected
   * direction. The map stops to move, when the apropriate key is released.
   */
  case GLFW_KEY_LEFT:
    map.StartKeyMove(MAP_KEY_MOVE_RIGHT);
    break;
  case GLFW_KEY_RIGHT:
    map.StartKeyMove(MAP_KEY_MOVE_LEFT);
    break;
  case GLFW_KEY_UP:
    map.StartKeyMove(MAP_KEY_MOVE_DOWN);
    break;
  case GLFW_KEY_DOWN:
    map.StartKeyMove(MAP_KEY_MOVE_UP);
    break;

  /*
   * Map can be moved with mouse while ALT key is pressed.
   */
  case GLFW_KEY_LALT:
    if (!radar.GetMoving()) map.mouse_moving = true;
    break;

  /*
   * On '+' we zoom in the map. Both '+'s (on numeric keyboard and on
   * standard keyboard) are recognized. Also the key '=' is recognized as
   * '+', because it's on the same key as standard '+', but one does not
   * need to press the SHIFT key, too.
   */
  case GLFW_KEY_KP_ADD:
  case '+':
  case '=':
    map.Zoom(MAP_ZOOM_IN);
    break;

  /*
   * On '-' we zoom out the map. Both '-'s (on numeric keyboard and on
   * standard keyboard) are recognized.
   */
  case GLFW_KEY_KP_SUBTRACT:
  case '-':
    map.Zoom(MAP_ZOOM_OUT);
    break;

  /*
   * On '*' we reset zoom of the map. Both '*'s (on numeric keyboard and on
   * standard keyboard) are recognized.
   */
  case GLFW_KEY_KP_DIVIDE:
  case '/':
    map.Zoom(MAP_ZOOM_RESET);
    break;

  /*
   * With 'F5' - 'F8' you can change which segment should be shown.
   */
  case GLFW_KEY_F5:
  case GLFW_KEY_F6:
  case GLFW_KEY_F7:
  case GLFW_KEY_F8:
    view_segment = key - GLFW_KEY_F5;
    panel_info.seg_button->SetTexture(GUI_BS_UP, gui_table.GetTexture(DAT_TGID_SEG_BUTTONS, view_segment));
    break;

  case GLFW_KEY_F9:
    MenuCheckBoxOnClick(MNU_640);
    break;
  case GLFW_KEY_F10:
    MenuCheckBoxOnClick(MNU_800);
    break;
  case GLFW_KEY_F11:
    MenuCheckBoxOnClick(MNU_1024);
    break;


#if DEBUG
  case GLFW_KEY_INSERT:
    queue_events->LogQueue();
    break;

  case GLFW_KEY_BACKSPACE:
    show_all = !show_all;
    break;
#endif


  case GLFW_KEY_F1:
  case GLFW_KEY_F2:
  case GLFW_KEY_F3:
  case GLFW_KEY_F4:    
    if (!selection->IsEmpty()) {
      selection->SetAggressivity(TAGGRESSIVITY_MODE(key - GLFW_KEY_F1));
      if (panel_info.stay_button->IsChecked())
        selection->UpdateAction();
    }
    break;
  
  /*
   * With numbers you can manipulate with groups of units.
   */
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    {
      static double key_time[9] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

      key -= '1';

      if (glfwGetKey(GLFW_KEY_LCTRL))
        selection->StoreSelection(key);
      else {
        double delta = AppGetTimeSeconds() - key_time[key];
        bool double_key = delta < 0.5;
        bool center = glfwGetKey(GLFW_KEY_LALT) == GL_TRUE || double_key;

        key_time[key] += delta;

        selection->RestoreSelection(key, center, !double_key);
      }
      break;
    }
  }
#endif /* !HEADLESS */
}


/**
 *  This function is called when we are in game (#state == #ST_GAME) and a key
 *  was released.
 *
 *  @param key  GLFW key identifier of released key.
 */
void GameOnKeyUp(int key)
{
  if (!started) return;
  switch (key) {

  /*
   * The map stops it's move, when a key LEFT, RIGHT, UP or DOWN was
   * released. The map started it's move, when the same key was pressed.
   */
  case GLFW_KEY_LEFT:
    map.StopKeyMove(MAP_KEY_MOVE_RIGHT);
    break;
  case GLFW_KEY_RIGHT:
    map.StopKeyMove(MAP_KEY_MOVE_LEFT);
    break;
  case GLFW_KEY_UP:
    map.StopKeyMove(MAP_KEY_MOVE_DOWN);
    break;
  case GLFW_KEY_DOWN:
    map.StopKeyMove(MAP_KEY_MOVE_UP);
    break;

  /*
   * Map moving with mouse is stopped when ALT key is released.
   */
  case GLFW_KEY_LALT:
    map.mouse_moving = false;
    break;
  }
}


//========================================================================
// Radar Callbacks
//========================================================================

/**
 *   Callback function for mouse moving in radar.
 */
void GameOnRadarMouseMove(TGUI_BOX *sender, GLfloat x, GLfloat y)
{
  if (radar.GetMoving()) {
    GLfloat koef = radar.zoom / (GLfloat)DAT_MAPEL_DIAGONAL_SIZE;

    map.SetPosition(-(x - radar.dx) / koef, -(y / koef / 2));
  }
}


/**
 *   Callback function for mouse events in radar.
 */
void GameOnRadarMouseDown(TGUI_BOX *sender, GLfloat x, GLfloat y, int button)
{
  if (!started) return;

  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT:
    if (!map.mouse_moving && !map.drag_moving) {
      radar.SetMoving(true);
      GameOnRadarMouseMove(sender, x, y);
    }
    break;
  }
}


void GameOnRadarDraw(TGUI_BOX *sender)
{
  if (!started) return;

  radar.Draw();
  MenuPanelOnDraw(sender);
}


//========================================================================
// Callbacks for network messages
//========================================================================

/**
 *  Follower asked the leader to add a CPU player (lobby).
 *  used: pointer only stored via RegisterExtendedFunction (table read in donet.cpp).
 */
static void __attribute__((__used__)) ProcessRequestAddComputer (TNET_MESSAGE * /* msg */) {
  if (host == NULL || host->GetType () != THOST::ht_leader)
    return;

  giant->Lock ();
  if (started) {
    giant->Unlock ();
    return;
  }
  giant->Unlock ();

  player_array.Lock ();
  {
    int max_p = map_info_list.map_ext_info.max_players;
    if (player_array.GetCount () >= max_p + 1) {
      Warning ("request_add_computer: too many players for this map");
      player_array.Unlock ();
      return;
    }
    player_array.AddComputerPlayer ();
    host->AddEmptyAddress ();
    int idx = player_array.GetCount () - 1;
    string chosen;
    for (TMAP_RAC_INFO_NODE *r = map_info_list.rac_list; r; r = r->next) {
      bool taken = false;
      for (int j = 0; j < player_array.GetCount (); j++) {
        if (player_array.GetRaceIdName (j) == string (r->id_name)) {
          taken = true;
          break;
        }
      }
      if (!taken) {
        chosen = r->id_name;
        break;
      }
    }
    if (!chosen.empty ())
      player_array.SetRaceIdName (idx, chosen);
  }
  player_array.Unlock ();

  UpdatePlayersAndMenu ();
}

/**
 *  Follower asked the leader to start the game (menu Play or future UI).
 *  used: pointer only stored via RegisterExtendedFunction (table read in donet.cpp).
 */
static void __attribute__((__used__)) ProcessRequestStart (TNET_MESSAGE * /* msg */) {
  if (host == NULL || host->GetType () != THOST::ht_leader)
    return;

  giant->Lock ();
  if (started) {
    giant->Unlock ();
    return;
  }
  giant->Unlock ();

  player_array.Lock ();
  allowed_to_start_process_function = player_array.AllPlayersAreLocal ();

  int max_players = map_info_list.map_ext_info.max_players;
  if (player_array.GetCount () > max_players + 1) {
    Warning ("request_start: too many players for this map");
    player_array.Unlock ();
    return;
  }
  if (!player_array.EveryPlayerHasDifferentRace ()) {
    Warning ("request_start: every player must have a different race");
    player_array.Unlock ();
    return;
  }
  player_array.Unlock ();

  giant->Lock ();
  if (host != NULL && host->GetType () == THOST::ht_leader) {
    TLEADER *leader = dynamic_cast<TLEADER *>(host);
    leader->SendPlayerArray (selected_map_name, true);
  }
  giant->Unlock ();

#if HEADLESS
  TTIME clock;
  if (!StartGame (clock.GetActual ()))
    Warning ("request_start: StartGame failed");
  else {
    leader_ready = true;
    giant->Lock ();
    if (host != NULL && host->GetType () == THOST::ht_leader) {
      TLEADER *L = dynamic_cast<TLEADER *>(host);
      if (player_array.AllRemoteReady ()) {
        allowed_to_start_process_function = true;
        L->SendAllowProcessFunction ();
      }
    }
    giant->Unlock ();
  }
#else
  giant->Lock ();
  state = ST_GAME;
  giant->Unlock ();
#endif
}

/**
 *  Callback function which is called whenever a connect request is received.
 */
static void ProcessConnectRequest (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  in_port_t port;
  string player_name;

  msg->Extract (&port, sizeof (port));
  player_name = msg->ExtractString ();

  player_array.Lock ();

  dynamic_cast<TLEADER *>(host)->ConnectFollower (msg->GetAddress (), port, msg->GetFileDescriptor ());

  player_array.AddRemotePlayer (player_name, msg->GetAddress (), port);
  UpdatePlayersAndMenu ();

  player_array.Unlock ();

  giant->Unlock ();
}

/**
 *  Callback function which is called whenever a change of a race request is
 *  received.
 */
static void ProcessChangeRace (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  int index = msg->ExtractByte ();
  string name = msg->ExtractString ();
  string new_race = msg->ExtractString ();

  player_array.Lock ();

  if (player_array.GetPlayerName (index) != name) {
    Warning ("Players not synchronised");
    player_array.Unlock ();
    giant->Unlock ();
    return;
  }

  player_array.SetRaceIdName (index, new_race);

  UpdatePlayersAndMenu ();
  player_array.Unlock ();

  giant->Unlock ();
}

static void ProcessPlayerArray (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  double time;
  msg->Extract (&time, sizeof time);

  /* If the time, when the message originated is less greater than actual time,
   * we have wrong time and we'll correct it according to the received time. */
  if (AppGetTimeSeconds() < time)
    AppSetTimeSeconds(time);

  player_array.Lock ();

  TFOLLOWER *follower = dynamic_cast<TFOLLOWER *>(host);

  string net_map_name = msg->ExtractString ();    // Map name.
  uint32_t remote_map_hash;
  msg->Extract (&remote_map_hash, sizeof remote_map_hash);
  MenuUpdateMapInfo (net_map_name, remote_map_hash);
  T_BYTE player_count = msg->ExtractByte ();    // Count of players.

  /* Clear the array completely, remove hyper player too. */
  player_array.Clear ();
  player_array.RemovePlayer (0);

  int i;

  for (i = 0; i < player_count; i++) {
    string name = msg->ExtractString ();
    string race = msg->ExtractString ();
    bool computer = msg->ExtractByte () != 0;
    bool on_leader = !msg->ExtractByte ();
    int start_point = msg->ExtractByte ();

    if (on_leader) {
      player_array.AddRemotePlayer (name, follower->GetRemoteAddress (), follower->GetRemotePort (), computer);
    } else {
      in_addr addr;
      in_port_t port;

      msg->Extract (&addr, sizeof (in_addr));
      msg->Extract (&port, sizeof (in_port_t));

      /* Check, if the actual player is a local or a remote player.
       * Without HasMyAddress() we'd compare against an uninitialised 0.0.0.0:0
       * and accidentally classify the first player as remote on race conditions
       * where the leader's player_array message arrives before our address echo.
       */
      if (follower->HasMyAddress () &&
          TNET_RESOLVER::NetworkToAscii (addr) == TNET_RESOLVER::NetworkToAscii (follower->GetMyAddress ()) &&
          port == follower->GetMyPort ())
      {
        player_array.AddLocalPlayer (name, race, computer);
      } else
        player_array.AddRemotePlayer (name, addr, port, computer);
          
    }

    player_array.SetStartPoint (i, start_point);
    player_array.SetRaceIdName (i, race);
  }

  UpdatePlayersAndMenu ();

  /* Start the game if requested. */
  bool start_game = (msg->GetSubtype() == 1);
  if (start_game) {
    int my_id = player_array.GetMyPlayerID ();
    Debug (LogMsg ("My_id is %d", my_id));

    /* We are already connected to leader with remote_address 0, which is
     * hyperplayer. */

    /* Fill the follower's talker array of remote addresses with addresses of
     * all remote players' hosts. For local players add empty address. */
    for (i = 1; i < player_array.GetCount (); i++) {
      if (player_array.IsComputer (i)) {
        host->AddEmptyAddress ();
        Debug (LogMsg ("%d: Adding empty address (computer)", i));
      } else if (i < my_id) {
        /* Wait until player with id < my_id gets connected to me. */
        int fd;

        for (int j = 0; j < 180; j++) {
          fd = host->GetListener()->GetListenersFileDescriptor (player_array.GetAddress (i));
          if (fd != -1)
            break;
          Debug ("Still do not have that address");
          AppSleepSeconds(0.5);
        }

        if (fd != -1) {
          host->AddRemoteAddress (player_array.GetAddress (i), player_array.GetPort (i), fd);
          Debug (LogMsg ("%d: Adding address with file descriptor", i));
        } else {
          Error (LogMsg ("%d: host not connected in 15 seconds...", i));
        }
      } else if (player_array.IsRemote (i)) {
        host->AddRemoteAddress (player_array.GetAddress (i), player_array.GetPort (i));
        host->GetListener ()->AddListenerByFileDescriptor (player_array.GetAddress (i), player_array.GetPort (i), host->GetTalker ()->GetRemoteFiledescriptor (i));
        Debug (LogMsg ("%d: Adding remote address", i));
      } else {
        host->AddEmptyAddress ();
        Debug (LogMsg ("%d: Adding empty address", i));
      }
    }

    state = ST_GAME;
  }

  player_array.Unlock ();
  giant->Unlock ();
}

static void ProcessHello (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  double received = AppGetTimeSeconds();

  TFOLLOWER *follower = dynamic_cast<TFOLLOWER *>(host);

  in_addr my_address;
  in_port_t my_port;

  msg->Extract (&my_address, sizeof my_address);
  msg->Extract (&my_port, sizeof my_port);

  double time;
  msg->Extract (&time, sizeof time);

  /* Time shift is time that the request took divided by 2 (we beleive both
   * parts of the communication took the same amount of time. */
  double time_shift = (received - follower->GetPingRequestTime ()) / 2;
  AppSetTimeSeconds(time + time_shift);
  follower->SetMinimalTimeshift (time_shift);

  Debug (LogMsg ("Reply from Leader received in %.2f miliseconds", time_shift * 2000));

  TNET_RESOLVER resolver;

  follower->SetMyAddress (my_address, my_port);

  giant->Unlock ();
}

static void ProcessPingRequest (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  TLEADER *leader = dynamic_cast<TLEADER *>(host);

  double request_time;
  msg->Extract (&request_time, sizeof request_time);

  leader->SendPingReply (request_time);

  giant->Unlock ();
}

static void ProcessPingReply (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  double received = AppGetTimeSeconds();

  TFOLLOWER *follower = dynamic_cast<TFOLLOWER *>(host);

  double request_time;
  double time;

  msg->Extract (&request_time, sizeof request_time);
  msg->Extract (&time, sizeof time);

  /* Time shift is time that the request took divided by 2 (we beleive both
   * parts of the communication took the same amount of time. */
  if (request_time == follower->GetPingRequestTime ()) {
    double time_shift = (received - follower->GetPingRequestTime ()) / 2;

    if (time_shift < follower->GetMinimalTimeshift ()) {
      AppSetTimeSeconds(time + time_shift);
      follower->SetMinimalTimeshift (time_shift);
      Debug (LogMsg ("Ping reply from Leader received in %.2f miliseconds", time_shift * 2000));
    }
  }

  giant->Unlock ();
}

static void ProcessChatMessage (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  player_array.Lock ();
  
  for (int id = 1; (id = player_array.GetPlayerID (msg->GetAddress (), msg->GetPort (), id)) != -1; id++) {
    if (!player_array.IsComputer (id)) {
      string name = player_array.GetPlayerName (id);

      string message = name + ": " + msg->ExtractString ();

      ost->AddText(message.c_str ());

      break;
    }
  }

  player_array.Unlock ();

  giant->Unlock ();
}

static void ProcessNetEvent (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  /* Net events can arrive in lobby (before StartGame); pool is normally created there. */
  if (!pool_events) {
    pool_events = NEW TPOOL<TEVENT>(2 * EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);
  }

  int size = msg->GetSize();
  char data[128];
  TEVENT *pevent;

  msg->Extract(data, size);
  
  pevent = pool_events->GetFromPool();
  pevent->DelinearizeEvent(data, size);
  queue_events->PutEvent(pevent);

  giant->Unlock ();
}

static void ProcessSynchronise (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  player_array.PlayerReady (msg->GetAddress (), msg->GetPort ());

  if (leader_ready && player_array.AllRemoteReady ()) {
    allowed_to_start_process_function = true;

    TLEADER *leader = dynamic_cast<TLEADER *>(host);
    leader->SendAllowProcessFunction ();

    gui->HideMessageBox ();
  }

  giant->Unlock ();
}

static void ProcessAllowProcessFunction (TNET_MESSAGE *msg) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  allowed_to_start_process_function = true;
  gui->HideMessageBox ();

  giant->Unlock ();
}

static void ProcessDisconnect (TNET_MESSAGE *msg) {
  T_BYTE player_id = msg->ExtractByte();
  
  ProcessDisconnect(player_id);
}

static void ProcessDisconnect (int player_id) {
  giant->Lock ();

  if (host == NULL) {
    giant->Unlock ();
    return;
  }

  if (!players[player_id]->active) {
    Debug(LogMsg("Not disconnecting player %d (%s) - he is not active.",
          player_id, player_array.GetPlayerName (player_id).c_str ()));

    giant->Unlock ();
    return;
  }
  bool exists_player;

  if (player_array.IsRemote(player_id)) {
    players[player_id]->Disconnect();
  }
  
  players[player_id]->active = false;

  Info(LogMsg("Player %d (%s) was disconnected.",
       player_id, player_array.GetPlayerName (player_id).c_str ()));

  /* when hyper player was disconnected, dont write "You won!" message */
  if (player_id == 0) {
    giant->Unlock ();
    return;
  }

  exists_player = false;
  for (int i = 0; i < player_array.GetCount(); i++) {
    if ((players[i] != hyper_player) && (players[i] != myself) && (players[i]->active == true)){
      exists_player = true;
      break;
    }
  }

  if (!exists_player && !won_lose) {
    action_key = 0;
    gui->ShowMessageBox("You WON!", GUI_MB_OK);
    allowed_to_start_process_function = true;
    won_lose = true;
  }

  giant->Unlock ();
}

struct TDISCONNECT_DATA {
  in_addr address;
  in_port_t port;
};

static int SDLCALL OnDisconnectThread (void *d) {
  TDISCONNECT_DATA *data = static_cast<TDISCONNECT_DATA *>(d);

  Debug ("someone disconnected");

  giant->Lock ();

  if (host == NULL) {
    Debug ("But there is no host");
    giant->Unlock ();
    delete data;
    return 0;
  }

  if (state == ST_PLAY_MENU) {
    Debug ("in menu");

    if (host->GetType () == THOST::ht_follower) {
      action_force = true;
      MenuButtonOnClickKey (MNU_DISCONNECT);

      action_key = 0;
      gui->ShowMessageBox ("You were disconnected!", GUI_MB_OK);
    } else {
      player_array.Lock ();

      int player_id = player_array.GetPlayerID (data->address, data->port, 0);

      if (player_id != -1) {
        string player_name = player_array.GetPlayerName (player_id);

        Info (LogMsg ("Player %d (%s) disconnected", player_id, player_name.c_str ()));

        host->RemoveAddress (player_id);
        player_array.RemovePlayer (player_id);
        UpdatePlayersAndMenu ();
      }

      player_array.Unlock ();
    }
  } else if (state == ST_GAME) {
      Debug ("in game");

      player_array.Lock ();

      for (int id = 0; (id = player_array.GetPlayerID (data->address, data->port, id)) != -1; id++) {
        host->DisconnectAddress (id);
        ProcessDisconnect (id);
      }    

      player_array.Unlock ();
  }

  giant->Unlock ();
  delete data;
  return 0;
}

static void OnDisconnect (in_addr address, in_port_t port) {
  TDISCONNECT_DATA *data = NEW TDISCONNECT_DATA;

  data->address = address;
  data->port = port;

  if (SDL_CreateThread (OnDisconnectThread, "ondisc", data) == NULL)
    Critical ("Could not create OnDisconnect thread");
}


//========================================================================
// Creating GUI
//========================================================================

/**
 *  Clears gui variables.
 */
void ClearGuiVars()
{
  int i;

  main_menu = NULL;
  play_menu = NULL;
  ip_menu = NULL;
  game_menu = NULL;
  options_menu = NULL;
  video_menu = NULL;
  audio_menu = NULL;
  credits_menu = NULL;
  active_menu = NULL;

  main_panel = NULL;
  little_panel = NULL;
  chat_panel = NULL;
  dev_console_panel = NULL;
  radar_panel = NULL;

  chat_edit = NULL;
  dev_console_edit = NULL;

  ip_edit = NULL;
  ip_label = NULL;
  connect_button = NULL;
  create_button = NULL;

  map_list = NULL;
  map_label = NULL;
  play_button = NULL;
  disconn_button = NULL;

  resume_button = NULL;
  disconnect_button = NULL;

  map_scheme_label = NULL;
  map_size_label = NULL;
  map_players_label = NULL;
  map_races_label = NULL;
  map_author_label = NULL;

  for (i = 0; i < PL_MAX_PLAYERS; i++) {
    pl_name_label[i] = NULL;
    pl_race_combo[i] = NULL;
    pl_kill_button[i] = NULL;
  }

  load_panel = NULL;

#if !HEADLESS
  editor_menu = NULL;
  editor_map_list = NULL;
  editor_new_name_edit = NULL;
  editor_size_combo = NULL;
  editor_frag_list = NULL;
#endif
}


/**
 *  Creates main menu GUI.
 */
void CreateMenuGUI()
{
  TGUI_PANEL *panel;
  TGUI_BUTTON *button;
  TGUI_CHECK_BOX *check;
  TGUI_LABEL *label;
  TGUI_LIST_BOX *list;
  TGUI_EDIT_BOX *edit;
  TGUI_COMBO_BOX *combo;
  TGUI_SCROLL_BOX *scroll;

  int i;

#if SOUND
  TGUI_SLIDER *slider;
#endif

  GLfloat y;
  GLfloat check_width = 160.0f;

  // gui
  gui->SetSize((GLfloat)config.scr_width, (GLfloat)config.scr_height);
  gui->SetFont(font0);
  gui->SetFontColor(1, 1, 1);
  gui->SetColor(0.75f, 0.7f, 0.6f);

  panel = gui->AddPanel(
    0, 0, 0, 
    (GLfloat)config.scr_width, (GLfloat)config.scr_height,
    gui_table.GetTexture(DAT_TGID_PANELS, 0)
  );

  // main menu
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 128 + 40);
  else y = GLfloat(config.scr_height / 2 - 128 + 10);

  main_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 128), y, 256, 240);
  panel->SetPadding(0.0f);
  panel->SetAlpha(0.0f);
  panel->SetVisible(false);

  button = panel->AddButton(MNU_PLAY, 0, 205, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 0));
  SetMenuButton(true);

  button = panel->AddButton(MNU_QUICK_PLAY, 0, 175, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 1));
  SetMenuButton(true);

  button = panel->AddButton(MNU_OPTIONS, 0, 145, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 2));
  SetMenuButton(true);
  
  button = panel->AddButton(MNU_CREDITS, 0, 115, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 3));
  SetMenuButton(true);

  button = panel->AddButton(MNU_MAP_EDITOR, 18, 75, 220, 24, "MAP EDITOR");
  SetMenuButton(true);
  button->SetFontColor(0, 0, 0);
  
  button = panel->AddButton(MNU_QUIT, 0, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 4));
  SetMenuButton(true);
  
  // play menu
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 128 + 40);
  else y = GLfloat(config.scr_height / 2 - 128 + 10);

  play_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 128), y, 256, 240);
  panel->SetPadding(0.0f);
  panel->SetAlpha(0.0f);
  panel->SetVisible(false);

  resume_button = button = panel->AddButton(MNU_RESUME, 0, 190, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 5));
  SetMenuButton(connected);

  button = panel->AddButton(MNU_CREATE, 0, 155, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 6));
  SetMenuButton(true);

  button = panel->AddButton(MNU_CONNECT, 0, 120, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 7));
  SetMenuButton(true);

  disconnect_button = button = panel->AddButton(MNU_DISCONNECT, 0, 85, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 8));
  SetMenuButton(connected);

  button = panel->AddButton(MNU_MAIN, 0, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 9));
  SetMenuButton(true);

  // ip menu
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 75 - 50);
  else y = GLfloat(config.scr_height / 2 - 85);

  ip_menu = panel = gui->AddPanel(MNU_CONNECT, GLfloat(config.scr_width / 2 - 250), y, 500, 170);
  SetMenuPanel();

  panel->AddLabel(0, 30, 110, gui_table.GetTexture(DAT_TGID_LABELS, 10));
  ip_label = panel->AddLabel(0, 110, 110, TNET_RESOLVER::GetHostName ().c_str ());

  ip_edit = edit = panel->AddEditBox(MNU_IP, 110, 107, 170, 20, PL_MAX_ADDRESS_LENGTH);
  edit->SetFontColor(0, 0, 0);
  edit->SetText(config.address);
  edit->SetOnChange(MenuEditOnChange);

  panel->AddLabel(0, 30, 80, gui_table.GetTexture(DAT_TGID_LABELS, 9));
  edit = panel->AddEditBox(MNU_PLAYER_NAME, 110, 77, 170, 20, PL_MAX_PLAYER_NAME_LENGTH);
  edit->SetFontColor(0, 0, 0);
  edit->SetText(config.player_name);
  edit->SetOnChange(MenuEditOnChange);

  button = panel->AddButton(MNU_PLAY, 0, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 9));
  SetMenuButton(true);
  connect_button = button = panel->AddButton(MNU_CONNECT2, 244, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 7));
  SetMenuButton(false);
  connect_button->SetEnabled(*config.player_name > 0);
  create_button = button = panel->AddButton(MNU_CREATE2, 244, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 6));
  SetMenuButton(true);

  // game menu
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 230 - 50);
  else y = GLfloat(config.scr_height / 2 - 230);

  game_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 285), y, 570, 460);
  SetMenuPanel();
  panel->SetOnShow(MenuPanelOnShow);

  disconn_button = button = panel->AddButton(MNU_DISCONNECT, 20, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 8));
  SetMenuButton(true);
  play_button = button = panel->AddButton(MNU_PLAY2, 280, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 0));
  SetMenuButton(true);

  panel = panel->AddPanel(0, 0, 60, 570, 480);
  panel->SetPadding(0.0f);
  panel->SetAlpha(0);

  panel->AddLabel(0, 30, 353, gui_table.GetTexture(DAT_TGID_LABELS, 7));
  map_list = list = panel->AddListBox(MNU_MAP_LIST, 30, 185, 215, 150);
  list->SetFontColor(0, 0, 0);
  list->SetOnChange(MenuListOnChange);

  map_label = panel->AddLabel(0, 30, 320, "");

  panel->AddLabel(0, 265, 350, gui_table.GetTexture(DAT_TGID_LABELS, 8));
  scroll = map_info_scroll = panel->AddScrollBox(0, 260, 185, 285, 150);
  scroll->HideSlider(GUI_ST_HORIZONTAL);
  scroll->SetAlpha(0);

  scroll->AddLabel(0, 0, 120, "Author:");
  map_author_label =  scroll->AddLabel(0, 65, 120, "");
  scroll->AddLabel(0, 0, 100, "Scheme:");
  map_scheme_label =  scroll->AddLabel(0, 65, 100, "");
  scroll->AddLabel(0, 0, 80, "Size:");
  map_size_label =    scroll->AddLabel(0, 65, 80, "");
  scroll->AddLabel(0, 0, 60, "Players:");
  map_players_label = scroll->AddLabel(0, 65, 60, "");
  scroll->AddLabel(0, 0, 40, "Races:");
  map_races_label =   scroll->AddLabel(0, 65, 40, "");

  panel->AddLabel(0, 30, 150, gui_table.GetTexture(DAT_TGID_LABELS, 11));

  for (i = 1; i < PL_MAX_PLAYERS; i++) {
    pl_name_label[i] = panel->AddLabel(0, 30, 126 - GLfloat(i * 18), "");

    pl_race_combo[i] = combo = panel->AddComboBox(0, 325, 126 - GLfloat(i * 18), 200, 17);
    combo->SetItems("");
    combo->SetPadding(3.0f);
    combo->SetFontColor(0, 0, 0);
    combo->SetOnChange (MenuPlayerRaceComboOnChange);

    pl_kill_button[i] = button = panel->AddButton(MNU_KILL_PLAYER, 530, 126 - GLfloat(i * 18), 16, 16, "X");
    button->SetOnMouseClick(MenuButtonOnClick);
    button->SetFontColor(0, 0, 0);
  }

  add_comp_button = button = panel->AddButton(MNU_ADD_COMPUTER, 446, 150, 100, 16, "Add computer");
  button->SetFontColor(0, 0, 0);
  button->SetOnMouseClick(MenuButtonOnClick);

  // options menu
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 128 + 40);
  else y = GLfloat(config.scr_height / 2 - 128 + 10);

  options_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 128), y, 256, 240);
  panel->SetPadding(0.0f);
  panel->SetAlpha(0.0f);
  panel->SetVisible(false);

  button = panel->AddButton(MNU_VIDEO, 0, 190, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 10));
  SetMenuButton(true);

  button = panel->AddButton(MNU_AUDIO, 0, 155, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 11));
#if SOUND
  SetMenuButton(true);
#else
  SetMenuButton(false);
#endif

  button = panel->AddButton(MNU_MAIN, 0, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 9));
  SetMenuButton(true);

  // video options
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 230 - 50);
  else y = GLfloat(config.scr_height / 2 - 230);

  video_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 250), y, 500, 460);
  SetMenuPanel();

  panel->AddLabel(0, 30, 400, gui_table.GetTexture(DAT_TGID_LABELS, 0));

  check = panel->AddCheckBox(MNU_FULLSCREEN, 50, 370, check_width, 17, "fullscreen *");
  check->SetChecked(config.fullscreen);
  check->SetOnMouseClick(MenuCheckBoxOnClick);

  check = panel->AddCheckBox(MNU_VERT_SYNC, 50, 350, check_width, 17, "v. synchronisation");
  check->SetChecked(config.vert_sync);
  check->SetOnMouseClick(MenuCheckBoxOnClick);

  panel->AddLabel(0, 30, 300, gui_table.GetTexture(DAT_TGID_LABELS, 1));

  // get supported video modes
  {
    GLFWvidmode *vid_modes = NEW GLFWvidmode[MAX_VID_MODES];
    int vid_modes_count = glfwGetVideoModes(vid_modes, MAX_VID_MODES);
    GLfloat y = 270.0f;
    GLfloat x = 50.0f;
    int w = 0;

    for (int i = 0; i < vid_modes_count; i++)
      if (vid_modes[i].RedBits == 8 && vid_modes[i].GreenBits == 8 && vid_modes[i].BlueBits == 8) {
        if (vid_modes[i].Width == 640 && vid_modes[i].Height == 480) {
          check = panel->AddGroupBox(MNU_640, x, y, check_width, 17, "640x480", 1);
          w = 640;
        }
        else if (vid_modes[i].Width == 800 && vid_modes[i].Height == 600) {
          check = panel->AddGroupBox(MNU_800, x, y, check_width, 17, "800x600", 1);
          w = 800;
        }
        else if (vid_modes[i].Width == 1024 && vid_modes[i].Height == 768) {
          check = panel->AddGroupBox(MNU_1024, x, y, check_width, 17, "1024x768", 1);
          w = 1024;
        }
        else if (vid_modes[i].Width == 1280 && vid_modes[i].Height == 1024) {
          check = panel->AddGroupBox(MNU_1280, x, y, check_width, 17, "1280x1024", 1);
          w = 1280;
        }
        else if (vid_modes[i].Width == 1152 && vid_modes[i].Height == 864) {
          check = panel->AddGroupBox(MNU_1152, x, y, check_width, 17, "1152x864", 1);
          w = 1152;
        }
        else if (vid_modes[i].Width == 1600 && vid_modes[i].Height == 1200) {
          check = panel->AddGroupBox(MNU_1600, x, y, check_width, 17, "1600x1200", 1);
          w = 1600;
        }
        else if (vid_modes[i].Width == 1920 && vid_modes[i].Height == 1080) {
          check = panel->AddGroupBox(MNU_1920, x, y, check_width, 17, "1920x1080", 1);
          w = 1920;
        }

        if (w) {
          check->SetChecked(config.scr_width == w);
          check->SetOnMouseClick(MenuCheckBoxOnClick);
          w = 0;
          y -= 20.0f;
        }
      }

    delete[] vid_modes;
  }

  panel->AddLabel(0, 240, 400, gui_table.GetTexture(DAT_TGID_LABELS, 2));
  panel->AddLabel(0, 438, 400, "**");

  check = panel->AddGroupBox(MNU_TF_NEAREST, 260, 370, check_width, 18, "nearest", 2);
  check->SetChecked(config.tex_mag_filter == GL_NEAREST);
  check->SetOnMouseClick(MenuCheckBoxOnClick);
  check = panel->AddGroupBox(MNU_TF_LINEAR, 260, 350, check_width, 18, "linear", 2);
  check->SetChecked(config.tex_mag_filter == GL_LINEAR);
  check->SetOnMouseClick(MenuCheckBoxOnClick);

  panel->AddLabel(0, 240, 300, gui_table.GetTexture(DAT_TGID_LABELS, 3));
  panel->AddLabel(0, 425, 300, "**");

  check = panel->AddGroupBox(MNU_MF_NONE, 260, 270, check_width, 18, "none", 3);
  check->SetChecked(config.tex_min_filter == config.tex_mag_filter);
  check->SetOnMouseClick(MenuCheckBoxOnClick);
  check = panel->AddGroupBox(MNU_MF_NEAREST, 260, 250, check_width, 18, "nearest", 3);
  check->SetChecked(config.tex_min_filter == GL_NEAREST_MIPMAP_NEAREST || config.tex_min_filter == GL_LINEAR_MIPMAP_NEAREST);
  check->SetOnMouseClick(MenuCheckBoxOnClick);
  check = panel->AddGroupBox(MNU_MF_LINEAR, 260, 230, check_width, 18, "linear", 3);
  check->SetChecked(config.tex_min_filter == GL_NEAREST_MIPMAP_LINEAR || config.tex_min_filter == GL_LINEAR_MIPMAP_LINEAR);
  check->SetOnMouseClick(MenuCheckBoxOnClick);

  label = panel->AddLabel(0, 30, 80, " * restart application to apply this option");
  label->SetColor(0.5f, 0.5f, 0.5f);
  label = panel->AddLabel(0, 30, 65, "** start new game to apply this option");
  label->SetColor(0.5f, 0.5f, 0.5f);

  button = panel->AddButton(MNU_OPTIONS, 122, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 9));
  SetMenuButton(true);

#if SOUND

  // audio options
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 230 - 50);
  else y = GLfloat(config.scr_height / 2 - 230);

  audio_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 250), y, 500, 460);
  SetMenuPanel();

  panel->AddLabel(0, 30, 400, gui_table.GetTexture(DAT_TGID_LABELS, 4));
  panel->AddLabel(0, 50, 370, "volume");
  slider = panel->AddSlider(MNU_MASTER_VOL, 180, 370, 280, 17, GUI_ST_HORIZONTAL);
  slider->SetPosition(config.snd_master_volume / 100.0f);
  slider->SetOnChange(MenuSliderOnChange);

  panel->AddLabel(0, 30, 320, gui_table.GetTexture(DAT_TGID_LABELS, 5));
  check = panel->AddCheckBox(MNU_MENU_MUSIC, 50, 290, check_width, 17, "play music");
  check->SetChecked(config.snd_menu_music);
  check->SetOnMouseClick(MenuCheckBoxOnClick);

  panel->AddLabel(0, 50, 260, "sound volume");
  slider = panel->AddSlider(MNU_MENU_SOUND_VOL, 180, 260, 280, 17, GUI_ST_HORIZONTAL);
  slider->SetPosition(config.snd_menu_sound_volume / 100.0f);
  slider->SetOnChange(MenuSliderOnChange);

  panel->AddLabel(0, 50, 240, "music volume");
  slider = panel->AddSlider(MNU_MENU_MUSIC_VOL, 180, 240, 280, 17, GUI_ST_HORIZONTAL);
  slider->SetPosition(config.snd_menu_music_volume / 100.0f);
  slider->SetOnChange(MenuSliderOnChange);

  panel->AddLabel(0, 30, 190, gui_table.GetTexture(DAT_TGID_LABELS, 6));
  check = panel->AddCheckBox(MNU_GAME_MUSIC, 50, 160, check_width, 17, "play music");
  check->SetChecked(config.snd_game_music);
  check->SetOnMouseClick(MenuCheckBoxOnClick);
  check = panel->AddCheckBox(MNU_UNIT_SPEECH, 50, 140, check_width, 17, "play unit speech");
  check->SetChecked(config.snd_unit_speech);
  check->SetOnMouseClick(MenuCheckBoxOnClick);

  panel->AddLabel(0, 50, 110, "sound volume");
  slider = panel->AddSlider(MNU_GAME_SOUND_VOL, 180, 110, 280, 17, GUI_ST_HORIZONTAL);
  slider->SetPosition(config.snd_game_sound_volume / 100.0f);
  slider->SetOnChange(MenuSliderOnChange);

  panel->AddLabel(0, 50, 90, "music volume");
  slider = panel->AddSlider(MNU_GAME_MUSIC_VOL, 180, 90, 280, 17, GUI_ST_HORIZONTAL);
  slider->SetPosition(config.snd_game_music_volume / 100.0f);
  slider->SetOnChange(MenuSliderOnChange);

  button = panel->AddButton(MNU_OPTIONS, 122, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 9));
  SetMenuButton(true);

#endif

  // credids
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 230 - 50);
  else y = GLfloat(config.scr_height / 2 - 230);

  credits_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 250), y, gui_table.GetTexture(DAT_TGID_PANELS, 2));
  SetMenuPanel();
  panel->SetColor(1, 1, 1);

  button = panel->AddButton(MNU_MAIN, 122, 15, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 9));
  SetMenuButton(true);

  // messagebox
  panel = gui->GetMessageBox();
  SetMenuPanel();
  panel->SetPadding(15);

  button = gui->GetMessageBox()->GetButton(GUI_MB_OK);
  button->SetTexture(GUI_BS_UP, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 12));
  button->SetCaption(NULL);
  SetMenuButton(true);

  button = gui->GetMessageBox()->GetButton(GUI_MB_YES);
  button->SetTexture(GUI_BS_UP, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 13));
  button->SetCaption(NULL);
  SetMenuButton(true);

  button = gui->GetMessageBox()->GetButton(GUI_MB_NO);
  button->SetTexture(GUI_BS_UP, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 14));
  button->SetCaption(NULL);
  SetMenuButton(true);

  button = gui->GetMessageBox()->GetButton(GUI_MB_CANCEL);
  button->SetTexture(GUI_BS_UP, gui_table.GetTexture(DAT_TGID_MENU_BUTTONS, 15));
  button->SetCaption(NULL);
  SetMenuButton(true);

  {
    char build_caption[96];
    TGUI_LABEL *build_lbl;
    snprintf(build_caption, sizeof(build_caption), "%s  %s",
             DO_VERSION_STRING, DO_GIT_REVISION);
    build_lbl = gui->AddLabel(0, (GLfloat)(config.scr_width - 280), 8.0f, build_caption);
    build_lbl->SetFontColor(0.58f, 0.54f, 0.5f);
    build_lbl->SetLineHeight(14.0f);
    build_lbl->SetAlpha(0.9f);
  }

#if !HEADLESS
  if (config.scr_height >= 600) y = GLfloat(config.scr_height / 2 - 160 - 50);
  else y = GLfloat(config.scr_height / 2 - 160);

  editor_menu = panel = gui->AddPanel(0, GLfloat(config.scr_width / 2 - 220), y, 440, 320);
  SetMenuPanel();
  panel->SetOnShow(EditorMenuOnShow);

  panel->AddLabel(0, 20, 265, "Maps");
  editor_map_list = list = panel->AddListBox(MNU_EDITOR_MAP_LIST, 20, 95, 200, 150);
  list->SetFontColor(0, 0, 0);
  list->SetOnChange(EditorMapListOnChange);

  button = panel->AddButton(MNU_EDITOR_OPEN, 230, 268, 90, 22, "Open");
  SetMenuButton(true);
  button->SetFontColor(0, 0, 0);

  panel->AddLabel(0, 20, 70, "New map id");
  editor_new_name_edit = edit = panel->AddEditBox(0, 20, 48, 200, 18, MAP_MAX_NAME_LENGTH - 1);
  edit->SetFontColor(0, 0, 0);

  panel->AddLabel(0, 240, 70, "Size");
  editor_size_combo = combo = panel->AddComboBox(0, 240, 48, 90, 18);
  combo->SetItems("80\n128\n160");
  combo->SetFontColor(0, 0, 0);
  combo->SetPadding(3.0f);

  button = panel->AddButton(MNU_EDITOR_NEW, 335, 48, 95, 22, "Create");
  SetMenuButton(true);
  button->SetFontColor(0, 0, 0);

  button = panel->AddButton(MNU_EDITOR_BACK, 20, 15, 100, 22, "Back");
  SetMenuButton(true);
  button->SetFontColor(0, 0, 0);
#endif

  // activate menu
  if (state == ST_PLAY_MENU) active_menu = play_menu;
  else if (state == ST_VIDEO_MENU) active_menu = video_menu;
  else active_menu = main_menu;

  active_menu->SetVisible(true);
}


/**
 *   Creates game GUI.
 */
void CreateGameGUI()
{
  TGUI_PANEL *panel;
  TGUI_LABEL *label;
  TGUI_BUTTON *button;
  TGUI_EDIT_BOX *edit;
  TGUI_SCROLL_BOX *scroll;
  TGUI_SLIDER *slider;
  int i;

  gui->SetFont(font0);
  gui->SetSize((GLfloat)config.scr_width, (GLfloat)config.scr_height);
  gui->SetFontColor(1, 0.93f, 0.82f);
  gui->SetOnMouseDown(GameOnMouseDown);
  gui->SetOnMouseUp(GameOnMouseUp);
  gui->GetDefTooltip()->SetColor(0, 0, 0);
  gui->GetDefTooltip()->SetAlpha(TOOLTIP_ALPHA);

  // little panel
  little_panel = panel = gui->AddPanel(0, 0, 0, GLfloat(config.scr_width - 200), 20);
  SetGamePanel(true);

  // materials
  for (i = 0; i < scheme.materials_count; i++) {
    label = panel->AddLabel(0, GLfloat(15 + i * 65), 4, 15, 12);
    label->SetTexture(scheme.tex_table.GetTexture(scheme.materials[i]->tg_id, 0));
    label->SetColor(1, 1, 1);
    label->SetOnMouseUp(GameOnMouseUp);

    panel_info.material_label[i] = label = panel->AddLabel(0, GLfloat(34 + i * 65), 1, "0");
    label->SetOnMouseUp(GameOnMouseUp);
  }

  // food, energy
  label = panel->AddLabel(0, little_panel->GetWidth() - 200, 4, 15, 12);
  label->SetTexture(myself->race->tex_table.GetTexture(myself->race->tg_food_id, 0));
  label->SetColor(1, 1, 1);
  label->SetOnMouseUp(GameOnMouseUp);

  panel_info.food_label = label = panel->AddLabel(0, little_panel->GetWidth() - 180, 1, "0");
  label->SetOnMouseUp(GameOnMouseUp);

  label = panel->AddLabel(0, little_panel->GetWidth() - 100, 4, 15, 12);
  label->SetTexture(myself->race->tex_table.GetTexture(myself->race->tg_energy_id, 0));
  label->SetColor(1, 1, 1);
  label->SetOnMouseUp(GameOnMouseUp);

  panel_info.energy_label = label = panel->AddLabel(0, little_panel->GetWidth() - 80, 1, "0");
  label->SetOnMouseUp(GameOnMouseUp);

  // chat panel
  chat_panel = panel = gui->AddPanel(0, 0, 0, GLfloat(config.scr_width - 200), 20);
  SetGamePanel(false);
  
  label = panel->AddLabel(0, 10, 2, "Say:");
  label->SetOnMouseUp(GameOnMouseUp);

  chat_edit = edit = panel->AddEditBox(0, 40, 2, panel->GetWidth() - 40, 17, 256);
  edit->SetAlpha(0);
  edit->SetPadding(0);
  edit->SetOnMouseUp(GameOnMouseUp);

  // developer console (local commands only; key `)
  dev_console_panel = panel = gui->AddPanel(0, 0, 22, GLfloat(config.scr_width - 200), 20);
  SetGamePanel(false);

  label = panel->AddLabel(0, 10, 2, "Dev:");
  label->SetOnMouseUp(GameOnMouseUp);

  dev_console_edit = edit = panel->AddEditBox(0, 40, 2, panel->GetWidth() - 40, 17, 256);
  edit->SetAlpha(0);
  edit->SetPadding(0);
  edit->SetOnMouseUp(GameOnMouseUp);

  // main panel
  main_panel = panel = gui->AddPanel(0, GLfloat(config.scr_width - 200), 0, 200, GLfloat(config.scr_height), gui_table.GetTexture(DAT_TGID_PANELS, 1));
  panel->SetAlpha(GAME_PANEL_ALPHA);
  panel->SetOnMouseUp(GameOnMouseUp);

  // unit picture
  panel_info.picture_image = label = panel->AddLabel(0, 15, GLfloat(config.scr_height - 235), 50, 40);
  label->SetColor(1, 1, 1);
  label->SetOnMouseUp(GameOnMouseUp);

  // unit info
  panel_info.name_label = label = panel->AddLabel(0, 75, GLfloat(config.scr_height - 215), "Unit name");
  label->SetFontColor(1, 0.93f, 0.82f);
  label->SetOnMouseUp(GameOnMouseUp);

  panel_info.life_label = label = panel->AddLabel(0, 75, GLfloat(config.scr_height - 227), "Unit life");
  label->SetFontColor(1, 0.93f, 0.82f);
  label->SetOnMouseUp(GameOnMouseUp);

  panel_info.hided_label = label = panel->AddLabel(0, 75, GLfloat(config.scr_height - 239), "Hided units");
  label->SetFontColor(1, 0.93f, 0.82f);
  label->SetOnMouseUp(GameOnMouseUp);

  panel_info.info_label = label = panel->AddLabel(0, 15, GLfloat(config.scr_height - 255), "Unit info");
  label->SetFontColor(0.7f, 0.6f, 0.4f);
  label->SetLineHeight(12);
  label->SetOnMouseUp(GameOnMouseUp);

  // unit materials
  for (i = 0; i < scheme.materials_count; i++) {
    panel_info.material_image[i] = label = panel->AddLabel(0, GLfloat(100 + i * 20), 333, 15, 12);
    label->SetTexture(scheme.tex_table.GetTexture(scheme.materials[i]->tg_id, 0));
    label->SetColor(1, 1, 1);
    label->SetOnMouseUp(GameOnMouseUp);
  }

  // progress info
  panel_info.progress_label = label = panel->AddLabel(0, 15, 12, "Progress");
  label->SetColor(1, 0.93f, 0.82f);
  label->SetOnMouseUp(GameOnMouseUp);

  // action panels
  for (i = 0; i < MNU_PANELS_COUNT; i++) {
    panel_info.action_panel[i] = scroll = main_panel->AddScrollBox(0, 15, 40, 170, GLfloat(config.scr_height - 412));
    scroll->SetColor(0, 0, 0);
    scroll->SetAlpha(0.3f);
    scroll->SetPadding(3);
    scroll->HideSlider(GUI_ST_HORIZONTAL);
    scroll->SetOnMouseUp(GameOnMouseUp);
    scroll->Hide();

    slider = scroll->GetVSlider();
    slider->SetWidth(10);
    slider->SetFaceColor(0.3f, 0.3f, 0.3f);
    slider->SetHoverColor(0.45f, 0.4f, 0.3f);
    slider->SetAlpha(0.3f);
    slider->SetOnMouseUp(GameOnMouseUp);
  }
  panel_info.act_action_panel = NULL;
  panel_info.act_build_button = NULL;
  last_item = NULL;

  // quard buttons
  {
    GLfloat y = panel_info.action_panel[MNU_PANEL_STAY]->GetClientHeight() - gui_table.GetTexture(DAT_TGID_GUARD_BUTTONS, 0)->frame_height;
    GLfloat x = 0.0f;

    for (i = 0; i < RAC_AGGRESIVITY_MODE_COUNT; i++) {
      button = panel_info.guard_button[i] = panel_info.action_panel[MNU_PANEL_STAY]->AddGroupButton(
        MNU_GUARD_BUTTON + i, x, y, gui_table.GetTexture(DAT_TGID_GUARD_BUTTONS, i), 4
      );
      SetStayButton();

      switch (i) {
        case AM_IGNORE:       button->SetTooltipText("Stay (F1)");              break;
        case AM_GUARDED:      button->SetTooltipText("Guard (F2)");             break;
        case AM_OFFENSIVE:    button->SetTooltipText("Offensive Guard (F3)");   break;
        case AM_AGGRESSIVE:   button->SetTooltipText("Aggressive Guard (F4)");  break;
        default:              break;
      }

      x += button->GetWidth() + 2;
      if (x + button->GetWidth() > panel_info.action_panel[MNU_PANEL_STAY]->GetClientWidth())
      {
        y -= button->GetHeight() + 2;
        x = 0.0f;
      }
    }

    panel_info.act_guard_button = NULL;
  }

  // build tooltip
  build_tooltip = NEW TBUILD_TOOLTIP();
  build_tooltip->SetColor(0, 0, 0);
  build_tooltip->SetAlpha(TOOLTIP_ALPHA);

  // order panel
  if (config.scr_height > 480)
    panel_info.order_panel = scroll = main_panel->AddScrollBox(0, 15, 40, 170, 56);
  else
    panel_info.order_panel = scroll = main_panel->AddScrollBox(0, 138, 40, 47, 56);
  scroll->SetColor(0, 0, 0);
  scroll->SetAlpha(0.3f);
  scroll->SetPadding(3);
  scroll->HideSlider(GUI_ST_VERTICAL);
  scroll->SetOnMouseUp(GameOnMouseUp);

  slider = scroll->GetHSlider();
  slider->SetHeight(10);
  slider->SetFaceColor(0.3f, 0.3f, 0.3f);
  slider->SetHoverColor(0.45f, 0.4f, 0.3f);
  slider->SetAlpha(0.3f);
  slider->SetOnMouseUp(GameOnMouseUp);

  // unit actions
  panel = main_panel->AddPanel(0, 18.0f, GLfloat(config.scr_height - 364), 170, 30);
  panel->SetPadding(0.0f);
  panel->SetColor(0, 0, 0);
  panel->SetAlpha(0.0f);
  panel->SetOnMouseUp(GameOnMouseUp);

  panel_info.stay_button = button = panel->AddGroupButton(MNU_ACTION_STAY, 3, 3, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 0), 2);
  button->SetTexture(GUI_BS_DOWN, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 6));
  SetGameButton("Stay (S)");

  panel_info.move_button = button = panel->AddGroupButton(MNU_ACTION_MOVE, 31, 3, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 1), 2);
  button->SetTexture(GUI_BS_DOWN, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 7));
  SetGameButton("Move (M)");

  panel_info.attack_button = button = panel->AddGroupButton(MNU_ACTION_ATTACK, 59, 3, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 2), 2);
  button->SetTexture(GUI_BS_DOWN, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 8));
  SetGameButton("Attack (A)");

  panel_info.mine_button = button = panel->AddGroupButton(MNU_ACTION_MINE, 87, 3, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 3), 2);
  button->SetTexture(GUI_BS_DOWN, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 9));
  SetGameButton("Mine (I)");

  panel_info.repair_button = button = panel->AddGroupButton(MNU_ACTION_REPAIR, 114, 3, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 4), 2);
  button->SetTexture(GUI_BS_DOWN, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 10));
  SetGameButton("Repair (R)");

  panel_info.build_button = button = panel->AddGroupButton(MNU_ACTION_BUILD, 142, 3, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 5), 2);
  button->SetTexture(GUI_BS_DOWN, gui_table.GetTexture(DAT_TGID_ACTION_BUTTONS, 11));
  SetGameButton("Build (B)");

  // radar panel
  radar_panel = panel = gui->AddPanel(0, GLfloat(config.scr_width - 185), GLfloat(config.scr_height - 185), DRW_RADAR_SIZE, DRW_RADAR_SIZE);
  panel->SetFaceColor(0, 0, 0);
  panel->SetPadding(0);
  panel->SetOnDraw(GameOnRadarDraw);
  panel->SetOnMouseUp(GameOnMouseUp);
  panel->SetOnMouseDown(GameOnRadarMouseDown);
  panel->SetOnMouseMove(GameOnRadarMouseMove);

  button = panel->AddCheckButton(MNU_TOGGLE_RADAR, 2, DRW_RADAR_SIZE - 17, 15, 15, "");
  button->SetColor(0.3f, 0.3f, 0.3f);
  button->SetOnMouseUp(GameOnMouseUp);
  button->SetOnMouseClick(GameButtonOnClick);
  button->SetTooltipText("Clip Radar");
  if (!radar.IsHideable())
    button->SetChecked(true);

  // toggle panel button
  button = gui->AddButton(MNU_TOGGLE_PANEL, GLfloat(config.scr_width - 15), GLfloat(config.scr_height - 15), 15, 15, "");
  SetGameButton("Toggle Panels (Tab)");

  // view segment button
  panel = main_panel->AddPanel(0, 18.0f, 10.0f, 170, 24);
  panel->SetPadding(0);
  panel->SetColor(0, 0, 0);
  panel->SetAlpha(0.0f);
  panel->SetOnMouseUp(GameOnMouseUp);

  panel_info.seg_button = button = panel->AddButton(MNU_VIEW_SEGMENT, 142, 0, gui_table.GetTexture(DAT_TGID_SEG_BUTTONS, 3));
  SetGameButton("Change View Segment (F5-F8)");

  // update panel
  selection->UpdateInfo(false);
  ChangeActionPanel(MNU_PANEL_EMPTY);

  // toggle main panel
  if (!main_panel_visible)
    ToggleMainPanel();
}


void ChangeActionPanel(int panel)
{
  if (panel_info.act_action_panel)
    panel_info.act_action_panel->Hide();

  panel_info.act_action_panel = panel_info.action_panel[panel];
  panel_info.act_action_panel->Show();
}


void CreateBuildButtons()
{
  if (state != ST_GAME) return;

  TGUI_BUTTON *button;
  int i;
  GLfloat y = panel_info.action_panel[MNU_PANEL_BUILD]->GetClientHeight() - 40.0f;
  GLfloat x = 0.0f;

  // uncheck button
  UncheckBuildButton();

  // worker's build buttons
  if (selection->GetFirstUnit()->TestItemType(IT_WORKER)) {
    TLIST<TBUILDING_ITEM>::TNODE<TBUILDING_ITEM> *node;
    TWORKER_ITEM *itm = static_cast<TWORKER_ITEM *>(selection->GetBuilderItem());

    if (last_item == itm)
      return;

    panel_info.action_panel[MNU_PANEL_BUILD]->Clear();

    for (i = 0, node = itm->build_list.GetFirst(); node; node = node->GetNext(), i++) {
      button = panel_info.action_panel[MNU_PANEL_BUILD]->AddGroupButton(
        reinterpret_cast<intptr_t>(node->GetPitem()), x, y, myself->race->tex_table.GetTexture(node->GetPitem()->tg_picture_id, 0), 3
      );
      SetBuildButton();
      
      if (i % 3 == 2) {
        y -= 42.0f;
        x = 0.0f;
      }
      else x += 52.0f;
    }

    last_item = itm;
  }

  // factory's produce buttons
  else if (selection->GetFirstUnit()->TestItemType(IT_FACTORY)) {
    TPRODUCEABLE_NODE *node;
    TFACTORY_ITEM *itm = static_cast<TFACTORY_ITEM *>(selection->GetBuilderItem());

    if (last_item == itm)
      return;

    panel_info.action_panel[MNU_PANEL_BUILD]->Clear();
  
    for (i = 0, node = itm->GetProductsList().GetFirstNode(); node; node = node->GetNextNode(), i++) {
      button = panel_info.action_panel[MNU_PANEL_BUILD]->AddButton(
        reinterpret_cast<intptr_t>(node->GetProduceableItem()), x, y, myself->race->tex_table.GetTexture(node->GetProduceableItem()->tg_picture_id, 0)
      );

      SetProduceButton();
    
      if (i % 3 == 2) {
        y -= 42.0f;
        x = 0.0f;
      }
      else x += 52.0f;
    }

    last_item = itm;
  }
}


void UncheckBuildButton()
{
  // uncheck button
  if (panel_info.act_build_button) {
    panel_info.act_build_button->SetChecked(false);
    panel_info.act_build_button = NULL;
  }
}


void UpdateGuardButtons()
{
  if (state != ST_GAME) return;

  panel_info.guard_button[AM_GUARDED]->SetEnabled(selection->GetCanAttack());
  panel_info.guard_button[AM_OFFENSIVE]->SetEnabled(selection->GetCanAttack() && selection->GetCanMove());
  panel_info.guard_button[AM_AGGRESSIVE]->SetEnabled(selection->GetCanAttack() && selection->GetCanMove());
}


void CheckGuardButton(TAGGRESSIVITY_MODE button)
{
  if (panel_info.act_guard_button)
    panel_info.act_guard_button->SetChecked(false);

  if (button != AM_NONE) {
    panel_info.act_guard_button = panel_info.guard_button[button];
    panel_info.act_guard_button->SetChecked(true);
  }
  else
    panel_info.act_guard_button = NULL;
}


void CreateOrderButtons()
{
  if (state != ST_GAME) return;
  if (!selection->GetFirstUnit()->TestItemType(IT_FACTORY)) return;

  panel_info.order_panel->Clear();

  TFACTORY_UNIT * funit = static_cast<TFACTORY_UNIT *>(selection->GetFirstUnit());
  TGUI_BUTTON *button;
  int i;
  char txt[4];

  for (i = 0; i < funit->GetOrderSize(); i++) {
    button = panel_info.order_panel->AddButton(
      reinterpret_cast<intptr_t>(funit->GetOrderedUnit(i)->GetProduceableItem()), i * 52.0f, 0.0f, myself->race->tex_table.GetTexture(funit->GetOrderedUnit(i)->GetProduceableItem()->tg_picture_id, 0)
    );

    sprintf(txt, "%d", i + 1);
    SetOrderButton(txt);

    if (i == 0)
      button->SetOnMouseClick(GameOrderOnClick);
    else
      button->SetAlpha(0.5f);
  }
}


void SetOrderVisibility(bool vis)
{
  int i;
  panel_info.order_panel->SetVisible(vis);

  if (config.scr_height > 480) {
    if (vis) {
      for (i = 0; i < MNU_PANELS_COUNT; i++) {
        panel_info.action_panel[i]->SetPosY(40 + panel_info.order_panel->GetHeight() + 5);
        panel_info.action_panel[i]->SetHeight(GLfloat(config.scr_height - 412 - panel_info.order_panel->GetHeight() - 5));
      }
    }
    else {
      for (i = 0; i < MNU_PANELS_COUNT; i++) {
        panel_info.action_panel[i]->SetPosY(40);
        panel_info.action_panel[i]->SetHeight(GLfloat(config.scr_height - 412));
      }
    }
  }
  else {
    if (vis) {
      for (i = 0; i < MNU_PANELS_COUNT; i++)
        panel_info.action_panel[i]->SetWidth(170 - panel_info.order_panel->GetWidth() - 5);
    }
    else {
      for (i = 0; i < MNU_PANELS_COUNT; i++)
        panel_info.action_panel[i]->SetWidth(170);
    }
  }
}


//========================================================================
// GLFW Size Callback
//========================================================================

/**
 *  GLFW window size callback function. This function is called by GLFW every
 *  time the window size is changed.
 *
 *  @param w New width of the window.
 *  @param h New height of the window.
 */
void GLFWCALL SizeCallback(int w, int h)
{
  config.scr_width = w;
  config.scr_height = h;

#if !HEADLESS
  /* Logical size w,h matches mouse/GUI; viewport must cover full GL drawable (HiDPI). */
  int fbw = w, fbh = h;
  glfwGetFramebufferSize(&fbw, &fbh);
  glViewport(0, 0, fbw, fbh);

  glfSetFontDisplayMode(font0, w, h);
#endif
}


//========================================================================
// GLFW Key Callback
//========================================================================

/**
 *  GLFW key callback function. This function is called every time a key is
 *  pressed or released.
 *
 *  @param key    A key identifier (uppercase ASCII or a special key
 *                identifier).
 *  @param action Either GLWF_PRESS or GLFW_RELEASE.
 */
void GLFWCALL KeyCallback(int key, int action)
{
  need_redraw->SetTrue ();

  switch (state) {
  case ST_MAIN_MENU:
  case ST_VIDEO_MENU:
  case ST_PLAY_MENU:
    if (action == GLFW_PRESS) MenuOnKeyDown(key);
    break;

  case ST_GAME:
    if (action == GLFW_PRESS) GameOnKeyDown(key);
    else GameOnKeyUp(key);
    break;

#if !HEADLESS
  case ST_EDITOR:
    if (action == GLFW_PRESS) EditorOnKeyDown(key);
    break;
#endif

  case ST_QUIT:
  default:
    break;
  }
}


//========================================================================
// GLFW Mouse Callbacks
//========================================================================

/**
 *  GLFW mouse button callback function. This function is called every time a
 *  mouse button is pressed or released.
 *
 *  @param button  A mouse button identifier (one of
 *                 GLFW_MOUSE_BUTTON_{LEFT|MIDDLE|RIGHT}).
 *  @param action  Either GLFW_PRESS or GLFW_RELEASE.
 */
void GLFWCALL MouseButtonCallback(int button, int action)
{
  need_redraw->SetTrue ();

  if (action == GLFW_PRESS) gui->MouseDown(GLfloat(mouse.x), GLfloat(mouse.y), button);
  else gui->MouseUp(GLfloat(mouse.x), GLfloat(mouse.y), button);
}

/**
 *  GLFW mouse position callback function. This function is called every time a
 *  mouse has changed position.
 *
 *  @param x  X coordinate.
 *  @param y  Y coordinate.
 */
void GLFWCALL MousePosCallback(int x, int y)
{
  need_redraw->SetTrue ();

  y = config.scr_height - y;

  int dx = x - mouse.rx;
  int dy = y - mouse.ry;

  if (map.mouse_moving && (state == ST_GAME
#if !HEADLESS
      || state == ST_EDITOR
#endif
      ))
  {
    map.Move(-dx * projection.game_h_coef, -dy * projection.game_v_coef);
  }
  else {
    int last_x = mouse.x;
    int last_y = mouse.y;

    mouse.x += dx;
    mouse.y += dy;
    if (mouse.x < 0) mouse.x = 0;
    if (mouse.y < 0) mouse.y = 0;
    if (mouse.x >= config.scr_width) mouse.x = config.scr_width - 1;
    if (mouse.y >= config.scr_height) mouse.y = config.scr_height - 1;

    if (map.drag_moving && (state == ST_GAME
#if !HEADLESS
        || state == ST_EDITOR
#endif
        )) {
      map.Move((mouse.x - last_x) * projection.game_h_coef, (mouse.y - last_y) * projection.game_v_coef);
    }
  }

  mouse.rx = x;
  mouse.ry = y;

  gui->MouseMove(GLfloat(mouse.x), GLfloat(mouse.y));
}


/**
 *  GLFW mouse wheel callback function. This function is called every time a
 *  mouse wheel changes position.
 *
 *  @param pos Actual wheel position.
 */
void GLFWCALL MouseWheelCallback(int pos)
{
  need_redraw->SetTrue ();

  static int last_pos;

  switch (state) {
  case ST_GAME:
    map.Zoom(pos - last_pos);
    break;
#if !HEADLESS
  case ST_EDITOR:
    map.Zoom(pos - last_pos);
    break;
#endif

  default: break;
  }

  last_pos = pos;
}

/**
 *  GLFW window refresh callback function. This function is called every time
 *  part of the window client area needs to be repainted - for instance when
 *  another window lying on top of our window has changed its position.
 */
void GLFWCALL WindowRefreshCallback () {
  need_redraw->SetTrue ();
}


//========================================================================
// Menu & Game
//========================================================================

/**
 *  Main function for menu. Contains menu loop.
 */
void Menu()
{
  TTIME clock;

//  Info("Running menu");
  CreateMenuGUI();

  if (error == ERR_LOAD_MAP) {
    action_key = 0;
    gui->ShowMessageBox ((string ("Error loading map '") + selected_map_name + "'").c_str(), GUI_MB_OK);
    error = ERR_NONE;
  }

  map_info_list.LoadMapList();

#if SOUND
  ChangeSoundVolume(config.snd_menu_sound_volume);
  if (config.snd_menu_music && !sounds_table.sounds[DAT_SID_MENU_MUSIC]->IsPlaying())
  {
    sounds_table.sounds[DAT_SID_MENU_MUSIC]->Play();
    sounds_table.sounds[DAT_SID_MENU_MUSIC]->Stop();   // hack for looping :((
    sounds_table.sounds[DAT_SID_MENU_MUSIC]->Play();
  }
#endif

  projection.SetProjection(PRO_MENU);
  mouse.ResetCursor();
  gui->MouseMove(GLfloat(mouse.x), GLfloat(mouse.y));  // update gui under mouse

  // menu loop
  while (state == ST_MAIN_MENU || state == ST_PLAY_MENU || state == ST_VIDEO_MENU) {
    // updates events
    clock.Update();
    mouse.Update(false, clock.GetShift());
    gui->Update(clock.GetShift());

    // Updates ost (On Screen Text). If there has been some change made, set
    // need_redraw to true.
    if (ost->Update(clock.GetActual()))
      need_redraw->SetTrue ();

    // Draw the screen, but only when there has been some change made.
    if (need_redraw->IsTrue ()) {
      gui->Draw();
      ost->Draw();
      mouse.Draw();

      glfwSwapBuffers ();
      gui->PollEvents();
    }

    AppSleepSeconds(0.01); // 100 fps
    glfwPollEvents ();

    gui->PollEvents();

#if SOUND
    FmodUpdate();
#endif
    if (!glfwGetWindowParam(GLFW_OPENED))
      state = ST_QUIT;
  }

  if (state == ST_RESET_VIDEO_MENU) state = ST_VIDEO_MENU;

  // delete gui
  ClearGuiVars();
  gui->Reset();
  
  // clear menu maps and players structures
  map_info_list.ClearMapList();
}


/**
 *  Update thread function. Started as an SDL thread from Game().
 *
 *  @param arg Unused (SDL thread entry convention).
 */
static int SDLCALL ProcessFunction(void *arg)
{
  TTIME time;
  TEVENT * act_event;
  TPLAYER_UNIT * act_unit;

  fps_of_update.Reset ();

  while (!allowed_to_start_process_function)
    AppSleepSeconds(0.02);

  Info ("Update: Running");

  while (started) {
    time.Update ();
    fps_of_update.Update (time.GetShift ());

    // cycle which get from queue all events with time_stamp <= actual time.
    while ((queue_events->GetFirstEventTimeStamp() != -1) && (queue_events->GetFirstEventTimeStamp() <= time.GetActual())) {
      process_mutex->Lock();

      act_event = queue_events->GetFirstEvent();

      act_unit = ((TPLAYER_UNIT *)players[act_event->GetPlayerID()]->hash_table_units.GetUnitPointer(act_event->GetUnitID()));

      // process event only in case that unit exists
      if (act_unit) {
      

        // local (not remote) units
        if (!player_array.IsRemote(act_event->GetPlayerID())) {
          
          // units running on my computer can have in queue only one event (events are NULLed only by US_... (not by requests))
          if ((act_event->GetEvent() < RQ_FIRST))
            act_unit->pevent = NULL;
          
          
          #if DEBUG_EVENTS
            Debug(LogMsg("PROC_L: P:%d U:%d E:%s RQ:%d X:%d Y:%d Z:%d R:%d I1:%d TS:%f RT:%f COUNT:%d", act_event->GetPlayerID(), act_event->GetUnitID(), EventToString(act_event->GetEvent()), act_event->GetRequestID(), act_event->simple1, act_event->simple2, act_event->simple3, act_event->simple4, act_event->int1, act_event->GetTimeStamp(), AppGetTimeSeconds(), queue_events->GetQueueLength()));
          #endif

          act_unit->ProcessEvent(act_event);
        }
        
        // units of not local players must be checked for right order of events according to time stamp
        if (player_array.IsRemote(act_event->GetPlayerID())) {
          // test if timestamp of las processed event is smaller than actual event time stamp
          if ((act_unit->last_event_time_stamp <= act_event->GetTimeStamp()) || (act_event->GetEvent() >= RQ_FIRST)){
            
            if (act_event->GetEvent() < RQ_FIRST) act_unit->last_event_time_stamp = act_event->GetTimeStamp();

            #if DEBUG_EVENTS
              if (act_unit->pevent)
                Error(LogMsg("Remote unit has PEVENT!"));

              Debug(LogMsg("PROC_R: P:%d U:%d E:%s RQ:%d X:%d Y:%d Z:%d R:%d I1:%d TS:%f RT:%f COUNT:%d", act_event->GetPlayerID(), act_event->GetUnitID(), EventToString(act_event->GetEvent()), act_event->GetRequestID(), act_event->simple1, act_event->simple2, act_event->simple3, act_event->simple4, act_event->int1, act_event->GetTimeStamp(), AppGetTimeSeconds(), queue_events->GetQueueLength()));
            #endif

            act_unit->ProcessEvent(act_event);
          }
        }
      }

      process_mutex->Unlock();
      
      pool_events->PutToPool(act_event);
    }

    {
      int pc = player_array.GetCount ();
      for (int ai = 1; ai < pc; ai++) {
        if (players[ai] && players[ai]->active && player_array.IsComputer (ai)
            && !player_array.IsRemote (ai))
          players[ai]->UpdateAI (time.GetShift ());
      }
    }

    // sleep that long, we get 50 fps
    time.SleepToGetExpectedFrameDuration (0.02);
  }
  return 0;
}


bool StartGame(double stime)
{
  state = ST_GAME;
  won_lose = false;

  process_thread = NULL;

#if !HEADLESS
  // create loading gui
  gui->SetFont(font0);
  gui->SetFontColor(1, 0.93f, 0.82f);

  load_panel = gui->AddPanel(0, 0, 0, (GLfloat)config.scr_width, (GLfloat)config.scr_height, gui_table.GetTexture(DAT_TGID_PANELS, 0));
  load_panel->AddLabel(0, GLfloat(config.scr_width/2) - 40, GLfloat(config.scr_height/2) - 7, "LOADING...");

  gui->Draw();
  glfwSwapBuffers();
#endif

  // create mutexes
  delete_mutex  = SDL_CreateMutex ();

  if (!delete_mutex) {
    Critical ("Could not create mutex");
    goto error;
  }

  // create instances of TPOOL for events and TQUEUE_EVENTS
  // create instance of TPOOL for path info 
  pool_path_info    = NEW TPOOL<TPATH_INFO>(EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);
  pool_sel_node     = NEW TPOOL<TSEL_NODE>(EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);

  pool_nearest_info = NEW TPOOL<TNEAREST_INFO>(EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);
  if (!pool_events)
    pool_events = NEW TPOOL<TEVENT>(2 * EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);

  // load map
  char map_name[MAP_MAX_NAME_LENGTH];
  char * last;
  
  strcpy (map_name, selected_map_name.c_str ());
  last = strrchr(map_name, '.');
  if (last)
    *last = 0;

  // set player options
  view_segment = DRW_ALL_SEGMENTS;

  if (!map.LoadMap(map_name)) {
    error = ERR_LOAD_MAP;
    state = ST_MAIN_MENU;
    goto error_with_own_state;
  }

  //create thread pool for path finding but only if doesn't exist yet
  if (threadpool_astar == NULL)
    threadpool_astar = threadpool_astar->CreateNewThreadPool(5, 50);
  //create thread pool for searching of the nearest building but only if doesn't exist yet
  if (threadpool_nearest == NULL)
    threadpool_nearest = threadpool_nearest->CreateNewThreadPool(3, 30);

  //check success
  if (!threadpool_astar || !threadpool_nearest) 
  {
    Critical ("Could not create thread pools");
    goto error;
  }

  selection = NEW TSELECTION;

#if !HEADLESS
  strcpy(myself->name, config.player_name);

  // moves map to player's initial view position
  map.CenterMapel(myself->initial_x, myself->initial_y);
#endif

  // reset events
  map.start_time = stime;

  started = true;

  // start Update thread
  process_thread = SDL_CreateThread (ProcessFunction, "game_update", NULL);

  if (process_thread == NULL) {
    Critical ("Could not create threads");
    goto error;
  }

#if !HEADLESS
  gui->Reset();
#endif
  
  // clear menu structures
  map_info_list.ClearRacList();
  return true;

error:
  state = ST_QUIT;
error_with_own_state:
 
  // clear menu structures
  map_info_list.ClearRacList();
#if !HEADLESS
  gui->Reset();
#endif

  if (process_thread) {
    SDL_WaitThread (process_thread, NULL);
    process_thread = NULL;
  }
  
  started = false;
  
  // delete selection
  if (selection){
    delete selection;
    selection = NULL;
  }

  // clear map
  map.DeleteMap();
  
  // clear pools
  if (pool_events){ delete pool_events; pool_events = NULL;}
  if (pool_path_info){ delete pool_path_info; pool_path_info = NULL;}
  if (pool_nearest_info){ delete pool_nearest_info; pool_nearest_info = NULL;}
  if (pool_sel_node){ delete pool_sel_node; pool_sel_node = NULL;}
  
  if (delete_mutex) {
    SDL_DestroyMutex(delete_mutex);
    delete_mutex = NULL;
  }

  return false;
}


#if !HEADLESS

void EditorMenuOnShow(TGUI_BOX *)
{
  map_info_list.LoadMapList();
  TMAP_BASIC_INFO_NODE *act;
  string maps;

  if (map_info_list.map_list) {
    for (act = map_info_list.map_list; act != NULL; act = act->next)
      maps += string(act->name) + "\n";
    if (maps.size())
      maps.erase(maps.end() - 1);
  } else
    maps = "No maps";

  editor_map_list->SetItems(maps.c_str());
  if (map_info_list.map_list && editor_map_list->GetItemsCount() > 0) {
    char *first_line = editor_map_list->GetItem(0);
    if (first_line)
      editor_map_list->SetItem(first_line);
  }
  if (map_info_list.map_list)
    EditorMapListOnChange(editor_map_list, 0);
}


void EditorMapListOnChange(TGUI_BOX *, int) {}


void EditorFragListOnChange(TGUI_BOX *, int item_index)
{
  if (item_index >= 0)
    editor_selected_fid = item_index;
}

void EditorFragButtonOnClick(TGUI_BOX *sender)
{
  intptr_t key = sender->GetKey();
  if (key >= MNU_EDITOR_FRAG_BASE && key < MNU_EDITOR_OBJ_BASE) {
    editor_selected_fid = (int)(key - MNU_EDITOR_FRAG_BASE);
    editor_tool = ET_TERRAIN;
  }
}

static void EditorSeparatorOnDraw(TGUI_BOX *sender)
{
  GLfloat w = sender->GetWidth();
  GLfloat h = sender->GetHeight();
  glDisable(GL_TEXTURE_2D);
  glColor4f(0.5f, 0.5f, 0.5f, 0.8f);
  glBegin(GL_LINES);
    glVertex2f(0, h * 0.5f);
    glVertex2f(w, h * 0.5f);
  glEnd();
  glEnable(GL_TEXTURE_2D);
}

void EditorTerrainThumbOnDraw(TGUI_BOX *sender)
{
  int fid = (int)(sender->GetKey() - MNU_EDITOR_FRAG_BASE);
  int sid = editor_paint_segment;
  if (fid < 0 || sid < 0 || sid >= DAT_SEGMENTS_COUNT) return;
  TGUI_TEXTURE *tex = scheme.terrf[sid][fid].GetFirstTexture();
  if (!tex || !tex->gl_id) return;

  GLfloat w = sender->GetWidth();
  GLfloat h = sender->GetHeight();
  float fvw = (float)tex->frame_width / tex->width;
  float fvh = (float)tex->frame_height / tex->height;

  glEnable(GL_TEXTURE_2D);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glBindTexture(GL_TEXTURE_2D, tex->gl_id);
  glBegin(GL_QUADS);
    glTexCoord2f(0, 0);          glVertex2f(0, 0);
    glTexCoord2f(fvw, 0);        glVertex2f(w, 0);
    glTexCoord2f(fvw, fvh);      glVertex2f(w, h);
    glTexCoord2f(0, fvh);        glVertex2f(0, h);
  glEnd();

  GLFfont *fnt = TGUI::self->GetFont();
  if (fnt) {
    char idtxt[16];
    snprintf(idtxt, sizeof(idtxt), "%d", fid);
    glColor3f(0.0f, 0.0f, 0.0f);
    glfDisable(GLF_RESET_PROJECTION);
    glfPrint(fnt, 3, h - fnt->fHeight - 1, idtxt, false);
    glColor3f(1.0f, 1.0f, 0.0f);
    glfPrint(fnt, 2, h - fnt->fHeight, idtxt, false);
    glfEnable(GLF_RESET_PROJECTION);
  }
}

void EditorObjButtonOnClick(TGUI_BOX *sender)
{
  intptr_t key = sender->GetKey();
  if (key >= MNU_EDITOR_OBJ_BASE && key < MNU_EDITOR_SOURCE_BASE) {
    editor_selected_oid = (int)(key - MNU_EDITOR_OBJ_BASE);
    editor_tool = ET_OBJECTS;
  }
}

void EditorSourceButtonOnClick(TGUI_BOX *sender)
{
  editor_selected_source_idx = (int)(sender->GetKey() - MNU_EDITOR_SOURCE_BASE);
  editor_tool = ET_SOURCE;
}

void EditorSchemeUnitButtonOnClick(TGUI_BOX *sender)
{
  editor_selected_scheme_uid = (int)(sender->GetKey() - MNU_EDITOR_SCHEME_UNIT_BASE);
  editor_tool = ET_SCHEME_UNIT;
}

void EditorSchemeBldButtonOnClick(TGUI_BOX *sender)
{
  editor_selected_scheme_bid = (int)(sender->GetKey() - MNU_EDITOR_SCHEME_BLD_BASE);
  editor_tool = ET_SCHEME_BUILDING;
}

void EditorStartPosButtonOnClick(TGUI_BOX *sender)
{
  editor_selected_start_point = (int)(sender->GetKey() - MNU_EDITOR_STARTPOS_BASE);
  editor_tool = ET_START_POS;
}

void EditorPlayerSelectOnClick(TGUI_BOX *sender)
{
  editor_selected_pid = (int)(sender->GetKey() - MNU_EDITOR_PLAYER_SELECT_BASE);
  editor_palette_level = EP_PLAYER_CATEGORY;
  EditorRebuildPalette();
}

void EditorPlayerBldButtonOnClick(TGUI_BOX *sender)
{
  editor_selected_player_bid = (int)(sender->GetKey() - MNU_EDITOR_PLAYER_BLD_BASE);
  editor_tool = ET_PLAYER_BUILDING;
}

void EditorPlayerUnitButtonOnClick(TGUI_BOX *sender)
{
  editor_selected_player_uid = (int)(sender->GetKey() - MNU_EDITOR_PLAYER_UNIT_BASE);
  editor_tool = ET_PLAYER_UNIT;
}

void EditorNavButtonOnClick(TGUI_BOX *sender)
{
  intptr_t key = sender->GetKey();
  switch (key) {
  case MNU_EDITOR_NAV_TERRAIN:
    editor_palette_level = EP_TERRAIN_TYPES;
    EditorRebuildPalette();
    break;
  case MNU_EDITOR_NAV_OBJECTS:
    editor_palette_level = EP_OBJECTS;
    EditorRebuildPalette();
    break;
  case MNU_EDITOR_NAV_PLAYER:
    editor_palette_level = EP_PLAYER_LIST;
    EditorRebuildPalette();
    break;
  case MNU_EDITOR_NAV_PLY_BLD:
    editor_palette_level = EP_PLAYER_BUILDINGS;
    EditorRebuildPalette();
    break;
  case MNU_EDITOR_NAV_PLY_UNI:
    editor_palette_level = EP_PLAYER_UNITS;
    EditorRebuildPalette();
    break;
  case MNU_EDITOR_NAV_BACK:
    if (editor_palette_level == EP_TERRAIN_VARIANTS)
      editor_palette_level = EP_TERRAIN_TYPES;
    else if (editor_palette_level == EP_PLAYER_UNITS || editor_palette_level == EP_PLAYER_BUILDINGS)
      editor_palette_level = EP_PLAYER_CATEGORY;
    else if (editor_palette_level == EP_PLAYER_CATEGORY)
      editor_palette_level = EP_PLAYER_LIST;
    else if (editor_palette_level == EP_PLAYER_LIST)
      editor_palette_level = EP_ROOT;
    else
      editor_palette_level = EP_ROOT;
    EditorRebuildPalette();
    break;
  default:
    if (key >= MNU_EDITOR_GROUP_BASE && key < MNU_EDITOR_GROUP_BASE + 100) {
      editor_palette_group = (int)(key - MNU_EDITOR_GROUP_BASE);
      editor_palette_level = EP_TERRAIN_VARIANTS;
      EditorRebuildPalette();
    }
    break;
  }
}

static std::string EditorExtractGroupPrefix(const char *name)
{
  if (!name || !*name) return "other";
  std::string s(name);
  if (s.size() > 3 && s.compare(0, 3, "ug_") == 0)
    s = s.substr(3);
  size_t pos = s.find('_');
  if (pos != std::string::npos)
    s = s.substr(0, pos);
  return s;
}

static void EditorReorderGroup(EditorFragGroup &g, const int *order, int order_len)
{
  std::set<int> id_set(g.frag_ids.begin(), g.frag_ids.end());
  std::vector<int> reordered;
  for (int i = 0; i < order_len; i++) {
    int fid = order[i];
    if (fid == -1) { reordered.push_back(-1); continue; }
    if (id_set.count(fid)) {
      reordered.push_back(fid);
      id_set.erase(fid);
    }
  }
  for (int fid : g.frag_ids)
    if (id_set.count(fid))
      reordered.push_back(fid);
  g.frag_ids = reordered;
}

void EditorBuildFragGroups(int sid)
{
  editor_frag_groups.clear();
  int n = scheme.terrf_count[sid];
  std::map<std::string, int> group_map;

  for (int i = 0; i < n; i++) {
    TTEX_GROUP *tg = scheme.terrf[sid][i].GetTexGroup();
    std::string prefix = tg ? EditorExtractGroupPrefix(tg->name) : "other";
    auto it = group_map.find(prefix);
    if (it == group_map.end()) {
      group_map[prefix] = (int)editor_frag_groups.size();
      EditorFragGroup g;
      g.name = prefix;
      g.representative_fid = i;
      g.frag_ids.push_back(i);
      editor_frag_groups.push_back(g);
    } else {
      editor_frag_groups[it->second].frag_ids.push_back(i);
    }
  }

  for (auto &g : editor_frag_groups) {
    if (g.name == "rocks") {
      static const int order[] = {
        1, 4, 5, 8, 7, 6, 3, 2, -1,
        24, 23, 78, 79, 26, 25, 77, 76, -1
      };
      EditorReorderGroup(g, order, (int)(sizeof(order) / sizeof(order[0])));
    }
  }
}

static void EditorGetThumbSize(TGUI_TEXTURE *tex, GLfloat *out_w, GLfloat *out_h)
{
  if (!tex || !out_w || !out_h) return;
  int fw = tex->frame_width;
  int fh = tex->frame_height;
  const int max_w = 85;
  float scale = 1.0f;
  if (fw > max_w)
    scale = (float)max_w / fw;
  int tw = (int)(fw * scale);
  int th = (int)(fh * scale);
  if (tw < 20) tw = 20;
  if (th < 14) th = 14;
  *out_w = (GLfloat)tw;
  *out_h = (GLfloat)th;
}

void EditorRebuildPalette(void)
{
  if (!editor_palette_sbox) return;
  editor_palette_sbox->Clear();

  if (editor_add_player_btn) {
    editor_add_player_btn->SetVisible(
        editor_palette_level == EP_PLAYER_LIST
        && player_array.GetCount() < 1 + EDITOR_MAX_NON_HYPER_PLAYERS);
  }

  int sid = editor_paint_segment;
  GLfloat scroll_h = editor_palette_sbox->GetHeight();
  GLfloat cx, cy;
  TGUI_BUTTON *button;

  switch (editor_palette_level) {
  case EP_ROOT: {
    if (editor_palette_title)
      editor_palette_title->SetCaption("Palette");
    cy = scroll_h - 40;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_TERRAIN, 10, cy, 170, 28, "Terrain");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.55f, 0.7f, 0.45f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);

    cy -= 36;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_OBJECTS, 10, cy, 170, 28, "Objects");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.55f, 0.55f, 0.75f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);

    cy -= 36;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_PLAYER, 10, cy, 170, 28, "Player");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.65f, 0.5f, 0.55f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    break;
  }
  case EP_TERRAIN_TYPES: {
    if (editor_palette_title)
      editor_palette_title->SetCaption("Terrain types");
    cy = scroll_h - 6;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_BACK, 4, cy - 22, 50, 20, "<< Back");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.6f, 0.6f, 0.6f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 30;

    for (int gi = 0; gi < (int)editor_frag_groups.size(); gi++) {
      EditorFragGroup &g = editor_frag_groups[gi];
      TGUI_TEXTURE *tex = scheme.terrf[sid][g.representative_fid].GetFirstTexture();
      if (tex) {
        GLfloat ttw, tth;
        EditorGetThumbSize(tex, &ttw, &tth);
        cy -= tth + 4;
        button = editor_palette_sbox->AddGroupButton(
          MNU_EDITOR_GROUP_BASE + gi, 4, cy, tex, 10);
        button->SetWidth(ttw);
        button->SetHeight(tth);
        button->SetOnMouseClick(EditorNavButtonOnClick);
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "%s (%d)", g.name.c_str(), (int)g.frag_ids.size());
        editor_palette_sbox->AddLabel(0, ttw + 10, cy + 4, lbl);
      } else {
        cy -= 24;
        button = editor_palette_sbox->AddButton(
          MNU_EDITOR_GROUP_BASE + gi, 4, cy, 170, 20, g.name.c_str());
        button->SetFontColor(0, 0, 0);
        button->SetOnMouseClick(EditorNavButtonOnClick);
      }
    }
    break;
  }
  case EP_TERRAIN_VARIANTS: {
    if (editor_palette_group < 0 || editor_palette_group >= (int)editor_frag_groups.size())
      break;
    EditorFragGroup &g = editor_frag_groups[editor_palette_group];
    char title[64];
    snprintf(title, sizeof(title), "Terrain: %s", g.name.c_str());
    if (editor_palette_title)
      editor_palette_title->SetCaption(title);

    cy = scroll_h - 6;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_BACK, 4, cy - 22, 50, 20, "<< Back");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.6f, 0.6f, 0.6f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 30;

    TGUI_TEXTURE *tex0 = NULL;
    if (!g.frag_ids.empty())
      tex0 = scheme.terrf[sid][g.frag_ids[0]].GetFirstTexture();
    GLfloat ttw0 = 44, tth0 = 28;
    if (tex0)
      EditorGetThumbSize(tex0, &ttw0, &tth0);
    int cols = (ttw0 * 2 + 6 <= 180) ? 2 : 1;
    cx = 2;
    int col = 0;

    for (int fi = 0; fi < (int)g.frag_ids.size(); fi++) {
      int fid = g.frag_ids[fi];
      if (fid == -1) {
        if (col % 2 == 1) { cx = 2; col++; }
        cy -= 4;
        TGUI_BUTTON *sep = editor_palette_sbox->AddButton(0, cx, cy, 170, 4, "");
        sep->SetAlpha(0.0f);
        sep->SetOnDraw(EditorSeparatorOnDraw);
        cy -= 4;
        continue;
      }
      if (fid < 0 || fid >= scheme.terrf_count[sid]) continue;
      TGUI_TEXTURE *tex = scheme.terrf[sid][fid].GetFirstTexture();
      if (!tex) continue;

      GLfloat ttw, tth;
      EditorGetThumbSize(tex, &ttw, &tth);
      cy -= tth + 2;
      button = editor_palette_sbox->AddGroupButton(
        MNU_EDITOR_FRAG_BASE + fid, cx, cy, ttw, tth, NULL, 1);
      button->SetAlpha(0.0f);
      button->SetOnDraw(EditorTerrainThumbOnDraw);
      button->SetOnMouseClick(EditorFragButtonOnClick);

      col++;
      if (cols == 2 && col % 2 == 1) {
        cy += tth + 2;
        cx = ttw + 6;
      } else {
        cx = 2;
      }
    }
    break;
  }
  case EP_OBJECTS: {
    if (editor_palette_title)
      editor_palette_title->SetCaption("Objects");
    cy = scroll_h - 6;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_BACK, 4, cy - 22, 50, 20, "<< Back");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.6f, 0.6f, 0.6f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 30;

    int obj_sid = sid;
    if (scheme.terro_count[obj_sid] <= 0) {
      obj_sid = -1;
      for (int t = 0; t < DAT_SEGMENTS_COUNT; t++) {
        if (scheme.terro_count[t] > 0) {
          obj_sid = t;
          break;
        }
      }
      if (obj_sid < 0)
        obj_sid = sid;
    }
    editor_object_segment = obj_sid;

    int no = scheme.terro_count[obj_sid];
    for (int oi = 0; oi < no; oi++) {
      TSURFACE_ITEM *si = &scheme.terro[obj_sid][oi];
      TGUI_TEXTURE *tex = scheme.tex_table.GetTexture(si->tg_stay_id, 0);
      if (tex) {
        GLfloat ttw, tth;
        EditorGetThumbSize(tex, &ttw, &tth);
        cy -= tth + 4;
        button = editor_palette_sbox->AddGroupButton(
          MNU_EDITOR_OBJ_BASE + oi, 4, cy, tex, 2);
        button->SetWidth(ttw);
        button->SetHeight(tth);
        button->SetOnMouseClick(EditorObjButtonOnClick);
        if (si->name)
          editor_palette_sbox->AddLabel(0, ttw + 10, cy + 4, si->name);
      } else {
        cy -= 24;
        button = editor_palette_sbox->AddButton(
          MNU_EDITOR_OBJ_BASE + oi, 4, cy, 170, 20, si->name ? si->name : "???");
        button->SetFontColor(0, 0, 0);
        button->SetOnMouseClick(EditorObjButtonOnClick);
      }
    }

    if (hyper_player && hyper_player->race) {
      TRACE *hr = hyper_player->race;
      cy -= 8;
      editor_palette_sbox->AddLabel(0, 4, cy - 12, "--- Sources ---");
      cy -= 20;
      for (int si = 0; si < hr->sources_count; si++) {
        TSOURCE_ITEM *src = hr->sources[si];
        if (!src)
          continue;
        TGUI_TEXTURE *tex = hr->tex_table.GetTexture(src->tg_picture_id, 0);
        if (tex) {
          GLfloat ttw, tth;
          EditorGetThumbSize(tex, &ttw, &tth);
          cy -= tth + 4;
          button = editor_palette_sbox->AddGroupButton(
            MNU_EDITOR_SOURCE_BASE + si, 4, cy, tex, 2);
          button->SetWidth(ttw);
          button->SetHeight(tth);
          button->SetOnMouseClick(EditorSourceButtonOnClick);
          if (src->name)
            editor_palette_sbox->AddLabel(0, ttw + 10, cy + 4, src->name);
        } else {
          cy -= 24;
          button = editor_palette_sbox->AddButton(
            MNU_EDITOR_SOURCE_BASE + si, 4, cy, 170, 20, src->name ? src->name : "src");
          button->SetFontColor(0, 0, 0);
          button->SetOnMouseClick(EditorSourceButtonOnClick);
        }
      }

      editor_palette_sbox->AddLabel(0, 4, cy - 12, "--- Units ---");
      cy -= 20;
      for (int ui = 0; ui < hr->units_count; ui++) {
        TFORCE_ITEM *fu = hr->units[ui];
        if (!fu)
          continue;
        TGUI_TEXTURE *tex = hr->tex_table.GetTexture(fu->tg_picture_id, 0);
        if (tex) {
          GLfloat ttw, tth;
          EditorGetThumbSize(tex, &ttw, &tth);
          cy -= tth + 4;
          button = editor_palette_sbox->AddGroupButton(
            MNU_EDITOR_SCHEME_UNIT_BASE + ui, 4, cy, tex, 2);
          button->SetWidth(ttw);
          button->SetHeight(tth);
          button->SetOnMouseClick(EditorSchemeUnitButtonOnClick);
          if (fu->name)
            editor_palette_sbox->AddLabel(0, ttw + 10, cy + 4, fu->name);
        } else {
          cy -= 24;
          button = editor_palette_sbox->AddButton(
            MNU_EDITOR_SCHEME_UNIT_BASE + ui, 4, cy, 170, 20, fu->name ? fu->name : "unit");
          button->SetFontColor(0, 0, 0);
          button->SetOnMouseClick(EditorSchemeUnitButtonOnClick);
        }
      }

      editor_palette_sbox->AddLabel(0, 4, cy - 12, "--- Buildings ---");
      cy -= 20;
      for (int bi = 0; bi < hr->buildings_count; bi++) {
        TBUILDING_ITEM *bld = hr->buildings[bi];
        if (!bld)
          continue;
        TGUI_TEXTURE *tex = hr->tex_table.GetTexture(bld->tg_picture_id, 0);
        if (tex) {
          GLfloat ttw, tth;
          EditorGetThumbSize(tex, &ttw, &tth);
          cy -= tth + 4;
          button = editor_palette_sbox->AddGroupButton(
            MNU_EDITOR_SCHEME_BLD_BASE + bi, 4, cy, tex, 2);
          button->SetWidth(ttw);
          button->SetHeight(tth);
          button->SetOnMouseClick(EditorSchemeBldButtonOnClick);
          if (bld->name)
            editor_palette_sbox->AddLabel(0, ttw + 10, cy + 4, bld->name);
        } else {
          cy -= 24;
          button = editor_palette_sbox->AddButton(
            MNU_EDITOR_SCHEME_BLD_BASE + bi, 4, cy, 170, 20, bld->name ? bld->name : "bld");
          button->SetFontColor(0, 0, 0);
          button->SetOnMouseClick(EditorSchemeBldButtonOnClick);
        }
      }
    }

    break;
  }
  case EP_PLAYER_LIST: {
    if (editor_palette_title)
      editor_palette_title->SetCaption("Player");
    cy = scroll_h - 6;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_BACK, 4, cy - 22, 50, 20, "<< Back");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.6f, 0.6f, 0.6f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 30;
    for (int pi = 1; pi < player_array.GetCount(); pi++) {
      char plab[48];
      snprintf(plab, sizeof(plab), "Player %d", pi);
      cy -= 28;
      button = editor_palette_sbox->AddButton(MNU_EDITOR_PLAYER_SELECT_BASE + pi, 4, cy, 170, 24, plab);
      button->SetFontColor(0, 0, 0);
      button->SetFaceColor(0.55f, 0.5f, 0.6f);
      button->SetOnMouseClick(EditorPlayerSelectOnClick);
    }
    break;
  }
  case EP_PLAYER_CATEGORY: {
    if (editor_palette_title)
      editor_palette_title->SetCaption("Player tools");
    cy = scroll_h - 6;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_BACK, 4, cy - 22, 50, 20, "<< Back");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.6f, 0.6f, 0.6f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 34;
    char cap[64];
    snprintf(cap, sizeof(cap), "P%d: pick category", editor_selected_pid);
    editor_palette_sbox->AddLabel(0, 4, cy, cap);
    cy -= 28;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_PLY_BLD, 4, cy, 170, 26, "Buildings");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.55f, 0.65f, 0.5f);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 32;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_PLY_UNI, 4, cy, 170, 26, "Units");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.55f, 0.55f, 0.65f);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    break;
  }
  case EP_PLAYER_BUILDINGS: {
    if (editor_palette_title)
      editor_palette_title->SetCaption("Player buildings");
    cy = scroll_h - 6;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_BACK, 4, cy - 22, 50, 20, "<< Back");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.6f, 0.6f, 0.6f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 30;
    if (editor_selected_pid >= 1 && editor_selected_pid < player_array.GetCount() &&
        players[editor_selected_pid] && players[editor_selected_pid]->race) {
      TRACE *pr = players[editor_selected_pid]->race;
      for (int bi = 0; bi < pr->buildings_count; bi++) {
        TBUILDING_ITEM *bld = pr->buildings[bi];
        if (!bld)
          continue;
        TGUI_TEXTURE *tex = pr->tex_table.GetTexture(bld->tg_picture_id, 0);
        if (tex) {
          GLfloat ttw, tth;
          EditorGetThumbSize(tex, &ttw, &tth);
          cy -= tth + 4;
          button = editor_palette_sbox->AddGroupButton(
            MNU_EDITOR_PLAYER_BLD_BASE + bi, 4, cy, tex, 2);
          button->SetWidth(ttw);
          button->SetHeight(tth);
          button->SetOnMouseClick(EditorPlayerBldButtonOnClick);
          if (bld->name)
            editor_palette_sbox->AddLabel(0, ttw + 10, cy + 4, bld->name);
        } else {
          cy -= 24;
          button = editor_palette_sbox->AddButton(
            MNU_EDITOR_PLAYER_BLD_BASE + bi, 4, cy, 170, 20, bld->name ? bld->name : "bld");
          button->SetFontColor(0, 0, 0);
          button->SetOnMouseClick(EditorPlayerBldButtonOnClick);
        }
      }
    }
    break;
  }
  case EP_PLAYER_UNITS: {
    if (editor_palette_title)
      editor_palette_title->SetCaption("Player units");
    cy = scroll_h - 6;
    button = editor_palette_sbox->AddButton(MNU_EDITOR_NAV_BACK, 4, cy - 22, 50, 20, "<< Back");
    button->SetFontColor(0, 0, 0);
    button->SetFaceColor(0.6f, 0.6f, 0.6f);
    button->SetHoverColor(1, 1, 1);
    button->SetOnMouseClick(EditorNavButtonOnClick);
    cy -= 30;
    if (editor_selected_pid >= 1 && editor_selected_pid < player_array.GetCount() &&
        players[editor_selected_pid] && players[editor_selected_pid]->race) {
      TRACE *pr = players[editor_selected_pid]->race;
      for (int ui = 0; ui < pr->units_count; ui++) {
        TFORCE_ITEM *fu = pr->units[ui];
        if (!fu)
          continue;
        TGUI_TEXTURE *tex = pr->tex_table.GetTexture(fu->tg_picture_id, 0);
        if (tex) {
          GLfloat ttw, tth;
          EditorGetThumbSize(tex, &ttw, &tth);
          cy -= tth + 4;
          button = editor_palette_sbox->AddGroupButton(
            MNU_EDITOR_PLAYER_UNIT_BASE + ui, 4, cy, tex, 2);
          button->SetWidth(ttw);
          button->SetHeight(tth);
          button->SetOnMouseClick(EditorPlayerUnitButtonOnClick);
          if (fu->name)
            editor_palette_sbox->AddLabel(0, ttw + 10, cy + 4, fu->name);
        } else {
          cy -= 24;
          button = editor_palette_sbox->AddButton(
            MNU_EDITOR_PLAYER_UNIT_BASE + ui, 4, cy, 170, 20, fu->name ? fu->name : "unit");
          button->SetFontColor(0, 0, 0);
          button->SetOnMouseClick(EditorPlayerUnitButtonOnClick);
        }
      }
    }
    break;
  }
  }
}


void EditorCaptureMapHead(const char *basename_no_ext)
{
  g_editor_saved_map_prologue.clear();
  if (!basename_no_ext || !*basename_no_ext)
    return;

  char path[4096];
  snprintf(path, sizeof(path), "%s%s%s", MAP_PATH, basename_no_ext, ".map");

  FILE *f = fopen(path, "rb");
  if (!f)
    return;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return;
  }
  long sz = ftell(f);
  if (sz <= 0 || sz > 32 * 1024 * 1024) {
    fclose(f);
    return;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return;
  }

  string buf;
  buf.resize((size_t)sz);
  if (fread(&buf[0], 1, (size_t)sz, f) != (size_t)sz) {
    fclose(f);
    return;
  }
  fclose(f);

  size_t pos_players = buf.find("<Players>");
  size_t pos_seg = buf.find("<Segment 0>");
  if (pos_players != string::npos)
    g_editor_saved_map_prologue = buf.substr(0, pos_players);
  else if (pos_seg != string::npos)
    g_editor_saved_map_prologue = buf.substr(0, pos_seg);
  else
    g_editor_saved_map_prologue = buf;
}


bool EditorBootstrap(const char *basename_raw)
{
  string base = EditorMapBaseId(basename_raw);
  if (base.empty())
    return false;

  if (!CreateGame())
    return false;

  player_array.Lock();
  string mapfile = base + ".map";
  if (!map_info_list.LoadMapInfo(false, mapfile.c_str())) {
    player_array.Unlock();
    action_force = true;
    Disconnect();
    return false;
  }
  player_array.SetRaceIdName(0, map_info_list.map_ext_info.scheme_id_name);
  {
    int max_p = map_info_list.map_ext_info.max_players;
    TMAP_RAC_INFO_NODE *r = map_info_list.rac_list;
    if (r) {
      player_array.SetRaceIdName(1, r->id_name);
      r = r->next;
    } else {
      player_array.SetRaceIdName(1, "human-red");
    }
    for (int i = 2; i <= max_p && r; i++, r = r->next) {
      player_array.AddComputerPlayer();
      if (host) host->AddEmptyAddress();
      player_array.SetRaceIdName(i, r->id_name);
    }
  }
  for (int i = 1; i < player_array.GetCount(); i++)
    player_array.SetStartPoint(i, i - 1);
  player_array.Unlock();

  EditorCaptureMapHead(base.c_str());

  delete_mutex = SDL_CreateMutex();
  if (!delete_mutex) {
    action_force = true;
    Disconnect();
    return false;
  }

  pool_path_info = NEW TPOOL<TPATH_INFO>(EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);
  pool_sel_node = NEW TPOOL<TSEL_NODE>(EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);
  pool_nearest_info = NEW TPOOL<TNEAREST_INFO>(EV_MIN_POOL_ELEMENTS, 0, EV_MIN_POOL_ELEMENTS);
  if (!pool_events)
    pool_events = NEW TPOOL<TEVENT>(2 * EV_MIN_POOL_ELEMENTS, 0, 2 * EV_MIN_POOL_ELEMENTS);

  char map_name[MAP_MAX_NAME_LENGTH];
  strncpy(map_name, base.c_str(), sizeof(map_name) - 1);
  map_name[sizeof(map_name) - 1] = 0;

  view_segment = DRW_ALL_SEGMENTS;

  if (!map.LoadMap(map_name)) {
    error = ERR_LOAD_MAP;
    if (pool_events) {
      delete pool_events;
      pool_events = NULL;
    }
    if (pool_path_info) {
      delete pool_path_info;
      pool_path_info = NULL;
    }
    if (pool_nearest_info) {
      delete pool_nearest_info;
      pool_nearest_info = NULL;
    }
    if (pool_sel_node) {
      delete pool_sel_node;
      pool_sel_node = NULL;
    }
    if (delete_mutex) {
      SDL_DestroyMutex(delete_mutex);
      delete_mutex = NULL;
    }
    action_force = true;
    Disconnect();
    return false;
  }

  if (threadpool_astar == NULL)
    threadpool_astar = threadpool_astar->CreateNewThreadPool(5, 50);
  if (threadpool_nearest == NULL)
    threadpool_nearest = threadpool_nearest->CreateNewThreadPool(3, 30);
  if (!threadpool_astar || !threadpool_nearest) {
    map.DeleteMap();
    if (pool_events) {
      delete pool_events;
      pool_events = NULL;
    }
    if (pool_path_info) {
      delete pool_path_info;
      pool_path_info = NULL;
    }
    if (pool_nearest_info) {
      delete pool_nearest_info;
      pool_nearest_info = NULL;
    }
    if (pool_sel_node) {
      delete pool_sel_node;
      pool_sel_node = NULL;
    }
    if (delete_mutex) {
      SDL_DestroyMutex(delete_mutex);
      delete_mutex = NULL;
    }
    action_force = true;
    Disconnect();
    return false;
  }

  selection = NEW TSELECTION;
  strcpy(myself->name, config.player_name);
  map.CenterMapel(map.width / 2, map.height / 2);
  map.start_time = 0;
  map_info_list.ClearRacList();
  started = false;
  return true;
}


void EditorShutdown(void)
{
  in_editor_mode = false;
  show_all = false;
  StopGame();
  action_force = true;
  Disconnect();
  editor_entry_basename.clear();
  g_editor_saved_map_prologue.clear();
  editor_frag_groups.clear();
  editor_palette_sbox = NULL;
  editor_right_panel = NULL;
  editor_palette_title = NULL;
  editor_add_player_btn = NULL;
  editor_object_segment = 1;
  editor_selected_pid = 1;
  editor_selected_source_idx = -1;
  editor_selected_scheme_uid = -1;
  editor_selected_scheme_bid = -1;
  editor_selected_player_bid = -1;
  editor_selected_player_uid = -1;
  editor_selected_start_point = -1;
}


void EditorOnMouseDown(TGUI_BOX *, GLfloat x, GLfloat, int button)
{
  if (x < GLfloat(config.scr_width - 200)) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT || button == GLFW_MOUSE_BUTTON_LEFT) {
      if (map.IsInMap(mouse.map_pos.x, mouse.map_pos.y)) {
        int mx = mouse.map_pos.x;
        int my = mouse.map_pos.y;
        if (editor_tool == ET_TERRAIN) {
          int gx = (mx / 5) * 5;
          int gy = (my / 5) * 5;
          map.EditorReplaceFragmentAt(editor_paint_segment, gx, gy, editor_selected_fid);
        } else if (editor_tool == ET_OBJECTS && editor_selected_oid >= 0) {
          map.EditorPlaceObject(editor_object_segment, mx, my, editor_selected_oid);
        } else if (editor_tool == ET_SOURCE && editor_selected_source_idx >= 0) {
          map.EditorPlaceSource(editor_selected_source_idx, mx, my);
        } else if (editor_tool == ET_SCHEME_BUILDING && editor_selected_scheme_bid >= 0) {
          map.EditorPlaceSchemeBuilding(editor_selected_scheme_bid, mx, my);
        } else if (editor_tool == ET_SCHEME_UNIT && editor_selected_scheme_uid >= 0) {
          map.EditorPlaceSchemeUnit(editor_selected_scheme_uid, mx, my);
        } else if (editor_tool == ET_PLAYER_BUILDING && editor_selected_player_bid >= 0) {
          map.EditorPlacePlayerBuilding(editor_selected_pid, editor_selected_player_bid, mx, my);
        } else if (editor_tool == ET_PLAYER_UNIT && editor_selected_player_uid >= 0) {
          map.EditorPlacePlayerUnit(editor_selected_pid, editor_selected_player_uid, mx, my);
        } else if (editor_tool == ET_START_POS && editor_selected_start_point >= 0) {
          map.EditorSetStartPosition(editor_selected_start_point, mx, my);
          editor_tool = ET_TERRAIN;
          editor_palette_level = EP_ROOT;
          EditorRebuildPalette();
        } else if (editor_tool == ET_ERASE) {
          map.EditorEraseAt(mx, my);
        } else {
          int sp = EditorFindNearestStartPoint(mx, my, 8);
          if (sp >= 0) {
            editor_selected_start_point = sp;
            editor_tool = ET_START_POS;
            if (editor_palette_title)
              editor_palette_title->SetCaption("Move start pos");
          }
        }
      }
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
      if (!radar.GetMoving())
        map.drag_moving = true;
    }
  }
}


void EditorOnMouseUp(TGUI_BOX *, GLfloat, GLfloat, int button)
{
  if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    map.drag_moving = false;
}


void EditorOnKeyDown(int key)
{
  if (gui->KeyDown(key))
    return;

  if (key == 'd' || key == 'D') {
    if (editor_tool == ET_ERASE) {
      editor_tool = ET_TERRAIN;
      editor_palette_level = EP_ROOT;
      EditorRebuildPalette();
    } else {
      editor_tool = ET_ERASE;
      if (editor_palette_title)
        editor_palette_title->SetCaption("Erase (D)");
    }
    return;
  }

  if (key == GLFW_KEY_ESC) {
    state = ST_MAIN_MENU;
  }
}


bool EditorGetPlacementPreview(int *out_x, int *out_y, int *out_w, int *out_h)
{
  int mx = mouse.map_pos.x;
  int my = mouse.map_pos.y;
  if (!map.IsInMap(mx, my))
    return false;

  switch (editor_tool) {
  case ET_TERRAIN:
  case ET_ERASE:
    *out_x = (mx / 5) * 5;
    *out_y = (my / 5) * 5;
    *out_w = 5;
    *out_h = 5;
    return true;

  case ET_OBJECTS:
    if (editor_selected_oid >= 0 &&
        editor_selected_oid < scheme.terro_count[editor_object_segment]) {
      TSURFACE_ITEM *si = &scheme.terro[editor_object_segment][editor_selected_oid];
      *out_w = si->GetWidth();
      *out_h = si->GetHeight();
      *out_x = mx;
      *out_y = my;
      return true;
    }
    return false;

  case ET_SOURCE:
    if (editor_selected_source_idx >= 0 && hyper_player && hyper_player->race &&
        editor_selected_source_idx < hyper_player->race->sources_count) {
      TSOURCE_ITEM *si = hyper_player->race->sources[editor_selected_source_idx];
      *out_w = si->GetWidth();
      *out_h = si->GetHeight();
      *out_x = mx;
      *out_y = my;
      return true;
    }
    return false;

  case ET_SCHEME_BUILDING:
    if (editor_selected_scheme_bid >= 0 && hyper_player && hyper_player->race &&
        editor_selected_scheme_bid < hyper_player->race->buildings_count) {
      TBUILDING_ITEM *bi = hyper_player->race->buildings[editor_selected_scheme_bid];
      *out_w = bi->GetWidth();
      *out_h = bi->GetHeight();
      *out_x = mx;
      *out_y = my;
      return true;
    }
    return false;

  case ET_SCHEME_UNIT:
    if (editor_selected_scheme_uid >= 0 && hyper_player && hyper_player->race &&
        editor_selected_scheme_uid < hyper_player->race->units_count) {
      TFORCE_ITEM *fi = hyper_player->race->units[editor_selected_scheme_uid];
      *out_w = fi->GetWidth();
      *out_h = fi->GetHeight();
      *out_x = mx;
      *out_y = my;
      return true;
    }
    return false;

  case ET_PLAYER_BUILDING:
    if (editor_selected_pid >= 1 && editor_selected_pid < player_array.GetCount() &&
        players[editor_selected_pid] && players[editor_selected_pid]->race &&
        editor_selected_player_bid >= 0 &&
        editor_selected_player_bid < players[editor_selected_pid]->race->buildings_count) {
      TBUILDING_ITEM *bi = players[editor_selected_pid]->race->buildings[editor_selected_player_bid];
      *out_w = bi->GetWidth();
      *out_h = bi->GetHeight();
      *out_x = mx;
      *out_y = my;
      return true;
    }
    return false;

  case ET_PLAYER_UNIT:
    if (editor_selected_pid >= 1 && editor_selected_pid < player_array.GetCount() &&
        players[editor_selected_pid] && players[editor_selected_pid]->race &&
        editor_selected_player_uid >= 0 &&
        editor_selected_player_uid < players[editor_selected_pid]->race->units_count) {
      TFORCE_ITEM *fi = players[editor_selected_pid]->race->units[editor_selected_player_uid];
      *out_w = fi->GetWidth();
      *out_h = fi->GetHeight();
      *out_x = mx;
      *out_y = my;
      return true;
    }
    return false;

  case ET_START_POS:
    *out_x = mx;
    *out_y = my;
    *out_w = 1;
    *out_h = 1;
    return true;
  }

  return false;
}


void CreateEditorGUI(void)
{
  TGUI_PANEL *panel;
  TGUI_BUTTON *button;

  gui->SetFont(font0);
  gui->SetSize((GLfloat)config.scr_width, (GLfloat)config.scr_height);
  gui->SetFontColor(1, 0.93f, 0.82f);
  gui->SetOnMouseDown(EditorOnMouseDown);
  gui->SetOnMouseUp(EditorOnMouseUp);
  gui->GetDefTooltip()->SetColor(0, 0, 0);
  gui->GetDefTooltip()->SetAlpha(TOOLTIP_ALPHA);

  panel = gui->AddPanel(0, 0, 0, GLfloat(config.scr_width - 200), 20);
  SetGamePanel(true);
  panel->AddLabel(0, 10, 2, "Map editor: LMB/RMB paint/place, MMB drag, D erase, Esc exit");

  editor_right_panel = panel = gui->AddPanel(0, GLfloat(config.scr_width - 200), 0,
                                             200, GLfloat(config.scr_height),
                                             gui_table.GetTexture(DAT_TGID_PANELS, 1));
  panel->SetAlpha(GAME_PANEL_ALPHA);
  panel->SetOnMouseUp(EditorOnMouseUp);

  editor_palette_title = panel->AddLabel(0, 10, GLfloat(config.scr_height - 36), "Palette");

  GLfloat scroll_top = GLfloat(config.scr_height - 56);
  GLfloat scroll_h = scroll_top - 70;
  editor_palette_sbox = panel->AddScrollBox(MNU_EDITOR_FRAG_LIST, 5, 70, 190, scroll_h);
  editor_palette_sbox->SetFaceColor(0.15f, 0.15f, 0.15f);
  editor_palette_sbox->SetAlpha(0.6f);

  button = panel->AddButton(MNU_EDITOR_SAVE, 10, 45, 80, 22, "Save");
  button->SetFontColor(0, 0, 0);
  button->SetFaceColor(0.7f, 0.6f, 0.4f);
  button->SetHoverColor(1, 0.93f, 0.82f);
  button->SetOnMouseClick(MenuButtonOnClick);

  button = panel->AddButton(MNU_EDITOR_EXIT, 100, 45, 80, 22, "Exit");
  button->SetFontColor(0, 0, 0);
  button->SetFaceColor(0.7f, 0.6f, 0.4f);
  button->SetHoverColor(1, 0.93f, 0.82f);
  button->SetOnMouseClick(MenuButtonOnClick);

  editor_add_player_btn = panel->AddButton(MNU_EDITOR_ADD_PLAYER, 10, 22, 80, 20, "+Player");
  editor_add_player_btn->SetFontColor(0, 0, 0);
  editor_add_player_btn->SetFaceColor(0.5f, 0.7f, 0.5f);
  editor_add_player_btn->SetHoverColor(0.7f, 1.0f, 0.7f);
  editor_add_player_btn->SetOnMouseClick(MenuButtonOnClick);
  editor_add_player_btn->SetVisible(false);

  EditorBuildFragGroups(editor_paint_segment);
  editor_palette_level = EP_ROOT;
  editor_tool = ET_TERRAIN;
  editor_selected_fid = 0;
  editor_selected_oid = -1;
  editor_selected_pid = 1;
  editor_selected_source_idx = -1;
  editor_selected_scheme_uid = -1;
  editor_selected_scheme_bid = -1;
  editor_selected_player_bid = -1;
  editor_selected_player_uid = -1;
  editor_selected_start_point = -1;
  EditorRebuildPalette();

  mouse.ResetCursor();
  gui->MouseMove(GLfloat(mouse.x), GLfloat(mouse.y));
}


void Editor(void)
{
  TTIME clock;
  int i;

  in_editor_mode = true;
  show_all = true;
  gui->Reset();

  if (!EditorBootstrap(editor_entry_basename.c_str())) {
    in_editor_mode = false;
    state = ST_MAIN_MENU;
    CreateMenuGUI();
    return;
  }

  CreateEditorGUI();

#if SOUND
  sounds_table.sounds[DAT_SID_MENU_MUSIC]->Stop();
#endif

  projection.SetProjection(PRO_GAME);
  fps.Reset();
  mouse.ResetCursor();
  gui->MouseMove(GLfloat(mouse.x), GLfloat(mouse.y));

  while (state == ST_EDITOR) {
    clock.Update();

    gui->Update(clock.GetShift());
    fps.Update(clock.GetShift());
    ost->Update(clock.GetActual());

    mouse.Update(true, clock.GetShift());
    selection->Update(clock.GetShift());
    map.UpdateMoving(clock.GetShift());
    projection.Update();
    map.UpdateActiveArea();

    int pl_count = player_array.GetCount();
    for (i = 0; i < pl_count; i++) {
      if (players[i]->active)
        players[i]->UpdateGraphics(clock.GetShift());
    }
    map.UpdateGraphics(clock.GetShift());
    scheme.UpdateGraphics(clock.GetShift());

    DrawGame();

    glfwSwapBuffers();
    gui->PollEvents();

    if (!glfwGetWindowParam(GLFW_OPENED))
      state = ST_QUIT;

    clock.SleepToGetExpectedFrameDuration(config.pr_expected_frame_duration);
    glfwPollEvents();
    gui->PollEvents();

#if SOUND
    FmodUpdate();
#endif
  }

  if (state == ST_QUIT && connected)
    Disconnect();

  EditorShutdown();

  ClearGuiVars();
  gui->Reset();
  panel_info.Clear();
  CreateMenuGUI();
}

#endif /* !HEADLESS */


void StopGame()
{
  started = false;

  if (process_thread) {
    SDL_WaitThread (process_thread, NULL);
    process_thread = NULL;
  }

  // delete selection
  if (selection) {
    delete selection;
    selection = NULL;
  }

  // delete map
  map.DeleteMap();
  
  // delete instances of pools
  if (pool_events){ delete pool_events; pool_events = NULL;}
  if (pool_path_info){ delete pool_path_info; pool_path_info = NULL;}
  if (pool_nearest_info){ delete pool_nearest_info; pool_nearest_info = NULL;}
  if (pool_sel_node){ delete pool_sel_node; pool_sel_node = NULL;}

  // kill all temporary threads
  if (threadpool_astar) { delete threadpool_astar; threadpool_astar = NULL; }
  if (threadpool_nearest) { delete threadpool_nearest; threadpool_nearest = NULL; }

  if (delete_mutex){
    SDL_DestroyMutex(delete_mutex);
    delete_mutex = NULL;
  }

  // queue is only cleared (it is destroyed in the end of program)
  queue_events->Clear();
}

/**
 *  Game function. Game loop is here.
 */
void Game(void)
{
  TTIME clock;
  int i;

  giant->Lock ();

  if (!started) {
    if (!StartGame(clock.GetActual())) {
      giant->Unlock ();
      return;
    }

    if (host->GetType () == THOST::ht_follower) {
      TFOLLOWER *follower = dynamic_cast<TFOLLOWER *>(host);

      follower->SendSynchronise ();
    } else {
      leader_ready = true;

      if (player_array.AllRemoteReady ()) {
        allowed_to_start_process_function = true;

        TLEADER *leader = dynamic_cast<TLEADER *>(host);
        leader->SendAllowProcessFunction ();
      }
    }
  }

  giant->Unlock ();

#if SOUND
  sounds_table.sounds[DAT_SID_MENU_MUSIC]->Stop();

  ChangeSoundVolume(config.snd_game_sound_volume);
  if (config.snd_game_music) {
    sounds_table.sounds[DAT_SID_GAME_MUSIC]->Play();
    sounds_table.sounds[DAT_SID_GAME_MUSIC]->Stop();   // hack for looping :((
    sounds_table.sounds[DAT_SID_GAME_MUSIC]->Play();
  }
#endif

  // create game GUI
  CreateGameGUI();

  fps.Reset();
  mouse.ResetCursor();
  gui->MouseMove(GLfloat(mouse.x), GLfloat(mouse.y));  // update gui under mouse
  myself->update_info = true;                         // update myself information on panels
  selection->UpdateInfo(true);

  if (!allowed_to_start_process_function) {
    action_key = MNU_PLAY2;
    gui->ShowMessageBox ("Waiting for other players...", GUI_MB_CANCEL);
  }

  // projection
  //projection.SetProjection(PRO_GAME);
  
  // main loop used for drawing
  while (state == ST_GAME) 
  {
    clock.Update();

    // gui environment
    gui->Update(clock.GetShift());

    // infos
    fps.Update(clock.GetShift());
    ost->Update(clock.GetActual());

    // mouse (MUST be called before selection update)
    mouse.Update(true, clock.GetShift());

    // selection
    selection->Update(clock.GetShift());

    // map position and active area
    map.UpdateMoving(clock.GetShift());

    // projection
    projection.Update();

    // active area
    map.UpdateActiveArea();

    if (!reduced_drawing) {
      // units and buildings (have to be called after updating active area)
      int pl_count = player_array.GetCount();
      for (i = 0; i < pl_count; i++) 
      {
        if (players[i]->active) players[i]->UpdateGraphics(clock.GetShift());
      }
      // map graphics (with sorting of units -> have to be called after updating units)
      map.UpdateGraphics(clock.GetShift());

      scheme.UpdateGraphics(clock.GetShift ());
    }

    // myself information
    if (myself->update_info) UpdateMyselfInfo();

    // draw game
    DrawGame();

    // change buffers
    glfwSwapBuffers();
    gui->PollEvents();
    
    if (!glfwGetWindowParam(GLFW_OPENED)) state = ST_QUIT;

    // sleep to get expected frame duration
    clock.SleepToGetExpectedFrameDuration (config.pr_expected_frame_duration);

    /* Process events to respon a bit quicker to some events (for example to
     * not to render one more frame, when QUIT key was pressed). */
    glfwPollEvents ();
    gui->PollEvents();

#if SOUND
    FmodUpdate();
#endif

  } // while (state == ST_GAME)

  if (state == ST_QUIT && connected) Disconnect();

  if (state == ST_RESET_VIDEO_MENU) state = ST_GAME;
  else {

#if SOUND
  sounds_table.sounds[DAT_SID_GAME_MUSIC]->Stop();
#endif

  }

  // delete gui
  ClearGuiVars();
  gui->Reset();
  panel_info.Clear();

  if (build_tooltip) {
    delete build_tooltip;
    build_tooltip = NULL;
  }
}


#if HEADLESS
/** Write s as a JSON string literal (quotes + escapes) to f. */
static void fprint_json_string(FILE *f, const std::string &s)
{
  fputc('"', f);
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    switch (c) {
      case '"':  fputs("\\\"", f); break;
      case '\\': fputs("\\\\", f); break;
      case '\b': fputs("\\b", f); break;
      case '\f': fputs("\\f", f); break;
      case '\n': fputs("\\n", f); break;
      case '\r': fputs("\\r", f); break;
      case '\t': fputs("\\t", f); break;
      default:
        if (c < 0x20u)
          fprintf(f, "\\u%04x", (unsigned)c);
        else
          fputc((int)c, f);
        break;
    }
  }
  fputc('"', f);
}

void RunDedicatedServer(const char *map_basename, int port)
{
  std::string map_base = map_basename ? map_basename : "";
  if (map_base.size() > 4 && map_base.compare(map_base.size() - 4, 4, ".map") == 0)
    map_base.resize(map_base.size() - 4);

  config.net_server_port = port;

  /* LoadMapInfo() joins MAP_PATH + file_name like opendir entries: must include ".map". */
  std::string map_file = map_base + ".map";
  if (!map_info_list.LoadMapInfo(false, map_file.c_str())) {
    Critical("Dedicated server: unknown or invalid map (use id_name without path, e.g. trial). "
             "Check MAP_PATH / --data and that maps/<id>.map exists.");
    return;
  }
  /* StartGame() strips trailing extension from selected_map_name before map.LoadMap(). */
  selected_map_name = map_file;
  Info(LogMsg("Dedicated server map=%s port=%d", map_base.c_str(), port));

  if (!CreateGame()) {
    Critical("Dedicated server: CreateGame failed");
    return;
  }

  /* Without GUI, hyper player never gets its race from MenuUpdateMapInfo. */
  player_array.Lock();
  player_array.SetRaceIdName(0, map_info_list.map_ext_info.scheme_id_name);
  player_array.Unlock();

  fprintf(stderr, "Dark Oberon dedicated server: map '%s' TCP %d\n", map_base.c_str(), port);
  fprintf(stderr, "Clients connect to this host:%d — then type: start\n", port);
  fprintf(stderr, "Commands: status | players | addcpu [easy|medium|hard] | start | quit | logs ...\n");

  bool running = true;
  while (running) {
#ifndef WINDOWS
    struct pollfd pfd;
    pfd.fd = fileno(stdin);
    pfd.events = POLLIN;
    int pr = poll(&pfd, 1, 100);
    if (pr > 0 && (pfd.revents & POLLIN)) {
      char buf[256];
      if (fgets(buf, sizeof buf, stdin) == NULL) {
        running = false;
        break;
      }
      if (strncmp(buf, "quit", 4) == 0)
        running = false;
      else if (strncmp(buf, "status", 6) == 0) {
        player_array.Lock();
        int n = player_array.GetCount();
        int active = 0;
        if (players) {
          for (int i = 0; i < n; i++) {
            if (players[i] && players[i]->active)
              active++;
          }
        }
        player_array.Unlock();
        fprintf(stderr, "slots=%d active=%d connected=%d started=%d\n",
                n, active, connected ? 1 : 0, started ? 1 : 0);
      } else if (strncmp(buf, "players", 7) == 0) {
        fputs("{\"players\":[", stderr);
        player_array.Lock();
        int n = player_array.GetCount();
        bool first_name = true;
        for (int i = 0; i < n; i++) {
          if (players && (!players[i] || !players[i]->active))
            continue;
          if (!first_name)
            fputc(',', stderr);
          first_name = false;
          fprint_json_string(stderr, player_array.GetPlayerName(i));
        }
        player_array.Unlock();
        fputs("],\"connected\":", stderr);
        fputs(connected ? "true" : "false", stderr);
        fputs(",\"started\":", stderr);
        fputs(started ? "true" : "false", stderr);
        fputs(",\"map\":", stderr);
        fprint_json_string(stderr, map_base);
        fprintf(stderr, ",\"port\":%d}\n", port);
      }
      else if (strncmp(buf, "addcpu", 6) == 0) {
        player_array.Lock();
        int max_p = map_info_list.map_ext_info.max_players;
        if (player_array.GetCount() >= max_p + 1) {
          Warning("addcpu: too many players for this map");
          player_array.Unlock();
          continue;
        }
        player_array.AddComputerPlayer();
        if (host)
          host->AddEmptyAddress();
        {
          int idx = player_array.GetCount() - 1;
          string chosen;
          for (TMAP_RAC_INFO_NODE *r = map_info_list.rac_list; r; r = r->next) {
            bool taken = false;
            for (int j = 0; j < player_array.GetCount(); j++) {
              if (player_array.GetRaceIdName(j) == string(r->id_name)) {
                taken = true;
                break;
              }
            }
            if (!taken) {
              chosen = r->id_name;
              break;
            }
          }
          if (!chosen.empty())
            player_array.SetRaceIdName(idx, chosen);

          const char *arg = buf + 6;
          while (*arg == ' ' || *arg == '\t')
            arg++;
          char lvname[16];
          int ln = 0;
          while (arg[ln] && arg[ln] != '\n' && arg[ln] != '\r' && arg[ln] != ' ' && ln < 15) {
            lvname[ln] = arg[ln];
            ln++;
          }
          lvname[ln] = 0;
          if (ln > 0) {
            bool ok = true;
            TAI_LEVEL_ID lv = TAI_LevelFromName(lvname, &ok);
            if (!ok)
              Warning(LogMsg("addcpu: unknown level '%s', using medium", lvname));
            player_array.SetAiLevel(idx, lv);
          }
        }
        player_array.Unlock();
        {
          const int lv = player_array.GetAiLevel(player_array.GetCount() - 1);
          fprintf(stderr, "addcpu: players=%d level=%s\n", player_array.GetCount(),
                  lv < 0 ? "default" : TAI_LevelName((TAI_LEVEL_ID)lv));
        }
      }
      else if (strncmp(buf, "logs", 4) == 0) {
        const char *p = buf + 4;
        while (*p == ' ' || *p == '\t')
          p++;
        if (*p == '\0' || *p == '\n' || *p == '\r') {
          TAI_LogListCpuPlayers(stderr);
        } else if (strncmp(p, "enable", 6) == 0
                   && (p[6] == '\0' || p[6] == ' ' || p[6] == '\t' || p[6] == '\n'
                       || p[6] == '\r')) {
          TAI_SetPhaseTransitionLogging(true);
          fputs("logs: phase transition logging on\n", stderr);
        } else if (p[0] == 'o' && p[1] == 'n'
                   && (p[2] == '\0' || p[2] == ' ' || p[2] == '\t' || p[2] == '\n'
                       || p[2] == '\r')) {
          TAI_SetPhaseTransitionLogging(true);
          fputs("logs: phase transition logging on\n", stderr);
        } else if (strncmp(p, "think", 5) == 0) {
          const char *q = p + 5;
          while (*q == ' ' || *q == '\t')
            q++;
          if (*q == '\0' || *q == '\n' || *q == '\r'
              || (q[0] == 'o' && q[1] == 'n'
                  && (q[2] == '\0' || q[2] == ' ' || q[2] == '\t' || q[2] == '\n' || q[2] == '\r'))) {
            TAI_SetThinkTraceLogging(true);
            fputs("logs: AI think trace on\n", stderr);
          } else if (q[0] == 'o' && q[1] == 'f' && q[2] == 'f'
                     && (q[3] == '\0' || q[3] == ' ' || q[3] == '\t' || q[3] == '\n'
                         || q[3] == '\r')) {
            TAI_SetThinkTraceLogging(false);
            fputs("logs: AI think trace off\n", stderr);
          } else {
            fputs("logs: usage — logs think | logs think on | logs think off | …\n", stderr);
          }
        } else if (strncmp(p, "off", 3) == 0
                   && (p[3] == '\0' || p[3] == ' ' || p[3] == '\t' || p[3] == '\n'
                       || p[3] == '\r')) {
          TAI_SetPhaseTransitionLogging(false);
          TAI_SetThinkTraceLogging(false);
          fputs("logs: phase + think trace off\n", stderr);
        } else {
          int slot = -1;
          if (std::sscanf(p, "%d", &slot) == 1 && slot >= 0)
            TAI_LogDumpPlayerAI(slot, stderr);
          else
            fputs("logs: usage — logs | logs on | logs off | logs think on | logs think off | logs <slot>\n",
                  stderr);
        }
      }
      else if (strncmp(buf, "start", 5) == 0) {
        player_array.Lock();
        int max_players = map_info_list.map_ext_info.max_players;
        if (player_array.GetCount() > max_players + 1) {
          Warning("start: too many players for this map");
          player_array.Unlock();
          continue;
        }
        if (!player_array.EveryPlayerHasDifferentRace()) {
          Warning("start: every player must have a different race");
          player_array.Unlock();
          continue;
        }
        player_array.Unlock();

        giant->Lock();
        if (host != NULL && host->GetType() == THOST::ht_leader) {
          TLEADER *leader = dynamic_cast<TLEADER *>(host);
          leader->SendPlayerArray(selected_map_name, true);
        }
        giant->Unlock();

        TTIME clock;
        if (!StartGame(clock.GetActual()))
          Warning("start: StartGame failed");
        else {
          leader_ready = true;
          giant->Lock();
          if (host != NULL && host->GetType() == THOST::ht_leader) {
            TLEADER *L = dynamic_cast<TLEADER *>(host);
            if (player_array.AllRemoteReady()) {
              allowed_to_start_process_function = true;
              L->SendAllowProcessFunction();
            }
          }
          giant->Unlock();
        }
      }
    }
#else
    (void)running;
    break;
#endif
    SDL_PumpEvents();
  }

  if (started)
    StopGame();
  Disconnect();
}
#endif /* HEADLESS */


//=========================================================================
// END
//=========================================================================
// vim:ts=2:sw=2:et:

