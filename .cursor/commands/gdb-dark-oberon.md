# GDB: dark-oberon klient (segfault / backtrace)

Při pádu GUI klienta — spusť z **kořene repa** (kde leží `./dark-oberon` po `make` ve `src/`).

## Batch (run → crash → backtrace → log)

```bash
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)" && \
gdb -q --batch -ex 'set pagination off' -ex run -ex 'thread apply all bt' -ex quit \
  --args ./dark-oberon 2>&1 | tee gdb-client-crash.log
```

Output goes to both console and `gdb-client-crash.log`.

## Agent: how to invoke

Run the command above directly — do NOT pipe through `tail` or other filters, so output streams in real-time. Use `block_until_ms: 0` to background it immediately, then read the terminal file or `gdb-client-crash.log` for results after the user closes the game / it crashes.
