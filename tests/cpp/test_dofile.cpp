/*
 * Unit tests for the config-file parser (src/dofile.*): values longer than the
 * caller's buffer must be truncated, never copied past its end.
 * Built with AddressSanitizer so any overflow fails the run. Run: make test-ai
 */
#include "dofile.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

// dologs.o is not linked: provide the few logging globals dofile.cpp needs.
FILE *err_log = NULL;
FILE *full_log = NULL;
void (*log_callback)(int, const char *, const char *) = NULL;
SDL_mutex *log_mutex = NULL;
char *LogMsg(const char *msg, ...)
{
  static char buf[4096];
  va_list ap;
  va_start(ap, msg);
  vsnprintf(buf, sizeof(buf), msg, ap);
  va_end(ap);
  return buf;
}

typedef void (*TestFn)();
static struct { const char *n; TestFn f; } g_tests[64];
static int g_nt = 0, g_fail = 0;
static void reg(const char *n, TestFn f) { g_tests[g_nt].n = n; g_tests[g_nt].f = f; g_nt++; }

#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)
#define TEST(name) static void name(); \
  static struct name##_reg { name##_reg() { reg(#name, name); } } name##_inst; static void name()

static std::string g_dir;

static std::string write_file(const std::string &name, const std::string &content)
{
  std::string path = g_dir + "/" + name;
  FILE *f = fopen(path.c_str(), "wt");
  fputs(content.c_str(), f);
  fclose(f);
  return path;
}

TEST(test_short_value_is_read_unchanged) {
  std::string p = write_file("short.cfg", "player_name \"Titan\"\n");
  TCONF_FILE *cf = OpenConfFile(p.c_str());
  CHECK(cf != NULL);
  char name[20];
  CHECK(cf->ReadStr(name, (char *)"player_name", (char *)"x", true));
  CHECK(!strcmp(name, "Titan"));
  delete cf;
}

TEST(test_long_value_is_truncated_to_buffer) {
  std::string longname(300, 'A');
  std::string p = write_file("long.cfg", "player_name \"" + longname + "\"\n");
  TCONF_FILE *cf = OpenConfFile(p.c_str());
  CHECK(cf != NULL);
  char name[20];
  cf->ReadStr(name, (char *)"player_name", (char *)"x", true);
  CHECK(strlen(name) == sizeof(name) - 1);
  CHECK(!strncmp(name, longname.c_str(), sizeof(name) - 1));
  delete cf;
}

TEST(test_long_default_is_truncated_to_buffer) {
  std::string p = write_file("empty.cfg", "\n");
  TCONF_FILE *cf = OpenConfFile(p.c_str());
  CHECK(cf != NULL);
  std::string def(100, 'D');
  char name[20];
  cf->ReadStr(name, (char *)"missing", (char *)def.c_str(), false);
  CHECK(strlen(name) == sizeof(name) - 1);
  delete cf;
}

TEST(test_long_multiline_item_name_does_not_overflow) {
  // A multi-line item ("_" continuation) is accumulated in a 10 KiB buffer
  // before its first word (the item name) is split off into a line buffer.
  std::string content;
  for (int i = 0; i < 4; i++) content += std::string(900, 'n') + "_\n";
  content += "end\n";
  std::string p = write_file("multiline.cfg", content);
  TCONF_FILE *cf = OpenConfFile(p.c_str());
  CHECK(cf != NULL);
  delete cf;
}

TEST(test_path_longer_than_128_chars_opens) {
  std::string sub = g_dir + "/" + std::string(150, 'd');
  CHECK(mkdir(sub.c_str(), 0700) == 0);
  std::string p = sub + "/deep.cfg";
  FILE *f = fopen(p.c_str(), "wt");
  fputs("name \"Deep\"\n", f);
  fclose(f);
  TCONF_FILE *cf = OpenConfFile(p.c_str());
  CHECK(cf != NULL);
  char name[20] = "";
  CHECK(cf && cf->ReadStr(name, (char *)"name", (char *)"x", true));
  CHECK(!strcmp(name, "Deep"));
  delete cf;
}

int main()
{
  char tmpl[] = "/tmp/dofile-test-XXXXXX";
  g_dir = mkdtemp(tmpl);
  for (int i = 0; i < g_nt; i++) {
    std::printf("%s\n", g_tests[i].n);
    g_tests[i].f();
  }
  std::printf("%d tests, %d failures\n", g_nt, g_fail);
  std::string cmd = "rm -rf '" + g_dir + "'";
  if (system(cmd.c_str())) {}
  return g_fail ? 1 : 0;
}
