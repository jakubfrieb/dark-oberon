#!/usr/bin/env python3
"""
Dark Oberon lobby: Flask UI + subprocess game servers (one container).

Anyone can see the running games and their connect addresses. Hosting a game needs an
account: each user may run one game at a time, and only its owner can start or stop it.
Games nobody is connected to are stopped after LOBBY_IDLE_MINUTES.
"""
from __future__ import annotations

import atexit
import datetime
import functools
import hmac
import json
import os
import re
import secrets
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

from flask import (Flask, abort, jsonify, redirect, render_template, request, session,
                   url_for)
from werkzeug.middleware.proxy_fix import ProxyFix

from accounts import AccountError, RateLimiter, UserStore

BASE_DIR = Path(os.environ.get("DATA_DIR", "/app")).resolve()
WEB_DIR = Path(__file__).resolve().parent

SERVER_BINARY = os.environ.get("SERVER_BINARY", str(BASE_DIR / "dark-oberon-server"))
DATA_DIR = str(BASE_DIR)
PORT_RANGE_START = int(os.environ.get("PORT_RANGE_START", "17000"))
PORT_RANGE_END = int(os.environ.get("PORT_RANGE_END", "17009"))
MAPS_DIR = Path(os.environ.get("MAPS_DIR", str(BASE_DIR / "maps")))
WEB_PORT = int(os.environ.get("WEB_PORT", "8080"))
GAME_PUBLIC_HOST = (os.environ.get("GAME_PUBLIC_HOST") or "").strip() or None

# Accounts, sessions, housekeeping
LOBBY_DATA_DIR = Path(os.environ.get("LOBBY_DATA_DIR", str(BASE_DIR / "lobby-data")))
TRUSTED_PROXIES = int(os.environ.get("TRUSTED_PROXIES", "0"))
SESSION_COOKIE_SECURE = os.environ.get("SESSION_COOKIE_SECURE", "0") == "1"
IDLE_SECONDS = int(float(os.environ.get("LOBBY_IDLE_MINUTES", "30")) * 60)
MAX_GAME_SECONDS = int(float(os.environ.get("LOBBY_MAX_GAME_HOURS", "8")) * 3600)
REAPER_INTERVAL = 60
REGISTER_LIMIT = 5          # new accounts per IP per hour
LOGIN_FAIL_LIMIT = 10       # failed logins per IP per 15 minutes

# Names the server reports for non-human slots.
NON_HUMAN_PLAYERS = {"HyperPlayer", "Computer"}


def log(msg: str) -> None:
    print(f"[lobby] {msg}", file=sys.stderr, flush=True)


def _secret_key() -> str:
    key = os.environ.get("LOBBY_SECRET_KEY", "").strip()
    if key:
        return key
    # Persist a generated key next to the user database so sessions survive restarts.
    path = LOBBY_DATA_DIR / "secret_key"
    LOBBY_DATA_DIR.mkdir(parents=True, exist_ok=True)
    if not path.is_file():
        path.write_text(secrets.token_hex(32))
        path.chmod(0o600)
    return path.read_text().strip()


app = Flask(__name__, template_folder=str(WEB_DIR / "templates"),
            static_folder=str(WEB_DIR / "static"))
app.config.update(
    SECRET_KEY=_secret_key(),
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE="Lax",
    SESSION_COOKIE_SECURE=SESSION_COOKIE_SECURE,
    PERMANENT_SESSION_LIFETIME=datetime.timedelta(days=30),
)
if TRUSTED_PROXIES > 0:
    app.wsgi_app = ProxyFix(app.wsgi_app, x_for=TRUSTED_PROXIES, x_proto=TRUSTED_PROXIES,
                            x_host=TRUSTED_PROXIES)

users = UserStore(LOBBY_DATA_DIR / "lobby.db")
register_limiter = RateLimiter(REGISTER_LIMIT, 3600)
login_limiter = RateLimiter(LOGIN_FAIL_LIMIT, 900)


class GameInstance:
    def __init__(self, inst_id: str, map_name: str, port: int, proc: subprocess.Popen,
                 owner: str):
        self.id = inst_id
        self.map_name = map_name
        self.port = port
        self.proc = proc
        self.owner = owner
        self.created_at = time.time()
        self.idle_since: float | None = None
        self.stopping = False          # set when the lobby itself ends the game
        self._lock = threading.Lock()
        self.last_json = None

        threading.Thread(target=self._stderr_reader, daemon=True).start()

    def _stderr_reader(self):
        # Must keep draining stderr until the server exits: once the pipe buffer (~64 KB) is full,
        # the game server blocks on its next log line and the game freezes for everyone.
        while True:
            try:
                line = self.proc.stderr.readline()
            except (ValueError, OSError):   # pipe closed
                break
            except Exception as e:          # never die silently; decoding itself is lenient
                log(f"{self.label()}: error reading the server log: {e!r}")
                time.sleep(0.1)
                continue
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

    def label(self) -> str:
        return f"{self.map_name}:{self.port} of {self.owner}"

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
        self.stopping = True
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

    def status(self) -> dict:
        with self._lock:
            return dict(self.last_json) if self.last_json else {}

    def humans(self) -> list[str]:
        return [p for p in self.status().get("players", []) if p not in NON_HUMAN_PLAYERS]

    def snapshot(self, viewer: str | None = None):
        j = self.status()
        alive = self.proc.poll() is None
        snap: dict = {
            "id": self.id,
            "map": self.map_name,
            "port": self.port,
            "owner": self.owner,
            "mine": viewer is not None and viewer.lower() == self.owner.lower(),
            "alive": alive,
            "exit_code": self.proc.poll() if not alive else None,
            "players": [p for p in j.get("players", []) if p != "HyperPlayer"],
            "started": bool(j.get("started")),
            "created_at": int(self.created_at),
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
        raise RuntimeError("All game slots are taken right now. Try again later.")

    def owned_by(self, owner: str) -> GameInstance | None:
        with self._instances_lock:
            for inst in self._instances.values():
                if inst.owner.lower() == owner.lower() and inst.proc.poll() is None:
                    return inst
        return None

    def create(self, map_name: str, owner: str) -> GameInstance:
        map_name = map_name.strip()
        if map_name.endswith(".map"):
            map_name = map_name[:-4]
        if not re.match(r"^[a-zA-Z0-9_-]+$", map_name):
            raise ValueError("Invalid map id")
        map_file = MAPS_DIR / f"{map_name}.map"
        if not map_file.is_file():
            raise ValueError(f"Map not found: {map_name}")

        # Hold lock for the one-per-user check, port allocation, Popen and register.
        with self._instances_lock:
            if self.owned_by(owner):
                raise PermissionError("You already have a game running. Stop it to create a new one.")
            port = self._alloc_port()
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
                # Player names and map texts need not be valid UTF-8; a strict decoder would
                # raise in the log reader.
                encoding="utf-8",
                errors="replace",
                bufsize=1,
            )
            inst = GameInstance(inst_id, map_name, port, proc, owner)
            self._instances[inst_id] = inst
            log(f"started {inst.label()} (pid {proc.pid})")
            return inst

    def get(self, inst_id: str) -> GameInstance | None:
        with self._instances_lock:
            return self._instances.get(inst_id)

    def remove(self, inst_id: str) -> None:
        with self._instances_lock:
            self._instances.pop(inst_id, None)

    def remove_dead(self) -> None:
        with self._instances_lock:
            dead = [k for k, v in self._instances.items() if v.proc.poll() is not None]
            for k in dead:
                inst = self._instances.pop(k)
                if not inst.stopping:
                    code = inst.proc.poll()
                    how = f"killed by signal {-code}" if code < 0 else f"exit code {code}"
                    log(f"game {inst.label()} ended unexpectedly ({how})")

    def stop_game(self, inst: GameInstance, why: str) -> None:
        log(f"stopping {inst.label()} ({why})")
        inst.stop()
        self.remove(inst.id)

    def shutdown(self, why: str) -> None:
        """Stop every running game, e.g. when this lobby process exits."""
        with self._instances_lock:
            instances = [i for i in self._instances.values() if i.proc.poll() is None]
        if instances:
            log(f"{why}: stopping {len(instances)} running game(s)")
        for inst in instances:
            self.stop_game(inst, why)

    def list_snapshots(self, viewer: str | None = None):
        with self._instances_lock:
            self.remove_dead()
            instances = list(self._instances.values())
        for inst in instances:
            inst.request_status()
        time.sleep(0.05)
        return [inst.snapshot(viewer) for inst in instances]

    def reap(self, now: float | None = None, request_status: bool = True) -> None:
        """Stop games nobody plays in (after IDLE_SECONDS) and games older than MAX_GAME_SECONDS."""
        now = time.time() if now is None else now
        with self._instances_lock:
            self.remove_dead()
            instances = list(self._instances.values())
        for inst in instances:
            if request_status:
                inst.request_status()
            if inst.humans():
                inst.idle_since = None
            elif inst.idle_since is None:
                inst.idle_since = max(inst.created_at, now - REAPER_INTERVAL)
            too_old = now - inst.created_at > MAX_GAME_SECONDS
            idle = inst.idle_since is not None and now - inst.idle_since > IDLE_SECONDS
            if too_old or idle:
                self.stop_game(inst, "max lifetime" if too_old else "idle")


manager = InstanceManager()
log(f"lobby worker {os.getpid()} started")
# Game servers are children of this worker and quit when their stdin closes, so a worker that
# exits (restart, shutdown) takes every game with it. Say so in the log instead of dropping
# players silently. (A SIGKILL, e.g. gunicorn's worker timeout, cannot be caught; gunicorn
# logs "WORKER TIMEOUT" then.)
atexit.register(lambda: manager.shutdown(f"lobby worker {os.getpid()} exiting"))


def _reaper_loop():
    while True:
        time.sleep(REAPER_INTERVAL)
        try:
            manager.reap()
        except Exception as e:  # never let housekeeping kill the thread
            log(f"reaper error: {e!r}")


if os.environ.get("LOBBY_REAPER", "1") != "0":
    threading.Thread(target=_reaper_loop, daemon=True).start()


# --- sessions, CSRF -------------------------------------------------------------

def current_user() -> str | None:
    return session.get("user")


def csrf_token() -> str:
    if "csrf" not in session:
        session["csrf"] = secrets.token_urlsafe(32)
    return session["csrf"]


def _csrf_ok(sent: str | None) -> bool:
    expected = session.get("csrf")
    return bool(expected and sent and hmac.compare_digest(expected, sent))


app.jinja_env.globals["csrf_token"] = csrf_token


def client_ip() -> str:
    return request.remote_addr or "unknown"


def api_login_required(fn):
    @functools.wraps(fn)
    def wrapper(*args, **kwargs):
        if not _csrf_ok(request.headers.get("X-CSRF-Token")):
            return jsonify({"error": "Your session expired. Reload the page."}), 400
        if not current_user():
            return jsonify({"error": "Sign in to host a game."}), 401
        return fn(*args, **kwargs)
    return wrapper


def owned_instance(inst_id: str) -> GameInstance:
    inst = manager.get(inst_id)
    if not inst:
        abort(404)
    if inst.owner.lower() != current_user().lower():
        abort(403)
    return inst


@app.errorhandler(403)
def _forbidden(_e):
    return jsonify({"error": "Only the player who created this game can control it."}), 403


@app.errorhandler(404)
def _not_found(_e):
    if request.path.startswith("/api/"):
        return jsonify({"error": "This game no longer exists."}), 404
    return redirect(url_for("index"))


# --- pages ----------------------------------------------------------------------

@app.route("/")
def index():
    return render_template("index.html", user=current_user(),
                           game_public_host=GAME_PUBLIC_HOST or "",
                           idle_minutes=IDLE_SECONDS // 60)


def _auth_page(template: str, **ctx):
    return render_template(template, user=current_user(), **ctx)


def _check_form_csrf():
    if not _csrf_ok(request.form.get("csrf_token")):
        abort(400)


@app.route("/register", methods=["GET", "POST"])
def register():
    if request.method == "GET":
        return _auth_page("register.html")
    _check_form_csrf()
    username = request.form.get("username", "")
    ip = client_ip()
    if register_limiter.blocked(ip):
        return _auth_page("register.html", error="Too many new accounts from your network. "
                          "Try again in an hour.", username=username), 429
    try:
        name = users.create(username, request.form.get("password", ""),
                            request.form.get("password2", ""))
    except AccountError as e:
        return _auth_page("register.html", error=str(e), username=username), 400
    register_limiter.hit(ip)
    session.clear()
    session.permanent = True
    session["user"] = name
    return redirect(url_for("index"))


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "GET":
        return _auth_page("login.html")
    _check_form_csrf()
    username = request.form.get("username", "")
    ip = client_ip()
    if login_limiter.blocked(ip):
        return _auth_page("login.html", error="Too many failed sign-ins. Try again in 15 minutes.",
                          username=username), 429
    name = users.authenticate(username, request.form.get("password", ""))
    if not name:
        login_limiter.hit(ip)
        return _auth_page("login.html", error="Wrong username or password.", username=username), 401
    session.clear()
    session.permanent = True
    session["user"] = name
    return redirect(url_for("index"))


@app.post("/logout")
def logout():
    _check_form_csrf()
    session.clear()
    return redirect(url_for("index"))


# --- API ------------------------------------------------------------------------

@app.get("/api/maps")
def api_maps():
    if not MAPS_DIR.is_dir():
        return jsonify({"maps": []})
    maps = sorted(p.stem for p in MAPS_DIR.glob("*.map"))
    return jsonify({"maps": maps})


@app.get("/api/instances")
def api_instances():
    return jsonify({"instances": manager.list_snapshots(current_user())})


@app.post("/api/instances")
@api_login_required
def api_create():
    data = request.get_json(silent=True) or {}
    map_name = data.get("map") or data.get("map_id")
    if not map_name:
        return jsonify({"error": "Pick a map first."}), 400
    try:
        inst = manager.create(str(map_name), current_user())
    except PermissionError as e:
        return jsonify({"error": str(e)}), 409
    except ValueError as e:
        return jsonify({"error": str(e)}), 400
    except RuntimeError as e:
        return jsonify({"error": str(e)}), 503
    inst.request_status()
    time.sleep(0.2)
    return jsonify(inst.snapshot(current_user())), 201


@app.post("/api/instances/<inst_id>/start")
@api_login_required
def api_start_game(inst_id):
    inst = owned_instance(inst_id)
    if inst.proc.poll() is not None:
        abort(404)
    try:
        inst.start_game()
    except RuntimeError as e:
        return jsonify({"error": str(e)}), 500
    inst.request_status()
    time.sleep(0.2)
    return jsonify(inst.snapshot(current_user()))


@app.delete("/api/instances/<inst_id>")
@api_login_required
def api_stop(inst_id):
    inst = owned_instance(inst_id)
    manager.stop_game(inst, "stopped by its owner")
    return jsonify({"ok": True})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=WEB_PORT, threaded=True)
