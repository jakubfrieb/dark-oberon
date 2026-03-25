/*
 * Dark Oberon — headless dedicated server (Leader host, no window).
 * Build: make -C src server
 */

#include "cfg.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "doalloc.h"
#include "doconfig.h"
#include "doengine.h"
#include "doevents.h"
#include "doipc.h"
#include "dologs.h"
#include "domap.h"
#include "donet.h"
#include "doplayers.h"
#include "dosimpletypes.h"

#include <SDL2/SDL.h>

#if defined(UNIX) || defined(__unix__)
#include <unistd.h>
#include <pwd.h>
#endif

static std::string default_user_dir()
{
#if defined(UNIX) || defined(__unix__)
  const char *h = getenv("HOME");
  if (h)
    return std::string(h) + "/.dark-oberon/";
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_dir)
    return std::string(pw->pw_dir) + "/.dark-oberon/";
#endif
  return "./.dark-oberon/";
}

static void usage(const char *argv0)
{
  fprintf(stderr,
          "Usage: %s --map <id> [--port N] [--data <dir>]\n"
          "  id   map basename, e.g. trial or trial.map (not a full path)\n"
          "  --data  directory that contains maps/, races/, schemes/, dat/ (default: dir of binary, else .)\n"
          "  default port %d\n",
          argv0, CFG_DEF_NET_SERVER_PORT);
}

/** Strip trailing .map so argv can be sunnybay.map or sunnybay. */
static void normalize_map_id(std::string &s)
{
  static const char suf[] = ".map";
  const size_t sl = sizeof(suf) - 1;
  if (s.size() > sl && s.compare(s.size() - sl, sl, suf) == 0)
    s.erase(s.size() - sl);
}

int main(int argc, char **argv)
{
  std::string map_storage;
  const char *map_arg = NULL;
  int port_override = -1;
  std::string data_dir = "./";

  if (argc > 0 && argv[0]) {
    std::string p(argv[0]);
    size_t s = p.find_last_of("/\\");
    if (s != std::string::npos)
      data_dir = p.substr(0, s + 1);
  }

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--map") && i + 1 < argc) {
      map_storage = argv[++i];
      normalize_map_id(map_storage);
      map_arg = map_storage.c_str();
    }
    else if (!strcmp(argv[i], "--port") && i + 1 < argc)
      port_override = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--data") && i + 1 < argc)
      data_dir = argv[++i];
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      usage(argc > 0 ? argv[0] : "dark-oberon-server");
      return 0;
    }
  }

  /* MAP_PATH is app_path + DATA_DIR "maps/"; app_path must end with '/' or paths become e.g. /appmaps/. */
  if (!data_dir.empty() && data_dir.back() != '/' && data_dir.back() != '\\')
    data_dir += '/';

  if (!map_arg || !*map_arg) {
    usage(argc > 0 ? argv[0] : "dark-oberon-server");
    return 1;
  }

  srand((unsigned)time(NULL));

  if (!OpenLogFiles()) {
    fprintf(stderr, "OpenLogFiles failed\n");
    return 1;
  }

  if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
    Critical(LogMsg("SDL_Init: %s", SDL_GetError()));
    return 1;
  }

  app_path = data_dir;
  user_dir = default_user_dir();

  init_sockets();
  pool_net_messages = NEW TPOOL<TNET_MESSAGE>(32, 32, 256);

  if (!LoadConfig()) {
    CloseLogFiles();
    return 1;
  }

  int port = (port_override > 0) ? port_override : config.net_server_port;

  strncpy(config.player_name, "Host", sizeof config.player_name);
  config.player_name[sizeof config.player_name - 1] = '\0';

  player_array.Initialise();

  init_giant();
  process_mutex = NEW TRECURSIVE_LOCK();
  CreateLogMutex();

  map.InitPools();

  queue_events = NEW TQUEUE_EVENTS;
  if (!queue_events) {
    Critical("queue_events");
    return 1;
  }
  pool_events = NEW TPOOL<TEVENT>(2 * EV_MIN_POOL_ELEMENTS, 0, 2 * EV_MIN_POOL_ELEMENTS);
  if (!pool_events) {
    Critical("pool_events");
    return 1;
  }

  RunDedicatedServer(map_arg, port);

  delete pool_events;
  pool_events = NULL;
  delete queue_events;
  queue_events = NULL;

  delete process_mutex;
  process_mutex = NULL;

  SaveConfig();

  end_sockets();
  SDL_Quit();
  CloseLogFiles();
  return 0;
}
