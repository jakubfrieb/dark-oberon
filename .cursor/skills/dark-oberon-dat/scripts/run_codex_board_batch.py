#!/usr/bin/env python3
"""
Codex-driven orc restyle of Dark Oberon AI boards.

  design  — codex draws one design sheet per entity   -> <work>/design/<entity>.png
  approve — mark design sheets approved by the user   -> <work>/design/approved.json
  restyle — codex repaints every board of approved     -> <work>/raw/<board>.png
            entities (input: board flattened on white + design sheet)

State in <work>/_codex_state.json; finished jobs are skipped unless --force.

Usage:
    run_codex_board_batch.py design  <work> --entities footman[,..|all] [--refs DIR]
    run_codex_board_batch.py approve <work> footman [peasant ...]
    run_codex_board_batch.py restyle <work> [--only footman,..] [--animation stay]
    (design/restyle: [--parallel N] [--force] [--dry-run])
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image

from board_postprocess import flatten_on_white, slots_bbox
from codex_prompts import design_prompt, restyle_prompt

HERE = Path(__file__).resolve().parent


class CodexUsageLimit(RuntimeError):
    """codex refused to run: the ChatGPT plan usage limit is exhausted (retrying is pointless)."""


def codex_command(images: list[Path], work: Path) -> list[str]:
    cmd = ["codex", "exec", "--skip-git-repo-check", "-s", "workspace-write", "-C", str(work)]
    for img in images:
        cmd += ["-i", str(img)]
    return cmd + ["-"]


def run_codex(prompt: str, images: list[Path], expect: Path, work: Path,
              retries: int = 3, timeout: int = 600) -> bool:
    before = expect.stat().st_mtime if expect.exists() else None
    for attempt in range(1, retries + 1):
        print(f"  codex {expect.name} attempt {attempt}/{retries}", file=sys.stderr)
        try:
            res = subprocess.run(codex_command(images, work), input=prompt, text=True,
                                 cwd=str(work), timeout=timeout, check=False,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        except subprocess.TimeoutExpired:
            continue
        out = res.stdout or ""
        if "usage limit" in out:
            line = next((l for l in out.splitlines() if "usage limit" in l), "usage limit")
            raise CodexUsageLimit(line.strip())
        if expect.exists() and expect.stat().st_mtime != before:
            try:
                with Image.open(expect) as im:
                    im.verify()
                return True
            except Exception:
                expect.unlink(missing_ok=True)
        time.sleep(1)
    return False


class Batch:
    def __init__(self, work: Path, entities: dict, runner=run_codex, refs: list[Path] | None = None):
        self.work = Path(work)
        self.entities = entities
        self.runner = runner
        self.refs = list(refs or [])
        self.lock = threading.Lock()
        self.state_path = self.work / "_codex_state.json"
        self.state = json.loads(self.state_path.read_text()) if self.state_path.exists() else {}
        self.manifest = json.loads((self.work / "boards/_boards_manifest.json").read_text())

    # -- state --------------------------------------------------------------
    def _set(self, key: str, ok: bool) -> None:
        with self.lock:
            # merge with what other processes wrote meanwhile
            if self.state_path.exists():
                self.state = json.loads(self.state_path.read_text())
            prev = self.state.get(key, {})
            self.state[key] = {"status": "done" if ok else "failed",
                               "attempts": prev.get("attempts", 0) + 1}
            self.state_path.write_text(json.dumps(self.state, indent=2))

    def _done(self, key: str, out: Path) -> bool:
        return self.state.get(key, {}).get("status") == "done" and out.exists()

    def _run_jobs(self, jobs: list[tuple], parallel: int, dry_run: bool) -> dict:
        """jobs: (key, prompt, images, out)"""
        if dry_run:
            for key, prompt, images, out in jobs:
                print(f"--- {key} -> {out}\n$ {' '.join(codex_command(images, self.work))}\n{prompt}\n")
            return {}

        def one(job):
            key, prompt, images, out = job
            ok = self.runner(prompt, images, out, self.work)
            self._set(key, ok)
            print(f"  {'done  ' if ok else 'FAILED'} {key}")
            return key, ok

        with ThreadPoolExecutor(max_workers=max(1, parallel)) as ex:
            return dict(ex.map(one, jobs))

    # -- helpers ------------------------------------------------------------
    def _entity_type(self, eid: str) -> str:
        for b in self.manifest["boards"]:
            if b["entity_id"] == eid:
                return b["entity_type"]
        raise KeyError(eid)

    def _flat_input(self, board: dict) -> Path:
        """Board flattened on white and cropped to its slots (codex fills the canvas)."""
        dst = self.work / "codex_in" / board["board_file"]
        if not dst.exists():
            dst.parent.mkdir(parents=True, exist_ok=True)
            flat = flatten_on_white(Image.open(self.work / "boards" / board["board_file"]))
            flat.crop(slots_bbox(board)).save(dst)
        return dst

    def _human_preview(self, eid: str) -> Path:
        boards = {b["animation"]: b for b in self.manifest["boards"] if b["entity_id"] == eid}
        board = boards.get("stay") or boards.get("picture") or next(iter(boards.values()))
        return self._flat_input(board)

    # -- design -------------------------------------------------------------
    def design(self, entity_ids: list[str], force=False, parallel=1, dry_run=False) -> dict:
        jobs = []
        for eid in entity_ids:
            out = self.work / "design" / f"{eid}.png"
            key = f"design:{eid}"
            if not force and self._done(key, out):
                print(f"  skip {key} (done)")
                continue
            out.parent.mkdir(parents=True, exist_ok=True)
            prompt = design_prompt(eid, self.entities[eid], self._entity_type(eid),
                                   f"design/{eid}.png")
            jobs.append((key, prompt, [self._human_preview(eid), *self.refs], out))
        return self._run_jobs(jobs, parallel, dry_run)

    # -- approve ------------------------------------------------------------
    def _approved(self) -> dict:
        p = self.work / "design" / "approved.json"
        return json.loads(p.read_text()) if p.exists() else {}

    def approve(self, entity_ids: list[str]) -> None:
        approved = self._approved()
        for eid in entity_ids:
            if not (self.work / "design" / f"{eid}.png").exists():
                raise FileNotFoundError(f"design sheet missing for {eid}")
            approved[eid] = True
        (self.work / "design" / "approved.json").write_text(json.dumps(approved, indent=2))

    # -- restyle ------------------------------------------------------------
    def restyle(self, only=None, animation=None, force=False, parallel=1, dry_run=False, hint=None) -> dict:
        approved = self._approved()
        jobs = []
        for b in self.manifest["boards"]:
            eid = b["entity_id"]
            if (only and eid not in only) or (animation and b["animation"] != animation):
                continue
            if not approved.get(eid):
                print(f"  SKIP {b['board_id']}: design not approved")
                continue
            out = self.work / "raw" / b["board_file"]
            key = f"board:{b['board_id']}"
            if not force and self._done(key, out):
                continue
            src = self._flat_input(b)
            out.parent.mkdir(parents=True, exist_ok=True)
            prompt = restyle_prompt(b, self.entities[eid], f"raw/{b['board_file']}", hint)
            jobs.append((key, prompt, [src, self.work / "design" / f"{eid}.png"], out))
        return self._run_jobs(jobs, parallel, dry_run)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--entities-json", type=Path, default=HERE / "orc_entities.json")
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("design"); d.add_argument("work", type=Path)
    d.add_argument("--entities", required=True); d.add_argument("--refs", type=Path)
    a = sub.add_parser("approve"); a.add_argument("work", type=Path); a.add_argument("entities", nargs="+")
    r = sub.add_parser("restyle"); r.add_argument("work", type=Path)
    r.add_argument("--only"); r.add_argument("--animation")
    r.add_argument("--hint", default=None, help="extra instruction appended to the restyle prompt")
    for p in (d, r):
        p.add_argument("--parallel", type=int, default=1)
        p.add_argument("--force", action="store_true")
        p.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    entities = json.loads(args.entities_json.read_text("utf-8"))
    work = args.work.resolve()
    refs = sorted(Path(args.refs).resolve().glob("*.png")) if getattr(args, "refs", None) else []
    batch = Batch(work, entities, refs=refs)

    if args.cmd == "design":
        ids = list(entities) if args.entities == "all" else args.entities.split(",")
        res = batch.design(ids, args.force, args.parallel, args.dry_run)
    elif args.cmd == "approve":
        batch.approve(args.entities)
        print("approved:", ", ".join(args.entities))
        return 0
    else:
        only = set(args.only.split(",")) if args.only else None
        res = batch.restyle(only, args.animation, args.force, args.parallel, args.dry_run, args.hint)
    failed = [k for k, ok in res.items() if not ok]
    print(f"\n{len(res) - len(failed)} done, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
