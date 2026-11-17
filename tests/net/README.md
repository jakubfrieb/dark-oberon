# Network latency measurement (tests/net)

Measures how late the events of remote units arrive, with real game clients, for the relay plan
(`docs/NETWORK_RELAY_PLAN.md`, section 7). Results of the baselines: `BASELINE.md`.

## Metrics (`netstat`)

Every host keeps three fixed histograms of the events it receives (`src/doperf.cpp`):

| Line | What |
|---|---|
| `transit` | received by the listener − time stamp of the event (the owner's game clock when the state happened) |
| `dispatch` | received by the listener − put into the event queue (waiting for `giant` + dispatcher queue) |
| `perceived` | applied to the replica by the simulation − time stamp of the event |

- Only unit **states** count for `transit` and `perceived`; requests and `US_ZOMBIE`/`US_DELETE` are
  scheduled ahead. `dispatch` counts every event.
- No wire change: the time stamps are already on the wire in the leader's synced game clock, so the
  same numbers work for P2P (B0, B1) and the relay (B2).
- A state's time stamp is the simulation step it was due in, so `transit` includes up to one 20 ms
  step on the sender, and `perceived` adds up to one 20 ms step on the receiver. Both are the same
  in every topology.
- **Clock error:** each line set ends with `clock offset` = game clock − monotonic clock. On one
  machine, the difference between two hosts' offsets is their clock sync error, and it shifts
  `transit` and `perceived` by exactly that much (`run.sh` prints the differences as `[clock]`).
  Across machines it cannot be removed this way.
- Counting starts 5 s after the simulation starts. The events queued while the map loads are left
  out.
- Where to read it:
  - The dedicated server has the console command `netstat [reset]` and writes a log line at exit.
  - A GUI client writes `netstat:` lines to `~/.dark-oberon/logs/full.log` when it leaves a network
    game.

## Running

```sh
tests/net/build.sh OUT                    # GUI client + server of the working tree into OUT
NET_NO_NODELAY=1 tests/net/build.sh OUT   # the same without TCP_NODELAY (baseline B0)
tests/net/run.sh BIN_DIR OUT_DIR          # one measurement, settings in the env (see the script)
tests/net/baseline.sh BIN_DIR OUT_ROOT    # scenarios a, b, c of BASELINE.md
```

`run.sh` does the following:
1. It starts a user and network namespace (`unshare -Urn`, no root needed) and, when a delay is
   set, adds netem on its loopback. Every packet passes loopback once, so both directions get the
   delay.
2. It starts the headless server and two GUI clients, each with its own HOME.
3. It joins both clients through the menu with `xdotool` (windows are found by PID) and starts the
   game. The clients keep selecting their starting units and sending them between two points.
4. It prints the `netstat` of every host and the server `perf` to `OUT_DIR/summary.txt`.
   - A run whose client has fewer than `NET_MIN_SAMPLES` samples is marked `INVALID`.

Requirements and limits:
- It needs an X display (`$DISPLAY`, a real one works), `xdotool`, ImageMagick `import`, `tc`/`ip`
  (iproute2) and `bc`.
- It moves the real mouse pointer, so do not use the mouse during a run.
- The server port is 17100 by default (`NET_PORT`).
