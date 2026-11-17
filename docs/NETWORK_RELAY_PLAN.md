# Network relay: route in-game traffic through the server (0.7.0-rc.3 plan)

**Status:** accepted plan, implementation in progress on `integration/rc3`. Numbers are estimates until step 1 measures them.

**References:** code is named by file and symbol (line numbers drift). Paths are under `src/` unless stated otherwise. Baseline is 0.7.0-rc.2.

**Release order:**
- **rc.2 (released):** multiplayer fixes and network input validation. No wire change. It is the rollback target for rc.3.
- **rc.3:** relay, protocol v2 and the handshake. **BREAKING:** rc.3 cannot play with 0.6.0, rc.1 or rc.2.
- **0.7.0:** the stable release, after user testing.

## 1. Simulation and network model (as built)

**There is no lockstep and no fixed input delay.**
- Every machine simulates only the units of its own (local) players. All other units are replicas, driven by the state events their owner broadcasts:
  - the broadcast filter in `doforces.cpp`;
  - the `SendNetEvent(..., all_players)` calls in the buildings, factories, map-units, sources and workers code.
- A request (RQ_*) for a unit whose owner is remote goes only to that owner (`dounits.cpp`). Example: a mining request to a source on the server.

**Scheduling: no stall and no rollback.**
- `ProcessFunction` (`doengine.cpp`) runs every 20 ms and executes every event whose `ts <= now`.
- Remote events are applied when they arrive, however late. Two kinds are dropped:
  - a remote event that is older than the last one processed for that unit;
  - a remote move into a cell that is occupied (`doforces.cpp`).

**Timestamp lead.**
- **Movement: none.** A movement event carries its scheduled time, which is about "now" minus 0-20 ms of tick lag.
- **Combat: 0-400 ms.** RQ_FIRE_OFF and RQ_IMPACT are sent with future timestamps (`doforces.cpp`). 12 of the 24 `offensive_shot_time` entries are 0, so many attacks have no lead.
- Damage is applied only by the victim's owner (`dounits.cpp`).

**Consequence:** transport latency never stalls the game. It shows up as replicas that are more out of date and as more rejected events.

**Own units are unaffected.** `SendRequest` (`dounits.cpp`) queues an event locally when the owner is local, so the delay from input to action does not depend on the topology.

**Clock.**
- Followers take the leader's clock from hello plus 50 pings at connect.
- `ProcessPingReply` steps the clock whenever it sees a new minimum RTT.
- There is no ping during a game, so clock drift over a long game is not corrected.

**Headless server.** `RunDedicatedServer` is a stdin loop that polls every 100 ms. Neither it nor the simulation thread sits on the receive path (listener thread → dispatcher). `doipc.*` only provides locks.

## 2. Current flow and transport

**Game start.**
- The leader sends `player_array` (`doleader.cpp`). It holds each player's address as the leader saw it, which for a client behind NAT is the **public NAT IP**.
- Followers then connect to each other. Each one dials every player with a higher id. For players with a lower id it waits up to 180 × 0.5 s **while holding `giant`**. The message says 15 s; the real wait is 90 s.
- Behind NAT, and for two players behind the same NAT on a cloud server, that dial fails. N2 reports the failure.

**Messages.**
- Header: 4 B (size, type, subtype, dest). `dest` is sent on the wire (`donet.h`). A message is at most 255 B (`donet.h`).
- Event payload from `LinearizeEvent` (`doevents.cpp`): **50 B on 64-bit, 42 B on 32-bit** (54/46 B including the header). The sizes differ because two fields are `intptr_t`. Raw host byte order is used and the length is never checked.

**TCP.**
- `SO_KEEPALIVE` is set through `EnableKeepalive` (`donet.cpp`), called on dial and accept.
- **`TCP_NODELAY` is set nowhere, neither in master nor in N2.** Nagle can therefore hold back a small write by up to an RTT, and longer when it meets delayed ACK.

**Threads.**
- Listener: one thread per connection, blocking `recv` (`donet.cpp`).
- Dispatcher: one thread; `ProcessNetEvent` runs under `giant`.
- Talker: **one thread doing blocking `send`** for every connection. `PutMessage` blocks when its queue is full.
- The net threads never sleep. In steady state the only added delay is the 20 ms tick, plus whatever time the dispatcher waits for `giant`, which nobody has measured yet.

## 3. Latency

**Formula.** Δ = (A→S→B) − (A→B) = owd(A,S) + owd(S,B) + forward time − owd(A,B), where owd is one-way delay.
- Δ depends on **where the server is** relative to the players; it is not a fixed bound.
- For CZ/SK players and a Czech datacenter (RTT to the server about 10-40 ms), the estimate is (RTT_A + RTT_B)/2 ≈ 10-40 ms through the relay. A working direct path would be about 5-20 ms.
- Server RTT is measured (see section 7).

**Where relay costs nothing or wins:**
- **GUI-hosted LAN game:** traffic is relayed by the LAN host, so it stays at LAN speed (a fraction of a ms more).
- **LAN players on a cloud server:** they do not work today, because peers are handed the public NAT IP. With relay they work, at server RTT.
- **Anyone behind NAT:** fails today, works with relay.

**Rules for latency:**
- forward in the listener thread straight after the message is received and validated, never after the server's simulation step or a tick;
- set `TCP_NODELAY` on every socket;
- add no sleeps anywhere.

**Relay also removes the dial-and-wait phase** at game start, which today waits up to 90 s under `giant`.

## 4. Decision: always relay (star), one topology

| Option | Verdict |
|---|---|
| (a) Always relay | **Chosen.** One code path. Works wherever the lobby works. Costs nothing on GUI-hosted LAN games. |
| (b) Direct first, relay as per-pair fallback | Rejected. It needs per-pair negotiation and dial timeouts (`connect` has none today). It would only help a case that does not work today anyway. |
| (c) Relay on dedicated servers, P2P on a GUI host | Rejected. A GUI host behind NAT would still leave its followers dialing each other. |

- There is **no `--p2p` switch, no relay-mode byte and no lobby env plumbing.**
- The P2P dial code is removed from the follower in rc.3. The follower's listen port stays unused until it is cleaned up after rc.3.
- **Rollback:** redeploy the rc.2 server image and point the downloads back to rc.2.

## 5. Protocol v2 (rc.3, wire break)

**Serialization (mandatory, since the wire breaks anyway).**
- Fixed-width fields in network byte order: player id u8, unit i32, ts f64 sent as u64 bits, event/last/req i32, 6×u8, int1/int2 i64. That is 47 B, the same on every platform.
- Exact length check before a payload is parsed. The `Extract*` calls are bounded, with a `remaining()` helper. This builds on `fix/net-input-hardening`.

**Handshake.**
- Constant `NET_PROTOCOL_VERSION = 2`. `connect` gets a trailer `u16 proto` + version string, detected with `remaining()`.
- Each connection has a pre-admission state. The check runs **before** `ConnectFollower` / `AddRemotePlayer`. A client is admitted only if its protocol matches.
- A rejected connection gets **only a targeted reply**: never a broadcast, never a real `player_array` or start message. The server closes it after the reply.
- **Reply to an old client (0.6.0, rc.1, rc.2):**
  - Preferred, only if verified with the real rc.2 binary: a lobby-only `player_array` addressed to that connection, with the map name `UPDATE CLIENT TO 0.7.0-rc.3`. The old client would show "Map not found: …" with Play disabled (`MenuUpdateMapInfo`, ).
  - Otherwise: a plain chat message and close.
- **rc.3 client → old server or GUI host:** the `hello` has no trailer, so the client disconnects with "Host runs an older Dark Oberon; both sides need 0.7.0-rc.3".
- Formats have been unchanged since v0.6.0 (`git diff v0.6.0 master -- src/donet.cpp` changes only the "refused" flag; v0.6.0 already has the map hash). An unknown message type only logs a `Critical` (`donet.cpp`); it does not crash.

**Routing (leader).**
- **Origin** is derived from the connection the message came in on. Whatever origin or dest a follower supplies is never trusted for identity.
- On the leader→follower leg the header carries the origin id in the old `dest` byte. Inside the leader, `route` and `origin` are separate local fields of the message. Leader-originated messages carry the sending leader-side player.
- A **player id → connection** table replaces the TNET_ADDRESS alias objects.
  - Followers map every non-local id to the leader connection.
  - The leader maps follower ids to their connections; leader-local ids (hyper player 0, host human, CPUs) are handled locally.
  - The table is read as a snapshot under a short dedicated lock. The lock is never held across send or enqueue.
- **Connections have stable handles** (fd + generation). Exactly **one owner closes a socket** (the connection object). This removes fd-reuse confusion and the trap where one alias shuts down the shared leader socket.
- Inbound message from follower F, after validation:

  | Type and dest | Action |
  |---|---|
  | broadcast | Dispatch locally (the leader keeps replicas) and forward to every follower except F. |
  | dest is leader-local | Dispatch locally only. |
  | dest is another follower | Forward only. Never dispatch. |

- **Message ownership:** a forward is a **separate pooled copy or raw byte buffer**. The same TNET_MESSAGE never goes to both the dispatcher and a writer.

**Validation before forwarding (blocker).** A message is forwarded only if all of these hold:
- the connection is admitted;
- the game phase is right (in-game events only after the start);
- the type and subtype are on an allow-list for relay (event, chat, in-game ping; disconnect only from the leader);
- the payload length is exact for the type;
- player ids are within range;
- **ownership:**
  - a non-RQ event must have a `player_id` owned by F;
  - an RQ request is routed to the real owner of the target unit, which must be a valid id (for example, a mining request to a server-owned source is handled locally); only the kinds a host sends to another player's unit (`RQ_CAN_MINE`) go to another owner, every other request is an owner's broadcast of its own unit;
- **only the leader announces departures.**

Anything else is dropped. Malformed input, forged departures and anything a not yet admitted connection sends besides connect and ping are counted, and a connection that keeps sending them is evicted. A misrouted event (a broadcast or request for another player's unit) is dropped without counting: a replica of an honest host can produce one.

**Ordering and departures.**
- Order is FIFO per origin through the relay. Each destination gets exactly one delivery.
- A **departure barrier** is queued behind the leaving player's events that are already queued, so the departure is processed after them. Later events from a player who has left are dropped.
- The leader also sends `SendDisconnect(id)` for departures **during the game**; N2 sends it only before the start.
- **Losing the server** is one explicit terminal transition that reuses the N4 message ("Connection to the server was lost!"). It never runs `ProcessDisconnect` for each player; today that would end in "You WON!".

**Clock.**
- A new message type `net_protocol_time_sync` (handlers are keyed by type). The follower sends it every 2 s; the leader answers **without `giant`** and **only to the requester**.
- The follower **corrects** its clock with min-RTT filtering. It slews in steps of at most 2 ms instead of jumping. Each sample also gives the RTT metric.

## 6. Per-connection writers (head-of-line blocking, blocker)

- Every connection has **one bounded outgoing queue and one writer**: a thread per connection, or nonblocking I/O with a poll loop. The writer is the **only** code that writes to that fd.
- Enqueueing a forward never blocks.
- **Limits:** queued bytes (256 KB) and **queue age**. If the queue is full, or the oldest queued message is older than 2 s, the client is evicted; a message is never dropped from the stream (the peer would miss it without noticing). 250 ms was too tight for WAN play: one TCP retransmission on Wi-Fi alone takes 200 ms plus the round trip. Eviction closes the socket through its owner and sends the departure broadcast.
- **No `SO_SNDTIMEO` approach:** a 3 s stall would freeze everyone for 3 s.
- The incoming side keeps one reader per connection. The dispatcher's `PutMessage` must not stall the reader for a long time; this is measured as queue residence time.

## 7. Measurement and acceptance

**Metrics.** Kept only where an acceptance criterion needs them. Printed by the server `perf`/`netstat` commands and a dev-console `netstat`, and logged at game end.
- **Perceived latency:** the time from when a remote event is generated to when it is applied on the receiver. The sender's send time is embedded in debug builds, or carried as an optional trailer while measuring. Report p50, p95, p99 and max.
- **Receive → dispatch:** time waiting on `giant` and time sitting in the queue. On the server, forward latency (received → written) at p99, plus writer queue age and depth.
- **Server RTT and clock offset**, from the time-sync messages.
- **Server load for 4 humans:** bandwidth, packets/s and CPU. Each broadcast becomes 3 copies; the upper estimate is about 390 KB/s.

**Baselines:**

| Name | What |
|---|---|
| B0 | P2P as shipped (the rc.2 build) |
| B1 | P2P + `TCP_NODELAY` (rc.3 step 1) |
| B2 | relay |

**B2 is gated against B1**, not only against B0. Otherwise the Nagle fix could hide a regression caused by relay.

**Harness.**
- **Transport bots:** a `netbot.py` protocol client running on **one host**, timed with a monotonic clock, with no game simulation.
- **Realistic load:** real clients plus the headless server with `spawn 600 fight` (`perf_smoke.sh` pattern).
- **`netem`** in **both directions**, in network namespaces. It needs root, runs locally only, and never touches lolbit.

**Acceptance criteria:**
1. **LAN / GUI host:** B2 perceived p95 ≤ B1 p95 + 2 ms. Server forward p99 < 2 ms during `spawn 600 fight`.
2. **20 ± 2 ms one-way, both directions, per client:**
   - B2 perceived p95 ≤ B1 p95 + 25 ms, with an absolute limit of p99 ≤ 100 ms;
   - no regression in the `perf` step p99.
3. **Head-of-line tests:**
   - one client with 0.5% loss and 20 ms delay → the other clients' p99 does not change by more than 10 ms;
   - `kill -STOP` one client for 5 s → the others stay below p99 100 ms, and the stopped client is evicted and announced as departed.
4. **Clock drift:** the offset at minute 1 and at minute 30 differs by less than 5 ms, measured on two physical hosts.
5. **Own units:** input → first local US_MOVE is the same in B0 and B2.
6. **Soak:** 30 minutes, 4 humans + 2 CPUs, 10 ms jitter. No disconnects and no stuck "Waiting".
7. **Version pairings** (section 5): no crash, and a readable message in each.

**Test matrix.**
- Topologies: dedicated server on a local docker lobby, and a GUI host, with 2, 3 or 4 humans, with and without CPUs.
- Clients: Linux, Windows, Haiku VM, and **macOS built from source**.
- **NAT:** the Haiku VirtualBox VM on NAT networking plus a host client, against a server outside the NAT. This fails on rc.2 and must pass on rc.3.
- Old-client pairings: the rc.1 and rc.2 binaries against an rc.3 server, and rc.3 against an rc.2 host.
- Disruptions: the server is killed mid-game, and a follower quits mid-game.

## 8. Release and downloads (two systems, not one atomic step)

- **The problem:** the product page (`deploy/srv_dark-oberon/src/static/index.html`, `server.py`) links to GitHub `releases/latest`, which **skips pre-releases**. rc.3 testers would download rc.2 and then fail the handshake.
- **The fix, one of:**
  - the lobby `DOWNLOAD_URL` (`server/web/app.py` (`DOWNLOAD_URL`)) points at the rc.3 GitHub pre-release while rc.3 is in testing;
  - or `server.py` learns to offer the newest pre-release.
- The lobby already shows `GAME_VERSION` (`app.py`). Add "requires client 0.7.0-rc.3" next to it.
- **Deployment order:**
  1. Publish the GitHub pre-release with every client build, including notes for building on macOS from source.
  2. Deploy the server image (the rc.3 `dark-oberon-server`, with `VERSION` set to 0.7.0-rc.3, and the download link).
  3. Announce. Between steps 1 and 2, rc.3 clients see the "older host" message and rc.2 clients keep working.
- Touch lolbit only when the user says so. Rehearse on a local docker lobby first.
- **CHANGELOG (rc.3), under Changed:** `**BREAKING:** network protocol v2; in-game traffic goes through the server; 0.6.0/rc.1/rc.2 clients cannot join rc.3 servers or hosts and vice versa.` Under the pre-1.0 caveat a MINOR (0.7.0) may break compatibility. The 0.7.0 notes add "0.6.x players must update".
- **Rollback:** redeploy the rc.2 image and point `DOWNLOAD_URL` back to rc.2; on macOS, check out the `v0.7.0-rc.2` tag. rc.2 servers and rc.2 clients remain compatible with each other.

## 9. Implementation steps (rc.3)

0. **(done in rc.2)** network input validation: bounded `Extract*`, exact event length, player-id bounds.
1. **`net/nodelay-metrics`:** `TCP_NODELAY` on every socket, plus the gating metrics from section 7. Measure B0 (the rc.2 build) and B1. Coalescing is postponed until the metrics show a backlog.
2. **`net/protocol-v2`:** fixed-width serialization in network byte order with length checks, the handshake with pre-admission state, the targeted rejection, the time-sync message, and unit tests for serialize/parse.
3. **`net/conn-writers`:** connection objects (handle + generation, one owner closes), a bounded queue and a writer per connection, the byte and age limits, eviction, and the player-id → connection table with its snapshot lock.
4. **`net/relay-leader`:** forward in the listener after validation, the routing table, the separate pooled copies, departure barrier plus in-game `SendDisconnect`, and ownership checks.
5. **`net/relay-follower`:** all remote ids route to the leader, no peer dial or 90 s wait, chat origin, and the terminal "server lost" state.
6. **`net/harness`:** netbot, real-client load runs, netem in both directions, and the head-of-line and drift tests. Measure B2 and go through the acceptance criteria and the test matrix.
7. **Release rc.3:** CHANGELOG, `VERSION`, the GitHub pre-release, and the lobby download link and server image, deployed in the order of section 8.

**After rc.3:** remove the follower listen port and the P2P leftovers; add coalescing if the metrics show a backlog; consider rate limits per connection.

## 10. Risks

- **Correctness of the relay** (ownership, ordering, departures): covered by validation, the barrier, and the disconnect and loss tests.
- **The server is a single point of failure.** It already is one, as clock master and lobby. Losing it is one clean terminal state.
- **Latency on badly placed servers:** Δ grows with the server's distance. That is acceptable for CZ/SK and the Czech datacenter, and it is measured through server RTT.
- **Scope creep in rc.3** (writers plus v2 plus relay). This is why the work is split into branches 1-5 that can each be reviewed on their own.

## Review changes (opposition panel → this revision)

**Changed:**
- Relay always, with no `--p2p` switch, relay byte or env plumbing. Rollback is a redeploy of rc.2.
- The LAN case is addressed directly: LAN players on a cloud server fail today, and a GUI-hosted LAN game stays at LAN speed.
- Latency is now given as Δ = (A→S→B) − (A→B), which depends on where the server is. The earlier "+0-20 ms" was wrongly presented as a bound.
- Head-of-line blocking is fixed with a per-connection bounded writer, byte and age limits, and eviction. This replaces the 3 s `SO_SNDTIMEO` approach.
- Forwards are separate pooled copies. Origin comes from the connection, `route` is separate from `dest`, and a player-id → connection table replaces the aliases.
- Connections have stable handles with a generation, and there is a single owner of close.
- Validation before forwarding is now a blocker. It builds on `fix/net-input-hardening` from rc.2.
- Handshake: bounded trailer detection, gating before admission, and targeted rejection. Fixed-width serialization in network byte order is now **mandatory**. The fake map name is used only if it is verified with the rc.2 binary.
- Ordering: FIFO per origin, a departure barrier, dropping events from players who left, and a terminal "server lost" state.
- Clock sync during the game now **corrects** the clock (it was measure-only). It is a new message type, answered without `giant`, only to the requester. A drift criterion was added.
- Measurement: perceived latency (generation → applied), receive → dispatch, B2 gated against B1, netem in both directions, head-of-line and STOP tests, server bandwidth/packets/CPU (about 390 KB/s upper estimate), and single-host bots plus real-client load.
- Release: GitHub `releases/latest` skips pre-releases, so a download path for rc.3 and a deployment order across the two systems were added. "Atomic" was dropped. macOS was added to the matrix and to rollback.
- Scope: NODELAY + metrics are step 1. Coalescing and the non-gating counters were cut.
- Facts corrected: the payload is 50/42 B (54/46 B with the header), keepalive line references, and the claims about dispatcher cost (now measured instead of asserted).

**Rejected:**
- Hybrid P2P (b): its complexity is not justified, since the case it helps fails today.
- Relay only on dedicated servers (c): it does not fix GUI hosts behind NAT.
- A wrapping relay message type: it costs 5 B per message, and the origin fits in the existing `dest` byte.
- A P2P fallback switch inside rc.3: it would mean two topologies to test, and redeploying rc.2 covers rollback.
- A measure-only ping: it would leave clock drift uncorrected over long games.

## Carried over from the rc.2 network review
- Connection identity: the talker's address lists and raw fd numbers are shared between threads without one owner. The connection handles and route table of step 3 replace them.
- Host teardown must not run while `giant` is held by a disconnect callback; move it to the main loop.
- The accept wait at game start holds `giant`; relay removes the follower dial and wait entirely.
- Robustness of shutdown paths (host leaving during the join handshake, a mid-game signal) is re-tested in the step 6 disruption runs.
- UX: a follower whose only opponent left before the start should get a clear message once departure ordering is reworked.
