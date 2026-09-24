# GDB: dark-oberon client (segfault / backtrace)

When the GUI client crashes — run from the **repo root** (where `./dark-oberon` lives after `make` in `src/`).

## Batch (run → game exits → backtrace → GDB exits)

```bash
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)" && \
gdb -q --batch -ex 'set pagination off' -ex run -ex 'thread apply all bt' -ex quit \
  --args ./dark-oberon 2>&1 | tail -80
```

- Output goes to the console only (no log file).
- **Do not run in the background** — the command runs until the game ends (normal exit or crash).
- After the game process ends, GDB prints the stacks of all threads and **exits on its own** (`quit`); no interactive `(gdb)` prompt is left.

## Agent: how to invoke

Run the command above **in the foreground** (no `block_until_ms: 0` / no background). After a crash or when the game is closed, GDB exits by itself and the terminal command finishes — the output is in the same terminal.

The tool timeout (`block_until_ms`) is **optional** — if the environment requires an upper bound, use e.g. **600000** (10 minutes); otherwise feel free to omit it.

Do not use interactive `gdb -ex run` without `--batch` if you want it to exit automatically after a crash.
