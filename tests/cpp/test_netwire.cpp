/*
 * Unit tests for the network wire format (protocol version 2): fixed-width
 * fields in network byte order (src/donet.h), the linearized game event
 * (TEVENT, src/doevents.cpp), the version trailer of connect/hello
 * (THOST, src/dohost.cpp), the in-game clock correction of a follower
 * (TCLOCK_FILTER, src/dofollower.cpp) and the outgoing queue and writer of a
 * connection (TNET_OUT_QUEUE, TNET_LISTENER, src/donet.cpp). Parsing must accept exactly the bytes
 * the sender produces and reject shorter or longer payloads.
 * Built with AddressSanitizer. Run: make test-ai
 */
#include "doevents.h"
#include "dofollower.h"
#include "dohost.h"
#include "doplayers.h"
#include "build_info.h"
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

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
  CHECK(m.GetConnection() == 7);
  m.Init_send(1, 0);   // a pooled message reused for sending: no stale reply connection
  CHECK(m.GetConnection() == no_connection);
  m.SetReplyConnection(5);
  CHECK(m.GetConnection() == 5);
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

TEST(test_clock_slews_by_small_steps) {
  TCLOCK_FILTER f;
  double clock = 0;                       // our correction so far; the leader is 10 ms ahead
  for (int i = 0; i < 4; i++) {
    double step = f.AddSample(0.001, 0.010 - clock);
    CHECK(std::fabs(step - TCLOCK_FILTER::max_step) < 1e-12);   // never a jump
    clock += step;
  }
  for (int i = 0; i < 4; i++)
    clock += f.AddSample(0.001, 0.010 - clock);
  CHECK(std::fabs(clock - 0.010) < 1e-9);   // converged, no overshoot
  CHECK(std::fabs(f.GetOffset()) < 1e-9);
  CHECK(std::fabs(f.AddSample(0.001, 0.010 - clock)) < 1e-9);
}

TEST(test_clock_trusts_smallest_round_trip) {
  TCLOCK_FILTER f;
  CHECK(std::fabs(f.AddSample(0.002, 0.0005) - 0.0005) < 1e-12);
  // later samples were queued (long round trip): their offsets are not trusted
  for (int i = 0; i < TCLOCK_FILTER::samples_count - 1; i++)
    CHECK(std::fabs(f.AddSample(0.200, -0.050)) < 1e-12);
  // the good sample has left the window: now the best of the slow ones counts
  CHECK(std::fabs(f.AddSample(0.150, -0.040) + TCLOCK_FILTER::max_step) < 1e-12);
}

TEST(test_out_queue_drops_when_full) {
  TNET_OUT_QUEUE q(100, 0.25);
  char data[40] = {0};
  CHECK(q.Put(data, 40, 1.0) == TNET_OUT_QUEUE::queued);
  CHECK(q.Put(data, 40, 1.0) == TNET_OUT_QUEUE::queued);
  CHECK(q.GetBytes() == 80);
  CHECK(q.Put(data, 40, 1.1) == TNET_OUT_QUEUE::dropped);   // over the byte limit
  CHECK(q.GetDropped() == 1);
  CHECK(q.Put(data, 20, 1.1) == TNET_OUT_QUEUE::queued);    // fits exactly
  CHECK(std::fabs(q.Pop(1.2) - 0.2) < 1e-9);                // how long it waited
  CHECK(q.GetBytes() == 60);
  CHECK(q.Put(data, 40, 1.2) == TNET_OUT_QUEUE::queued);    // room again
  std::string all;
  CHECK(q.Peek(all, 100) == 3 && all.size() == 100);        // 40 + 20 + 40 queued
  CHECK(q.Peek(all, 99) == 2 && all.size() == 60);          // whole messages only
  CHECK(q.Peek(all, 10) == 1 && all.size() == 40);          // but at least one
}

TEST(test_out_queue_stalls_on_age) {
  TNET_OUT_QUEUE q(1000, 0.25);
  char data[10] = {1, 2, 3};
  CHECK(q.GetAge(5.0) == 0);
  CHECK(q.Put(data, 10, 1.0) == TNET_OUT_QUEUE::queued);
  std::string head;
  CHECK(q.Peek(head, 1) == 1 && head == std::string(data, 10));   // at least one message
  CHECK(q.Put(data, 10, 1.25) == TNET_OUT_QUEUE::queued);   // at the limit
  // the front stays queued while the writer is blocked writing it: it keeps ageing
  CHECK(q.Put(data, 10, 1.3) == TNET_OUT_QUEUE::stalled);
  CHECK(q.GetBytes() == 20);                                // a stalled put queues nothing
  q.Pop(1.3);                                               // written at last
  CHECK(std::fabs(q.GetAge(1.3) - 0.05) < 1e-9);
  CHECK(q.Put(data, 10, 1.3) == TNET_OUT_QUEUE::queued);
}

static std::atomic<TNET_CONN_ID> g_disconnected(no_connection);
static void on_disconnect(TNET_CONN_ID conn) { g_disconnected = conn; }

static void wait_for(bool (*done)(), double seconds)
{
  for (int i = 0; i < seconds * 100 && !done(); i++)
    usleep(10000);
}

/** A connection whose peer does not read is evicted; the others are not held up. */
TEST(test_connection_writer_and_eviction) {
  if (!pool_net_messages)
    pool_net_messages = new TPOOL<TNET_MESSAGE>(32, 32, 256);
  g_disconnected = no_connection;

  int a[2], b[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, a) == 0);
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, b) == 0);

  TNET_LISTENER *l = new TNET_LISTENER(1000, 0);
  TNET_MESSAGE_HANDLER handler;   // the end of a connection reaches it through the dispatcher
  handler.RegisterOnDisconnect(on_disconnect);
  TNET_DISPATCHER *dispatcher = new TNET_DISPATCHER(l->GetMessageQueue(), &handler);
  l->ConsumerIsAttached(dispatcher->GetThread());
  in_addr addr;
  addr.s_addr = 0;
  TNET_CONN_ID slow = l->AddDialedConnection(addr, 1, a[0]);
  TNET_CONN_ID fast = l->AddDialedConnection(addr, 2, b[0]);
  CHECK(slow != fast && slow != no_connection && fast != no_connection);

  TNET_MESSAGE m;
  m.Init_send(1, 0);
  m.PackString("0123456789012345678901234567890123456789012345678");   // 54 B with the header

  // the writer delivers the exact bytes
  CHECK(l->Send(fast, &m));
  T_BYTE got[64];
  int n = 0;
  while (n < m.GetSize())
    n += read(b[1], got + n, m.GetSize() - n);
  CHECK(memcmp(got, m.GetHeaderStart(), m.GetSize()) == 0);

  // nobody reads a[1]: the socket buffer fills, then the queue ages and the connection is evicted
  bool evicted_sent = false;
  for (int i = 0; i < 20000 && g_disconnected == no_connection; i++) {
    if (!l->Send(slow, &m))
      evicted_sent = true;
    CHECK(l->Send(fast, &m) || i == 0);   // never held up by the slow one
    char sink[4096];
    while (recv(b[1], sink, sizeof sink, MSG_DONTWAIT) > 0) {}
    usleep(1000);
  }
  wait_for([]() { return g_disconnected != no_connection; }, 2);
  CHECK(evicted_sent);
  CHECK(g_disconnected == slow);
  CHECK(!l->IsConnectionOpen(slow));
  CHECK(!l->Send(slow, &m));          // a closed handle takes nothing
  CHECK(l->IsConnectionOpen(fast));
  CHECK(l->Send(fast, &m));
  CHECK(!l->Send(12345678, &m));      // an unknown handle neither

  // a shut down connection ends like a lost one
  g_disconnected = no_connection;
  l->Shutdown(fast);
  wait_for([]() { return g_disconnected != no_connection; }, 2);
  CHECK(g_disconnected == fast);

  delete l;
  dispatcher->ThreadJoined();
  delete dispatcher;
  close(a[1]);
  close(b[1]);
}

/** A forward is measured from when it was received; other messages are not forwards. */
TEST(test_out_queue_forward_time) {
  TNET_OUT_QUEUE q(1000, 0.25);
  char data[10] = {0};
  double fwd = 0;
  CHECK(q.Put(data, 10, 2.0) == TNET_OUT_QUEUE::queued);
  CHECK(q.Put(data, 10, 2.0, 1.999) == TNET_OUT_QUEUE::queued);
  q.Pop(2.001, &fwd);
  CHECK(fwd == -1);
  q.Pop(2.001, &fwd);
  CHECK(std::fabs(fwd - 0.002) < 1e-9);
}

// ---- relay decision (THOST::RouteRelay) ---------------------------------------------------
// Players: 0 hyper (leader), 1 human on the leader, 2 follower CA (conn 10), 3 follower CB
// (conn 11), 4 CPU (leader), 5 follower CC (conn 12).
static const TNET_CONN_ID CA = 10, CB = 11, CC = 12;
static std::vector<TNET_CONN_ID> relay_routes()
{
  TNET_CONN_ID r[] = { no_connection, no_connection, CA, CB, no_connection, CC };
  return std::vector<TNET_CONN_ID>(r, r + 6);
}
static TRELAY relay(T_BYTE type, int dest, int pid, bool rq, TNET_CONN_ID from,
                    std::vector<bool> departed = std::vector<bool>(6, false), bool in_game = true)
{
  TRELAY_MESSAGE m = { type, (T_BYTE)dest, pid, rq };
  return THOST::RouteRelay(m, from, relay_routes(), departed, in_game);
}
static bool dropped(const TRELAY &r, bool invalid)
{ return r.drop != NULL && !r.local && !r.forward && r.invalid == invalid; }

TEST(test_relay_broadcast) {
  TRELAY r = relay(net_protocol_event, all_players, 2, false, CA);   // CA's unit state
  CHECK(r.drop == NULL && r.local && r.forward && r.to == no_connection && r.origin == 2);
  r = relay(net_protocol_event, all_players, 2, true, CA);           // sync request of CA's unit
  CHECK(r.local && r.forward && r.to == no_connection);
  // spoofed: a state of another follower's, the leader's, a CPU's or the hyper player's unit
  CHECK(dropped(relay(net_protocol_event, all_players, 3, false, CA), true));
  CHECK(dropped(relay(net_protocol_event, all_players, 1, false, CA), true));
  CHECK(dropped(relay(net_protocol_event, all_players, 4, false, CA), true));
  CHECK(dropped(relay(net_protocol_event, all_players, 0, false, CA), true));
  CHECK(dropped(relay(net_protocol_event, all_players, 6, false, CA), true));   // no such player
  CHECK(dropped(relay(net_protocol_event, all_players, 2, false, CA, std::vector<bool>(6, false), false), true));
}

TEST(test_relay_requests) {
  // to a unit on the leader (a source of the hyper player, a CPU, the host): local only
  TRELAY r = relay(net_protocol_event, 0, 0, true, CA);
  CHECK(r.drop == NULL && r.local && !r.forward);
  r = relay(net_protocol_event, 4, 4, true, CA);
  CHECK(r.local && !r.forward);
  r = relay(net_protocol_event, 1, 1, true, CB);
  CHECK(r.local && !r.forward && r.origin == 3);
  // to another follower's unit: forwarded to that follower only, never dispatched here
  r = relay(net_protocol_event, 3, 3, true, CA);
  CHECK(r.drop == NULL && !r.local && r.forward && r.to == CB && r.origin == 2);
  r = relay(net_protocol_event, 2, 2, true, CC);
  CHECK(!r.local && r.forward && r.to == CA && r.origin == 5);
  // a state sent to one player, a dest that is not the unit's owner, a request to oneself
  CHECK(dropped(relay(net_protocol_event, 3, 3, false, CA), true));
  CHECK(dropped(relay(net_protocol_event, 5, 3, true, CA), true));
  CHECK(dropped(relay(net_protocol_event, 2, 2, true, CA), true));
}

TEST(test_relay_departed) {
  std::vector<bool> gone(6, false);
  gone[3] = true;   // CB left
  // CB's late messages and requests to his units are stale, not invalid
  CHECK(dropped(relay(net_protocol_event, all_players, 3, false, CB, gone), false));
  CHECK(dropped(relay(net_protocol_event, 3, 3, true, CA, gone), false));
  // the route of a closed connection is cleared: still not a local unit of the leader
  std::vector<TNET_CONN_ID> routes = relay_routes();
  routes[3] = no_connection;
  TRELAY_MESSAGE m = { net_protocol_event, 3, 3, true };
  CHECK(dropped(THOST::RouteRelay(m, CA, routes, gone, true), false));
  // CA is unaffected
  TRELAY r = relay(net_protocol_event, all_players, 2, false, CA, gone);
  CHECK(r.local && r.forward);
}

TEST(test_relay_chat_disconnect_other) {
  TRELAY r = relay(net_protocol_chat_message, 1, -1, false, CB);   // the claimed sender is ignored
  CHECK(r.drop == NULL && r.local && r.forward && r.to == no_connection && r.origin == 3);
  // a follower's own departure ("You LOST"): local only, the leader announces it
  r = relay(net_protocol_disconnect, all_players, 2, false, CA);
  CHECK(r.drop == NULL && r.local && !r.forward && r.departs);
  r = relay(net_protocol_disconnect, all_players, 2, false, CA, std::vector<bool>(6, false), false);
  CHECK(r.local && !r.forward && !r.departs);                     // lobby: the old handling
  // a forged departure of another player, the host or the hyper player
  CHECK(dropped(relay(net_protocol_disconnect, all_players, 3, false, CA), true));
  CHECK(dropped(relay(net_protocol_disconnect, all_players, 1, false, CA), true));
  CHECK(dropped(relay(net_protocol_disconnect, all_players, 0, false, CA), true));
  CHECK(dropped(relay(net_protocol_disconnect, all_players, 200, false, CA), true));
  // the rest is the local handlers' (time sync, ping, lobby); nothing is forwarded
  r = relay(net_protocol_time_sync, 0, -1, false, CA);
  CHECK(r.local && !r.forward);
  r = relay(net_protocol_synchronise, 0, -1, false, CA);
  CHECK(r.local && !r.forward);
  // a connection that is not a player's yet (connect, refused hosts): local, never forwarded
  r = relay(net_protocol_event, all_players, 2, false, 99);
  CHECK(r.local && !r.forward && r.origin == -1);
  r = relay(net_protocol_chat_message, all_players, -1, false, 99);
  CHECK(r.local && !r.forward);
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
