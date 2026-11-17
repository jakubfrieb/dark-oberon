# Network latency baselines B0 / B1 (rc.3 step 1)

| Name | Build |
|---|---|
| B0 | P2P as in rc.2: this branch built **without** `TCP_NODELAY` (`NET_NO_NODELAY=1`) |
| B1 | P2P + `TCP_NODELAY` (this branch) |

B0 and B1 are built from the same commit and differ only in the two `EnableNoDelay` calls.
B2 (relay) is to be measured later with the same harness and must be compared against B1
(`docs/NETWORK_RELAY_PLAN.md`, section 7).

## Builds

- Commit: `976d284` (metrics). The harness is the `tests/net` of this branch.
- Binaries: `do-wt/baselines/{B0,B1}/` (`dark-oberon`, `dark-oberon-server`, `BUILD_INFO.txt`).
- Raw results: `do-wt/baselines/results/{B0,B1,run2/*,run3/*}/{a,b,c}/` (`summary.txt` and the logs).
- `strace -e trace=setsockopt` on the server's accept path (run on the B0/B1 builds of the previous
  commit `bbd389d`, which have the same socket code):
  - B1: `setsockopt(fd, SOL_TCP, TCP_NODELAY, [1], 4) = 0`;
  - B0: no TCP_NODELAY call;
  - both: one SO_KEEPALIVE.

| Build | `dark-oberon` sha256 | `dark-oberon-server` sha256 |
|---|---|---|
| B0 | `b0c7bf4b…dd601` | `207bfc8e…03fc9` |
| B1 | `4df4b279…ca0e` | `ce3202d2…d7454` |

Full hashes: `BUILD_INFO.txt` next to the binaries.

## Commands

```sh
B=../baselines                                  # do-wt/baselines
tests/net/build.sh $B/B1
NET_NO_NODELAY=1 tests/net/build.sh $B/B0
tests/net/baseline.sh $B/B0 $B/results/B0       # scenarios a, b, c
tests/net/baseline.sh $B/B1 $B/results/B1
# repeats of a and b (run2, run3): NET_PORT=1710x tests/net/baseline.sh $B/Bx $B/results/runN/Bx "a b"
```

**Scenarios** (all on one Linux machine, in an unprivileged user + network namespace):
- **a:** localhost, no delay.
- **b:** netem `delay 20ms 2ms` on loopback, so 20 ± 2 ms one way in both directions (RTT about 40 ms).
- **c:** localhost, 2 hard CPU players on the server and `spawn 600 fight` (608 units). Used for the
  server step time and the dispatch time under load.

**Common setup:**
- Map `crossroads`, headless server, two GUI clients, no CPU players in a and b.
- The clients keep sending their 6 starting units between two points (`run.sh`).
- 15 s warm-up (30 s in c), then 60 s measured. `perf` is reset after the warm-up.
- `netstat` counts from 5 s after the game started.

## Results

**Perceived** is how late a remote unit's state is applied on the receiving client. In a and b it
covers the events of the **other client** only, so it is the peer-to-peer path that B2 will
route through the server.

Each value is the median and [range] over 3 runs × 2 clients, in ms.

| Scenario | Build | perceived p50 | perceived p95 | perceived p99 | perceived max | transit p95* | transit p99* | dispatch p99 / max |
|---|---|---|---|---|---|---|---|---|
| a localhost | B0 | 23.4 [16.2–24.6] | 58.4 [45.5–62.9] | 72.5 [60.2–92.3] | 78.8 [77.4–134.8] | 42.1 [32.7–47.2] | 55.5 [52.1–83.2] | 0.15 / 0.77 |
| a localhost | **B1** | 18.3 [13.9–21.4] | **32.3** [28.2–35.4] | **34.5** [31.6–38.8] | 37.2 [32.0–40.7] | **19.9** [19.8–20.0] | **20.1** [20.1–20.2] | 0.10 / 0.74 |
| b 20±2 ms | B0 | 58.3 [53.2–59.8] | 107.4 [100.2–110.0] | 126.3 [121.8–128.8] | 143.0 [139.7–151.8] | 96.5 [83.4–98.4] | 115.0 [112.7–117.7] | 0.20 / 0.75 |
| b 20±2 ms | **B1** | 36.2 [32.6–42.5] | **53.2** [47.0–56.9] | **56.6** [54.8–61.2] | 60.9 [58.3–62.7] | **40.1** [39.8–40.5] | **41.5** [41.3–41.6] | 0.15 / 0.80 |

\* `transit` is corrected by each run's client↔client clock error. For c1:
`transit + (c2-c1)`; for c2: `transit − (c2-c1)`, with `c2-c1` taken from `[clock]`.

Samples per client and run: 1000–2400. One B1-a client had 323.

**Scenario c** (one run each, under load):

| Build | server `perf` step p99 / max | server dispatch p99 / max | client dispatch p99 / max | client perceived p50 / p95 / p99 / max |
|---|---|---|---|---|
| B0 | 2.92 / 17.64 | 0.35 / 0.65 | 0.15 / 1.26 | 22 / 42 / 55 / 81 |
| B1 | 2.84 / 10.11 | 0.15 / 2.08 | 0.05 / 2.82 | 16 / 30 / 34 / 40 |

- In c, each client receives about 100 000 events per minute: the states of the spawned units.
- Most of those units exist only on the server, so the clients drop them. That is why `perceived`
  has only about 13 000 samples per client in c (the events that were applied).
- The step time is about 60 events per step in both builds.

## Reading the numbers

- **Nagle is the dominant extra delay in B0, even on localhost.** Linux loopback has a delayed ACK
  of at least 40 ms. Nagle holds the next small write until the previous one is acknowledged, so
  B0 shows tails of 60-130 ms where B1 has 32-40 ms.
- With 20 ms one way, NODELAY roughly halves:
  - p95: 107 → 53 ms;
  - p99: 126 → 57 ms.
- **The floor in B1 is the simulation tick, not the network.**
  - A state's time stamp is the 20 ms step it was due in. `transit` therefore includes 0-20 ms on the
    sender, and `perceived` adds 0-20 ms more until the receiver's next step.
  - On localhost, B1 transit p95 ≈ 20 ms and perceived p95 ≈ 32 ms, with almost no network time.
  - At 20 ms one way: transit p95 ≈ 40 ms (20 + tick + jitter), and perceived p95 ≈ 53 ms.
- **Dispatch** (waiting for `giant` plus queue residence) is well below 1 ms at p99 in every
  scenario. Its maximum is up to about 3 ms under the `spawn 600 fight` load. There is no backlog,
  so coalescing is not needed (plan, step 1).
- **Server step time** (`perf` step p99 about 2.9 ms in c) does not depend on NODELAY.

## Which metric to gate on

- `perceived` spreads by ±4-5 ms between runs and clients even in B1. The spread comes from the
  phase of the receiver's 20 ms step, not from the network.
- `transit` (corrected for the clock) spreads by less than 1 ms in B1.
- Gating B2 against B1:
  - for the **+2 ms** LAN criterion, use `transit` p95/p99, or pool many runs of `perceived`;
  - for the **+25 ms** and **p99 ≤ 100 ms** criteria, `perceived` is precise enough.

## Accuracy and caveats

- **Clock sync error.** Followers set the leader's clock with pings at connect. On one machine, the
  error of each pair of hosts is the difference of their `clock offset` lines (`[clock]` in
  `summary.txt`).
  - Client ↔ client: below 0.5 ms in most runs, at most 2.2 ms. The peer `perceived`/`transit`
    numbers above are accurate to about that.
  - Client ↔ server: about −6 ms on localhost in every run (the clients run 6 ms behind the leader).
    With 20 ms netem the error was −27 ms in 2 of the 3 B1-b runs, and about 0 ms in the third and
    in all B0-b runs.
  - So the server's own `perceived` and `transit` (from the clients' events) are shifted by +6 to
    +27 ms, and are left out of the tables (raw values are in `summary.txt`).
  - For B2 (relay), the client numbers stay comparable, because client↔client clock errors are
    small.
  - The size of the leader↔follower error is a finding for the time-sync work in step 2: the
    connect-time sync can be off by about one one-way delay.
- Across physical machines the clock error cannot be measured this way. Use the time-sync RTT
  (step 2) or the same-host harness.
- **Load.** A few units walk on each client, at about 30-40 events/s per client. This is enough to
  trigger Nagle; with more traffic per connection the B0 penalty could differ.
- **Run-to-run spread** comes from how many units actually walk (`xdotool` clicks into real
  windows). The ranges above are over 3 runs.
- **Environment:**
  - Manjaro, kernel 7.3, one machine. The GUI clients render on the desktop display (`:0`); no
    Xvfb was available.
  - Another agent ran GUI tests on the same display at the same time. One run failed because a
    join click got lost; it was repeated (B0 b run2).
- **Not measured here:**
  - an strace of the dial path of `TCP_NODELAY` on a follower. The peer-to-peer link is the
    follower's dialed socket, and B1-b is fast and symmetric on both clients, which shows that this
    socket has NODELAY too;
  - scenario c more than once per build;
  - the Windows and Haiku builds at runtime (the MinGW build compiles; `TCP_NODELAY` is in
    `winsock.h` and `netinet/tcp.h`).

## Step 3 (`net/conn-writers`): per-connection writers, still P2P

Same harness, scenarios a and b, 2 runs each of this branch (`adeff07`) and of the B1 binaries,
interleaved in one session (ports 17311/17312). Median and [range] over 2 runs × 2 clients, in ms.
`transit` is clock-corrected as above.

| Scenario | Build | perceived p50 | perceived p95 | perceived p99 | perceived max | transit p95 | transit p99 |
|---|---|---|---|---|---|---|---|
| a localhost | B1 | 21.4 [20.1–23.6] | 37.5 [37.5–39.5] | 39.8 [39.2–40.5] | 40.3 [39.6–41.0] | 19.9 [19.8–19.9] | 20.1 [20.1–20.2] |
| a localhost | step 3 | 19.5 [14.3–20.4] | 34.8 [28.8–36.4] | 37.8 [33.1–39.2] | 39.3 [34.2–40.1] | 19.9 [19.8–19.9] | 20.1 [20.1–20.2] |
| b 20±2 ms | B1 | 37.0 [15.4–66.2] | 52.9 [28.4–83.0] | 58.5 [32.8–87.0] | 60.8 [34.2–89.2] | 40.1 [40.0–40.2] | 41.5 [41.3–41.5] |
| b 20±2 ms | step 3 | 38.4 [34.8–42.1] | 55.1 [49.4–57.6] | 57.5 [54.9–60.4] | 60.7 [58.1–62.1] | 40.0 [39.3–40.9] | 41.8 [40.7–43.0] |

- No regression: `transit` is the same within 0.3 ms. The wide B1-b `perceived` range comes from
  the B1 clients' connect-time clock error to the server (−25 to −27 ms in run 1, `[clock]`; B1 has
  no in-game time sync), not from the network.
- `writer` (queued → written, every host): p99 0.15–0.85 ms, max below 0.9 ms in every run.
- The a/b runs above are of `adeff07` (32 KB send buffer, one write per message); at their load
  (30-40 events/s) the later writer changes make no difference.

**Battle load with delay** (scenario c settings — 2 hard CPUs, `spawn 600 fight`, about 1700
messages/s from the server to each client — plus netem; one run each, 40 s measured). Client
`transit` of the server's events, corrected by each client's `[clock]` offset to the server, p95/p99
in ms; server `writer` p99/max:

| Delay one way | Build | c1 transit | c2 transit | server writer | evicted |
|---|---|---|---|---|---|
| 20±2 ms | B1 | 41.5 / 58.3 | 41.2 / 53.6 | – | – |
| 20±2 ms | 32 KB, one write per message (`adeff07`) | 41.6 / 57.5 | 42.1 / 75.7 | 37.3 / 86.8 | no |
| 20±2 ms | 32 KB, backlog in one write | 40.4 / 42.0 | 40.8 / 41.9 | 6.5 / 39.1 | no |
| 20±2 ms | **64 KB, backlog in one write (`871a134`)** | 40.1 / 41.0 | 40.0 / 41.1 | 0.15 / 1.5 | no |
| 50±5 ms | B1 | 73.8 / 85.1 | 73.9 / 87.0 | – | – |
| 50±5 ms | 32 KB, one write per message | – | – | – | **both clients** |
| 50±5 ms | 32 KB, backlog in one write | 90.1 / 109.4 | 95.9 / 113.0 | 56.9 / 81.8 | no |
| 50±5 ms | **64 KB, backlog in one write (`871a134`)** | 73.6 / 89.6 | 72.6 / 74.3 | 20.6 / 77.4 | no |
| 50±5 ms | 128 KB, backlog in one write | 71.1 / 73.4 | 74.2 / 82.7 | 0.05 / 2.1 | no |

- With TCP_NODELAY every message is its own small segment, and the kernel charges each one far
  more than its 54 bytes against the send buffer. At 100 ms round trip a 32 KB buffer could not
  hold the segments in flight, so the writer queue grew and healthy clients were evicted.
- A larger buffer delays the eviction of a client that stopped reading (below), so 64 KB is a
  compromise; tune it in step 6 if needed.

**Head of line** (scenario c load, localhost; client 2 stopped with `kill -STOP` after 10 s of
measuring, for 5 s of walking that took about 7 s):

| Build | client 2 evicted | server `perf` step p99 / max | client 1 perceived p99 | client 1 writer max |
|---|---|---|---|---|
| `adeff07` (32 KB, one write per message) | yes, before `SIGCONT` (259 ms, 13 KB queued) | 2.99 / 7.04 | 39.5 | 0.63 |
| 32 KB, backlog in one write | no (the kernel buffers absorbed 7 s) | 3.14 / 9.61 | 38.1 | 0.52 |
| **`871a134` (64 KB, backlog in one write)** | yes, 6.7 s after `SIGSTOP` (260 ms, 15 KB queued) | 2.61 / 6.28 | 38.7 | 0.65 |
| 128 KB, backlog in one write | no | 3.10 / 4.81 | 37.8 | 0.39 |

- Nobody froze in any run. Client 1's `perceived` maximum (4.6–7 s) is client 2's backlog of
  states sent after `SIGCONT`.
- Eviction needs the kernel buffers (our send buffer and the stopped peer's receive buffer) to fill
  first, so how soon it comes depends on the traffic to that client: about 7 s at battle load,
  much later (or never, while it fits) at normal load.
- The evicted client shows "Connection to the server was lost!" after `SIGCONT`. Client 1 is not
  told: the in-game departure broadcast is step 4 (in P2P client 1 still has its own link to
  client 2).

## B2 (`net/relay`, rc.3 steps 4 and 5): relay through the server

Same harness, interleaved in one session: B1 a, b → B2 a, b, twice (ports 17411/17412), then
scenario c once each (port 17420). B2 binaries: `do-wt/baselines/B2/` (commit `7e63705`). Raw results:
`do-wt/baselines/results/b2cmp/run{1,2}/{B1,B2}/{a,b,c}/`.

With the relay, the other client's events go A → server → B. In this harness every packet crosses
the one netem'd loopback, so in b the relay path pays the 20 ms delay twice (A→S, S→B) where P2P
paid it once (A→B). On real networks the difference is owd(A,S) + owd(S,B) − owd(A,B) (plan,
section 3).

Median and [range] over 2 runs × 2 clients, in ms; `transit` clock-corrected as above:

| Scenario | Build | perceived p50 | perceived p95 | perceived p99 | perceived max | transit p95 | transit p99 |
|---|---|---|---|---|---|---|---|
| a localhost | B1 | 18.2 [15.1–20.9] | 35.3 [31.9–38.6] | 39.4 [39.1–40.0] | 40.4 [40.2–40.6] | 19.9 [19.8–19.9] | 20.1 [20.0–20.1] |
| a localhost | **B2** | 20.8 [16.2–23.9] | 37.8 [31.4–39.5] | 39.2 [38.2–40.0] | 40.1 [39.8–40.6] | **19.9** [19.8–19.9] | **20.2** [20.1–20.2] |
| b 20±2 ms | B1 | 39.7 [34.9–41.0] | 55.4 [51.5–58.5] | 58.8 [56.2–60.9] | 61.2 [60.1–62.5] | 40.2 [40.0–40.2] | 41.5 [41.4–41.5] |
| b 20±2 ms | **B2** | 59.7 [59.0–61.0] | **76.4** [73.2–78.2] | **80.5** [79.3–80.7] | 82.1 [80.1–83.2] | 60.5 [59.8–61.2] | 62.5 [61.7–63.4] |

Samples per client and run: 1284–2727.

**Scenario c** (2 hard CPUs, `spawn 600 fight`, one run each):

| Build | server `perf` step p99 / max | server `forward` p99 / max | client perceived p95 / p99 (c1, c2) |
|---|---|---|---|
| B1 | 1.87 / 6.76 | – | 30.5 / 36.2, 29.1 / 32.6 |
| B2 | 1.94 / 6.25 | 0.35 / 0.74 | 34.7 / 38.5, 35.2 / 38.6 |

In c most received events are the server's, and B1's clients run 6 ms behind the server's clock
(`[clock]`), which lowers their `perceived` by 6 ms; B2's clients are within 0.03 ms. Corrected, the
clients' numbers are the same within about 1 ms.

**Acceptance (plan, section 7):**
- LAN, +2 ms on `transit` p95: B2 − B1 = 0.0 ms (p99 +0.1 ms). Pass.
- 20 ms one way: perceived p95 B2 − B1 = +21.0 ms (≤ 25), p99 80.5 ms (≤ 100). Pass. The
  difference is the second 20 ms hop of this harness (`transit` +20.3 ms).
- Server forward p99 < 2 ms during `spawn 600 fight`: 0.35 ms. Pass. In every a/b run `forward` p99
  was 0.15–0.35 ms, max below 0.9 ms.
- Server step p99: 1.94 vs 1.87 ms, within the run-to-run spread (no regression).
- Clock: the step 2 time sync keeps client ↔ server below 1 ms in every B2 run (B1: −6 to −26 ms).

**NAT** (two clients in their own network namespaces, each with a veth link only to the server's
namespace, `ip_forward` 0, no routes between them; `c1 → c2` is "Network is unreachable"):
- B2: both clients play, each sees the other's units (perceived n = 1259 / 787). `strace` of each
  client: exactly one TCP `connect`, to its server address; the `accept` on the unused listen port
  never returns.
- B1: the game does not start. c1 `connect`s to c2 and gets `ENETUNREACH` ("cannot be reached, game
  not started"); c2 reports "did not connect to us".

**Head of line** (scenario c load, NAT namespaces, client 2 `kill -STOP`):
- 9 s and 25 s stops: client 1 perceived p99 39.1 / 38.9 ms, server step p99 2.5 / 2.2 ms; nobody
  froze. Client 1's perceived max (9.3 s / 25.6 s) is client 2's backlog after `SIGCONT`. Client 2
  was **not** evicted: its kernel receive buffer (`tcp_rmem` max 32 MB here) absorbed the backlog,
  so the server's queue never aged. Step 3 evicted after 6.7 s on plain loopback; a loopback STOP
  run of B2 was not repeated, so whether the namespaces/veth or the build make the difference is not
  confirmed. Eviction of a stopped client is therefore met only with a smaller receive buffer:
- 15 s stop with client 2's `tcp_rmem` max set to 256 KB: evicted ("oldest outgoing message has
  waited 259 ms, 15912 bytes queued"), client 1 logged "Player 2 (Bot2) was disconnected." and
  kept playing (perceived p99 38.8 ms, max 41.3 ms).
