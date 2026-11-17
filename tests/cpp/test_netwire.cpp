/*
 * Unit tests for the network wire format (protocol version 2): fixed-width
 * fields in network byte order (src/donet.h), the linearized game event
 * (TEVENT, src/doevents.cpp) and the version trailer of connect/hello
 * (THOST, src/dohost.cpp). Parsing must accept exactly the bytes the sender
 * produces and reject shorter or longer payloads.
 * Built with AddressSanitizer. Run: make test-ai
 */
#include "doevents.h"
#include "dohost.h"
#include "doplayers.h"
#include "build_info.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

// dologs.o and doplayers.o are not linked: provide what the tested files refer to.
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
TPLAYER_ARRAY::TPLAYER_ARRAY() {}
TPLAYER_ARRAY::~TPLAYER_ARRAY() {}
TPLAYER_ARRAY player_array;
TPLAYER **players = NULL;

typedef void (*TestFn)();
static struct { const char *n; TestFn f; } g_tests[64];
static int g_nt = 0, g_fail = 0;
static void reg(const char *n, TestFn f) { g_tests[g_nt].n = n; g_tests[g_nt].f = f; g_nt++; }

#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)
#define TEST(name) static void name(); \
  static struct name##_reg { name##_reg() { reg(#name, name); } } name##_inst; static void name()

static const int event_size = 47;   // the same on 32- and 64-bit hosts

/** A message as the listener hands it over: header + @p payload. */
static void receive(TNET_MESSAGE &m, const T_BYTE *payload, int len, T_BYTE type = 1)
{
  T_BYTE buf[max_net_message_size];
  buf[0] = (T_BYTE)(TNET_MESSAGE::GetHeaderSize() + len);
  buf[1] = type;
  buf[2] = 0;
  buf[3] = 0;
  memcpy(buf + 4, payload, len);
  in_addr a;
  a.s_addr = 0;
  m.Init_receive(a, 0, 7, buf);
}

/** Received copy of a packed message (its payload as sent on the wire). */
static void loopback(TNET_MESSAGE &sent, TNET_MESSAGE &got)
{
  receive(got, reinterpret_cast<const T_BYTE *>(sent.GetHeaderStart()) + TNET_MESSAGE::GetHeaderSize(),
          sent.GetPayloadSize(), sent.GetType());
}

static void sample_event(TEVENT &e)
{
  e.Clear(true);
  e.SetEventProps(3, -123456, false, 1234.5678125, 17, 2, 99, 1, 2, 3, 4, 250, 255, -5, (intptr_t)0x7fffffff);
}

TEST(test_fields_are_big_endian) {
  T_BYTE b[8];
  NetPutU16(b, 0x1234);
  CHECK(b[0] == 0x12 && b[1] == 0x34 && NetGetU16(b) == 0x1234);
  NetPutU32(b, 0xA1B2C3D4u);
  CHECK(b[0] == 0xA1 && b[3] == 0xD4 && NetGetU32(b) == 0xA1B2C3D4u);
  NetPutU64(b, 0x0102030405060708ull);
  CHECK(b[0] == 1 && b[7] == 8 && NetGetU64(b) == 0x0102030405060708ull);
  NetPutDouble(b, 1.0);   // IEEE 754: 3FF0 0000 0000 0000
  CHECK(b[0] == 0x3F && b[1] == 0xF0 && b[7] == 0);
  CHECK(NetGetDouble(b) == 1.0);
  NetPutDouble(b, -0.1);
  CHECK(NetGetDouble(b) == -0.1);
}

TEST(test_event_round_trip) {
  TEVENT e, d;
  sample_event(e);
  char buf[128];
  CHECK(e.LinearizeEvent(buf) == event_size);
  d.Clear(true);
  CHECK(d.DelinearizeEvent(buf, event_size));
  CHECK(d.GetPlayerID() == 3);
  CHECK(d.GetUnitID() == -123456);
  CHECK(d.GetTimeStamp() == 1234.5678125);
  CHECK(d.GetEvent() == 17 && d.GetLastEvent() == 2 && d.GetRequestID() == e.GetRequestID());
  CHECK(d.simple1 == 1 && d.simple2 == 2 && d.simple3 == 3 && d.simple4 == 4 && d.simple5 == 250 && d.simple6 == 255);
  CHECK(d.int1 == -5 && d.int2 == 0x7fffffff);
  CHECK(d.IsFromNet() && !d.GetPriority());
}

TEST(test_event_wire_layout) {
  TEVENT e;
  sample_event(e);
  T_BYTE b[128];
  e.LinearizeEvent(reinterpret_cast<char *>(b));
  CHECK(b[0] == 3);                                   // player id u8
  CHECK(NetGetU32(b + 1) == (uint32_t)-123456);        // unit id i32
  CHECK(NetGetDouble(b + 5) == 1234.5678125);          // time stamp f64
  CHECK(NetGetU32(b + 13) == 17);                      // event i32
  CHECK(b[25] == 1 && b[30] == 255);                   // simple1..6 u8
  CHECK(NetGetU64(b + 31) == (uint64_t)-5);            // int1 i64
  CHECK(NetGetU64(b + 39) == 0x7fffffff);              // int2 i64
}

TEST(test_event_wrong_length_rejected) {
  TEVENT e, d;
  sample_event(e);
  char buf[128];
  e.LinearizeEvent(buf);
  d.Clear(true);
  CHECK(!d.DelinearizeEvent(buf, event_size - 1));
  CHECK(!d.DelinearizeEvent(buf, event_size + 1));
  CHECK(!d.DelinearizeEvent(buf, 0));
  CHECK(!d.DelinearizeEvent(buf, 50));   // version 1 size on 64-bit hosts
  CHECK(!d.DelinearizeEvent(buf, 42));   // version 1 size on 32-bit hosts
  CHECK(!d.IsFromNet());                 // left unchanged
}

TEST(test_event_int_out_of_range_rejected_on_32bit) {
  TEVENT e, d;
  sample_event(e);
  char buf[128];
  e.LinearizeEvent(buf);
  NetPutU64(reinterpret_cast<T_BYTE *>(buf) + 31, 0x100000000ull);
  d.Clear(true);
  CHECK(d.DelinearizeEvent(buf, event_size) == (sizeof(intptr_t) == 8));
}

TEST(test_message_fields_round_trip) {
  TNET_MESSAGE m, r;
  m.Init_send(9, 0);
  m.PackU16(40000);
  m.PackU32(0xDEADBEEFu);
  m.PackDouble(-2.5);
  m.PackString("abc");
  CHECK(m.GetPayloadSize() == 2 + 4 + 8 + 4);
  loopback(m, r);
  CHECK(r.ExtractU16() == 40000);
  CHECK(r.ExtractU32() == 0xDEADBEEFu);
  CHECK(r.ExtractDouble() == -2.5);
  CHECK(r.ExtractString() == "abc");
  CHECK(r.Remaining() == 0);
  r.ExtractEnd();
  CHECK(!r.ExtractFailed());
}

TEST(test_message_short_payload_rejected) {
  T_BYTE p[8] = {0};
  TNET_MESSAGE r;
  receive(r, p, 7);
  double d = r.ExtractDouble();
  CHECK(r.ExtractFailed());
  CHECK(d == 0.0);   // zero-filled
  receive(r, p, 1);
  r.ExtractU16();
  CHECK(r.ExtractFailed());
}

TEST(test_message_long_payload_rejected) {
  T_BYTE p[9] = {0};
  TNET_MESSAGE r;
  receive(r, p, 9);
  r.ExtractDouble();
  CHECK(r.Remaining() == 1);
  CHECK(!r.ExtractFailed());
  r.ExtractEnd();
  CHECK(r.ExtractFailed());
}

TEST(test_reply_connection) {
  TNET_MESSAGE m;
  T_BYTE none[1] = {0};
  receive(m, none, 0);
  CHECK(m.GetFileDescriptor() == 7);
  m.Init_send(1, 0);   // a pooled message reused for sending: no stale reply socket
  CHECK(m.GetFileDescriptor() == -1);
  m.SetReplyConnection(5);
  CHECK(m.GetFileDescriptor() == 5);
}

TEST(test_version_trailer) {
  TNET_MESSAGE m, r;
  m.Init_send(net_protocol_connect, 0);
  m.PackU16(1234);
  m.PackString("Player");
  THOST::PackVersion(&m);
  loopback(m, r);
  CHECK(r.ExtractU16() == 1234);
  CHECK(r.ExtractString() == "Player");
  std::string version;
  CHECK(THOST::ExtractVersion(&r, version) == NET_PROTOCOL_VERSION);
  CHECK(version == DO_VERSION_STRING);
  CHECK(!r.ExtractFailed());
}

TEST(test_version_trailer_missing_is_old_host) {
  // a connect of 0.6.0 .. 0.7.0-rc.2: host-order port and the name, nothing else
  const T_BYTE p[] = {0x40, 0x9C, 'O', 'l', 'd', 0};
  TNET_MESSAGE r;
  receive(r, p, sizeof p);
  r.ExtractU16();
  CHECK(r.ExtractString() == "Old");
  std::string version = "x";
  CHECK(THOST::ExtractVersion(&r, version) == 0);
  CHECK(version.empty());
  CHECK(!r.ExtractFailed());
}

TEST(test_version_trailer_malformed) {
  const T_BYTE truncated[] = {0, 2, 'v'};   // no terminator
  TNET_MESSAGE r;
  receive(r, truncated, sizeof truncated);
  std::string version;
  THOST::ExtractVersion(&r, version);
  CHECK(r.ExtractFailed());

  const T_BYTE extra[] = {0, 2, 'v', 0, 'x'};
  receive(r, extra, sizeof extra);
  THOST::ExtractVersion(&r, version);
  CHECK(r.ExtractFailed());

  const T_BYTE one[] = {2};
  receive(r, one, sizeof one);
  THOST::ExtractVersion(&r, version);
  CHECK(r.ExtractFailed());
}

int main()
{
  for (int i = 0; i < g_nt; i++) {
    int before = g_fail;
    g_tests[i].f();
    std::printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", g_tests[i].n);
  }
  std::printf("%d tests, %d failures\n", g_nt, g_fail);
  return g_fail ? 1 : 0;
}
