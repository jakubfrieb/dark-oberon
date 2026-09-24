#!/usr/bin/env python3
"""
Monitor and manage Codex / OpenAI image board batch jobs.

Usage:
  python3 codex_job_monitor.py <boards-dir>           # Show status
  python3 codex_job_monitor.py <boards-dir> --watch   # Live polling
  python3 codex_job_monitor.py <boards-dir> --reset   # Clear state for re-run
  python3 codex_job_monitor.py <boards-dir> --preview # Open last edited board
"""

import argparse, json, sys, time, os
from pathlib import Path
from datetime import datetime, timedelta

STATE_FILE = "_batch_state.json"
MANIFEST_FILE = "_boards_manifest.json"


def load_state(boards_dir: Path) -> dict:
    p = boards_dir / STATE_FILE
    if p.exists():
        return json.loads(p.read_text())
    return None


def count_boards(boards_dir: Path, entities: str = None) -> list:
    all_boards = sorted(
        p.name for p in boards_dir.glob("*.png")
        if not p.name.startswith("_") and not p.name.startswith("test_")
    )
    if entities:
        ents = [e.strip() for e in entities.split(",")]
        all_boards = [b for b in all_boards if any(b.startswith(f"{e}__") for e in ents)]
    return all_boards


def get_entities(boards: list) -> dict:
    entities = {}
    for b in boards:
        ent = b.split("__")[0]
        entities.setdefault(ent, []).append(b)
    return entities


def format_duration(seconds: float) -> str:
    if seconds < 60:
        return f"{seconds:.0f}s"
    elif seconds < 3600:
        return f"{seconds/60:.1f}m"
    else:
        h = int(seconds // 3600)
        m = int((seconds % 3600) // 60)
        return f"{h}h{m:02d}m"


def show_status(boards_dir: Path):
    state = load_state(boards_dir)
    all_boards = count_boards(boards_dir)
    total = len(all_boards)

    if not total:
        print("No boards found.")
        return

    entities = get_entities(all_boards)

    if state is None:
        print(f"No batch started yet. {total} boards across {len(entities)} entities.")
        print("\nEntities:")
        for ent, boards in sorted(entities.items()):
            print(f"  {ent}: {len(boards)} boards")
        return

    edited_dir = boards_dir / "edited"
    if "completed" in state or "failed" in state:
        completed = set(state.get("completed", []))
        failed = set(state.get("failed", []))
    else:
        # OpenAI batch state: derive completed from edited/ files (custom_id == board name without .png)
        completed = set(
            f"{p.stem}.png" for p in edited_dir.glob("*.png")
        ) if edited_dir.exists() else set()
        failed = set()
    n_done = len(completed)
    n_fail = len(failed)
    n_pending = total - n_done - n_fail

    edited_count = len(list(edited_dir.glob("*.png"))) if edited_dir.exists() else 0
    if state.get("batch_id"):
        print(f"OpenAI batch: {state['batch_id']}  status={state.get('status', '?')}")

    # Estimate time remaining based on edited file timestamps
    eta_str = "?"
    if edited_dir.exists() and edited_count >= 2:
        edited_files = sorted(edited_dir.glob("*.png"), key=lambda p: p.stat().st_mtime)
        if len(edited_files) >= 2:
            first_t = edited_files[0].stat().st_mtime
            last_t = edited_files[-1].stat().st_mtime
            elapsed = last_t - first_t
            if elapsed > 0 and edited_count > 1:
                avg_per_board = elapsed / (edited_count - 1)
                remaining_s = avg_per_board * n_pending
                eta_str = format_duration(remaining_s)

    pct = (n_done / total * 100) if total else 0
    bar_len = 40
    filled = int(bar_len * n_done / total)
    bar = "█" * filled + "░" * (bar_len - filled)

    print(f"Codex Batch: {boards_dir}")
    print(f"  [{bar}] {pct:.0f}%")
    print(f"  Done: {n_done}/{total}  Failed: {n_fail}  Pending: {n_pending}")
    print(f"  Edited files: {edited_count}")
    if n_pending > 0:
        print(f"  ETA: ~{eta_str}")
    print()

    # Per-entity breakdown
    print("Entities:")
    for ent, boards in sorted(entities.items()):
        ent_done = sum(1 for b in boards if b in completed)
        ent_fail = sum(1 for b in boards if b in failed)
        ent_total = len(boards)
        status = "DONE" if ent_done == ent_total else f"{ent_done}/{ent_total}"
        if ent_fail:
            status += f" ({ent_fail} failed)"
        print(f"  {ent:20s} {status}")

    if failed:
        print(f"\nFailed boards:")
        for f in sorted(failed):
            print(f"  {f}")


def watch_status(boards_dir: Path, interval: int = 10):
    print(f"Watching {boards_dir} (Ctrl+C to stop)\n")
    try:
        while True:
            os.system("clear" if os.name != "nt" else "cls")
            print(f"[{datetime.now().strftime('%H:%M:%S')}]")
            show_status(boards_dir)

            state = load_state(boards_dir)
            all_boards = count_boards(boards_dir)
            if state:
                completed = set(state.get("completed", []))
                failed = set(state.get("failed", []))
                if len(completed) + len(failed) >= len(all_boards):
                    print("\nBatch complete!")
                    break

            time.sleep(interval)
    except KeyboardInterrupt:
        print("\nStopped watching.")


def reset_state(boards_dir: Path):
    state_path = boards_dir / STATE_FILE
    edited_dir = boards_dir / "edited"
    if state_path.exists():
        state_path.unlink()
        print(f"Removed {state_path}")
    if edited_dir.exists():
        import shutil
        shutil.rmtree(edited_dir)
        print(f"Removed {edited_dir}")
    print("State reset. Ready for a fresh run.")


def preview_latest(boards_dir: Path):
    edited_dir = boards_dir / "edited"
    if not edited_dir.exists():
        print("No edited boards yet.")
        return
    latest = max(edited_dir.glob("*.png"), key=lambda p: p.stat().st_mtime, default=None)
    if latest:
        print(f"Latest: {latest}")
        print(f"  Modified: {datetime.fromtimestamp(latest.stat().st_mtime).strftime('%Y-%m-%d %H:%M:%S')}")
        # Try to open with system viewer
        if sys.platform == "linux":
            os.system(f'xdg-open "{latest}" 2>/dev/null &')
        elif sys.platform == "darwin":
            os.system(f'open "{latest}"')
    else:
        print("No edited boards yet.")


def main():
    parser = argparse.ArgumentParser(description="Monitor SD board batch jobs")
    parser.add_argument("boards_dir", help="Path to boards/ directory")
    parser.add_argument("--watch", "-w", action="store_true",
        help="Live polling mode")
    parser.add_argument("--interval", "-i", type=int, default=10,
        help="Polling interval in seconds (default: 10)")
    parser.add_argument("--reset", action="store_true",
        help="Clear state and edited boards for a fresh run")
    parser.add_argument("--preview", "-p", action="store_true",
        help="Open the latest edited board in system viewer")

    args = parser.parse_args()
    boards_dir = Path(args.boards_dir)

    if not boards_dir.is_dir():
        sys.exit(f"Not a directory: {boards_dir}")

    if args.reset:
        reset_state(boards_dir)
    elif args.preview:
        preview_latest(boards_dir)
    elif args.watch:
        watch_status(boards_dir, args.interval)
    else:
        show_status(boards_dir)


if __name__ == "__main__":
    main()
