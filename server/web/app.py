#!/usr/bin/env python3
"""
Dark Oberon lobby: Flask UI + subprocess game servers (one container).
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import threading
import time
import uuid
from pathlib import Path

from flask import Flask, jsonify, render_template, request

BASE_DIR = Path(os.environ.get("DATA_DIR", "/app")).resolve()
WEB_DIR = Path(__file__).resolve().parent
app = Flask(__name__, template_folder=str(WEB_DIR / "templates"))

SERVER_BINARY = os.environ.get("SERVER_BINARY", str(BASE_DIR / "dark-oberon-server"))
DATA_DIR = str(BASE_DIR)
PORT_RANGE_START = int(os.environ.get("PORT_RANGE_START", "17000"))
PORT_RANGE_END = int(os.environ.get("PORT_RANGE_END", "17009"))
MAPS_DIR = Path(os.environ.get("MAPS_DIR", str(BASE_DIR / "maps")))
WEB_PORT = int(os.environ.get("WEB_PORT", "8080"))
GAME_PUBLIC_HOST = (os.environ.get("GAME_PUBLIC_HOST") or "").strip() or None


class GameInstance:
    def __init__(self, inst_id: str, map_name: str, port: int, proc: subprocess.Popen):
        self.id = inst_id
        self.map_name = map_name
        self.port = port
        self.proc = proc
        self._lock = threading.Lock()
        self.last_json = None

        threading.Thread(target=self._stderr_reader, daemon=True).start()

    def _stderr_reader(self):
        import sys
        try:
            for line in iter(self.proc.stderr.readline, ""):
                if not line:
                    break
                line = line.strip()
                print(f"[server:{self.port}] {line}", file=sys.stderr, flush=True)
                if line.startswith('{"players"'):
                    try:
                        data = json.loads(line)
                        with self._lock:
                            self.last_json = data
                    except json.JSONDecodeError:
                        pass
        except Exception:
            pass

    def request_status(self):
        if self.proc.poll() is not None:
            return
        try:
            self.proc.stdin.write("players\n")
            self.proc.stdin.flush()
        except (BrokenPipeError, OSError, AttributeError):
            pass
        time.sleep(0.15)

    def stop(self):
        try:
            if self.proc.stdin:
                self.proc.stdin.write("quit\n")
                self.proc.stdin.flush()
        except (BrokenPipeError, OSError, AttributeError):
            pass
        try:
            self.proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                pass

    def start_game(self):
        try:
            self.proc.stdin.write("start\n")
            self.proc.stdin.flush()
        except (BrokenPipeError, OSError, AttributeError) as e:
            raise RuntimeError(str(e)) from e

    def snapshot(self):
        with self._lock:
            j = dict(self.last_json) if self.last_json else None
        alive = self.proc.poll() is None
        snap: dict = {
            "id": self.id,
            "map": self.map_name,
            "port": self.port,
            "alive": alive,
            "exit_code": self.proc.poll() if not alive else None,
            "status": j,
        }
        if GAME_PUBLIC_HOST:
            snap["connect"] = f"{GAME_PUBLIC_HOST}:{self.port}"
        return snap


class InstanceManager:
    def __init__(self):
        self._instances: dict[str, GameInstance] = {}
        # RLock: list_snapshots calls remove_dead while holding the lock.
        self._instances_lock = threading.RLock()

    def _ports_in_use(self) -> set[int]:
        return {i.port for i in self._instances.values() if i.proc.poll() is None}

    def _alloc_port(self) -> int:
        used = self._ports_in_use()
        for p in range(PORT_RANGE_START, PORT_RANGE_END + 1):
            if p not in used:
                return p
        raise RuntimeError("No free port in configured range")

    def create(self, map_name: str, port: int | None = None) -> GameInstance:
        map_name = map_name.strip()
        if map_name.endswith(".map"):
            map_name = map_name[:-4]
        if not re.match(r"^[a-zA-Z0-9_-]+$", map_name):
            raise ValueError("Invalid map id")
        map_file = MAPS_DIR / f"{map_name}.map"
        if not map_file.is_file():
            raise ValueError(f"Map not found: {map_name}")

        # Hold lock for allocation + Popen + register so two threads cannot pick the same port.
        with self._instances_lock:
            if port is None:
                port = self._alloc_port()
            else:
                if not PORT_RANGE_START <= port <= PORT_RANGE_END:
                    raise ValueError(
                        f"Port must be between {PORT_RANGE_START} and {PORT_RANGE_END}"
                    )
                if port in self._ports_in_use():
                    raise ValueError("Port already in use")

            inst_id = str(uuid.uuid4())
            proc = subprocess.Popen(
                [
                    SERVER_BINARY,
                    "--map",
                    map_name,
                    "--port",
                    str(port),
                    "--data",
                    DATA_DIR,
                ],
                stdin=subprocess.PIPE,
                stderr=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                text=True,
                bufsize=1,
            )
            inst = GameInstance(inst_id, map_name, port, proc)
            self._instances[inst_id] = inst
            return inst

    def get(self, inst_id: str) -> GameInstance | None:
        with self._instances_lock:
            return self._instances.get(inst_id)

    def remove_dead(self) -> None:
        with self._instances_lock:
            dead = [k for k, v in self._instances.items() if v.proc.poll() is not None]
            for k in dead:
                del self._instances[k]

    def list_snapshots(self):
        with self._instances_lock:
            self.remove_dead()
            instances = list(self._instances.values())
        for inst in instances:
            inst.request_status()
        time.sleep(0.05)
        return [inst.snapshot() for inst in instances]


manager = InstanceManager()


@app.route("/")
def index():
    return render_template("index.html", game_public_host=GAME_PUBLIC_HOST or "")


@app.get("/api/maps")
def api_maps():
    if not MAPS_DIR.is_dir():
        return jsonify({"maps": []})
    maps = sorted(p.stem for p in MAPS_DIR.glob("*.map"))
    return jsonify({"maps": maps})


@app.get("/api/instances")
def api_instances():
    return jsonify({"instances": manager.list_snapshots()})


@app.post("/api/instances")
def api_create():
    data = request.get_json(silent=True) or {}
    map_name = data.get("map") or data.get("map_id")
    if not map_name:
        return jsonify({"error": "missing map"}), 400
    port = data.get("port")
    if port is not None:
        port = int(port)
    try:
        inst = manager.create(str(map_name), port)
    except ValueError as e:
        return jsonify({"error": str(e)}), 400
    except RuntimeError as e:
        return jsonify({"error": str(e)}), 503
    inst.request_status()
    time.sleep(0.2)
    return jsonify(inst.snapshot()), 201


@app.post("/api/instances/<inst_id>/start")
def api_start_game(inst_id):
    inst = manager.get(inst_id)
    if not inst or inst.proc.poll() is not None:
        return jsonify({"error": "not found"}), 404
    try:
        inst.start_game()
    except RuntimeError as e:
        return jsonify({"error": str(e)}), 500
    inst.request_status()
    time.sleep(0.2)
    return jsonify(inst.snapshot())


@app.delete("/api/instances/<inst_id>")
def api_stop(inst_id):
    inst = manager.get(inst_id)
    if not inst:
        return jsonify({"error": "not found"}), 404
    inst.stop()
    with manager._instances_lock:
        manager._instances.pop(inst_id, None)
    return jsonify({"ok": True})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=WEB_PORT, threaded=True)
