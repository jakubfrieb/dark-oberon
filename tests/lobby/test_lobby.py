"""Lobby web app: accounts, one game per user, owner-only control, idle reaper.

The game server binary is replaced by FakeProc, so no C++ build is needed.
"""
from __future__ import annotations

import importlib
import io
import re
import sys
from pathlib import Path

import pytest

WEB_DIR = Path(__file__).resolve().parents[2] / "server" / "web"


class FakeStdin(io.StringIO):
    def __init__(self, proc):
        super().__init__()
        self.proc = proc

    def write(self, s):
        self.proc.commands.append(s.strip())
        if s.strip() == "quit":
            self.proc.returncode = 0
        return len(s)

    def flush(self):
        pass


class FakeProc:
    instances: list["FakeProc"] = []

    def __init__(self, args, **kwargs):
        self.args = args
        self.commands: list[str] = []
        self.returncode = None
        self.stdin = FakeStdin(self)
        self.stderr = io.StringIO("")
        FakeProc.instances.append(self)

    def poll(self):
        return self.returncode

    def wait(self, timeout=None):
        return self.returncode

    def kill(self):
        self.returncode = -9


@pytest.fixture()
def lobby(tmp_path, monkeypatch):
    maps = tmp_path / "maps"
    maps.mkdir()
    (maps / "crossroads.map").write_text("x")
    (maps / "twin_ponds.map").write_text("x")
    monkeypatch.setenv("DATA_DIR", str(tmp_path))
    monkeypatch.setenv("MAPS_DIR", str(maps))
    monkeypatch.setenv("LOBBY_DATA_DIR", str(tmp_path / "lobby-data"))
    monkeypatch.setenv("GAME_PUBLIC_HOST", "game.example.org")
    monkeypatch.setenv("LOBBY_REAPER", "0")
    monkeypatch.setenv("PORT_RANGE_START", "17000")
    monkeypatch.setenv("PORT_RANGE_END", "17002")
    monkeypatch.syspath_prepend(str(WEB_DIR))
    for mod in ("app", "accounts"):
        sys.modules.pop(mod, None)
    app_mod = importlib.import_module("app")
    monkeypatch.setattr(app_mod.subprocess, "Popen", FakeProc)
    monkeypatch.setattr(app_mod.time, "sleep", lambda s: None)
    FakeProc.instances = []
    app_mod.app.config["TESTING"] = True
    return app_mod


def csrf_of(client):
    html = client.get("/").get_data(as_text=True)
    return re.search(r'name="csrf-token" content="([^"]+)"', html).group(1)


def form_csrf(client, path):
    html = client.get(path).get_data(as_text=True)
    return re.search(r'name="csrf_token" value="([^"]+)"', html).group(1)


def register(client, name="jakub", password="tajneheslo1"):
    token = form_csrf(client, "/register")
    return client.post("/register", data={
        "username": name, "password": password, "password2": password, "csrf_token": token,
    })


def login(client, name="jakub", password="tajneheslo1"):
    token = form_csrf(client, "/login")
    return client.post("/login", data={"username": name, "password": password, "csrf_token": token})


def create_game(client, map_name="crossroads"):
    return client.post("/api/instances", json={"map": map_name},
                       headers={"X-CSRF-Token": csrf_of(client)})


# --- accounts ---------------------------------------------------------------

def test_register_logs_in_and_redirects_home(lobby):
    c = lobby.app.test_client()
    r = register(c)
    assert r.status_code == 302 and r.headers["Location"].endswith("/")
    assert "jakub" in c.get("/").get_data(as_text=True)


def test_register_rejects_bad_input(lobby):
    c = lobby.app.test_client()
    assert "Username" in register(c, name="a").get_data(as_text=True)
    assert "at least 8" in register(c, name="valid_name", password="short").get_data(as_text=True)
    token = form_csrf(c, "/register")
    r = c.post("/register", data={"username": "other", "password": "tajneheslo1",
                                  "password2": "jineheslo22", "csrf_token": token})
    assert "do not match" in r.get_data(as_text=True)


def test_username_is_unique_case_insensitive(lobby):
    register(lobby.app.test_client(), name="Jakub")
    r = register(lobby.app.test_client(), name="jakub")
    assert "already taken" in r.get_data(as_text=True)


def test_password_is_stored_hashed(lobby):
    register(lobby.app.test_client(), password="tajneheslo1")
    raw = (Path(lobby.LOBBY_DATA_DIR) / "lobby.db").read_bytes()
    assert b"tajneheslo1" not in raw


def test_login_logout(lobby):
    register(lobby.app.test_client())
    c = lobby.app.test_client()
    assert "Wrong username or password" in login(c, password="spatne-heslo").get_data(as_text=True)
    assert login(c).status_code == 302
    assert c.post("/logout", data={"csrf_token": csrf_of(c)}).status_code == 302
    assert create_game(c).status_code == 401


def test_forms_require_csrf(lobby):
    c = lobby.app.test_client()
    r = c.post("/register", data={"username": "jakub", "password": "tajneheslo1", "password2": "tajneheslo1"})
    assert r.status_code == 400


def test_registration_is_rate_limited_per_ip(lobby):
    for i in range(lobby.REGISTER_LIMIT):
        assert register(lobby.app.test_client(), name=f"user{i}").status_code == 302
    r = register(lobby.app.test_client(), name="one_too_many")
    assert r.status_code == 429


# --- games ------------------------------------------------------------------

def test_anonymous_cannot_create_but_can_list(lobby):
    c = lobby.app.test_client()
    assert create_game(c).status_code == 401
    assert c.get("/api/instances").get_json() == {"instances": []}


def test_one_game_per_user(lobby):
    c = lobby.app.test_client()
    register(c)
    r = create_game(c)
    assert r.status_code == 201
    game = r.get_json()
    assert game["owner"] == "jakub" and game["mine"] is True
    assert game["connect"] == "game.example.org:17000"
    assert create_game(c, "twin_ponds").status_code == 409


def test_api_requires_csrf_header(lobby):
    c = lobby.app.test_client()
    register(c)
    assert c.post("/api/instances", json={"map": "crossroads"}).status_code == 400


def test_only_owner_controls_game(lobby):
    owner = lobby.app.test_client()
    register(owner, name="owner")
    game = create_game(owner).get_json()

    other = lobby.app.test_client()
    register(other, name="other")
    listed = other.get("/api/instances").get_json()["instances"][0]
    assert listed["owner"] == "owner" and listed["mine"] is False
    hdr = {"X-CSRF-Token": csrf_of(other)}
    assert other.post(f"/api/instances/{game['id']}/start", headers=hdr).status_code == 403
    assert other.delete(f"/api/instances/{game['id']}", headers=hdr).status_code == 403

    hdr = {"X-CSRF-Token": csrf_of(owner)}
    assert owner.post(f"/api/instances/{game['id']}/start", headers=hdr).status_code == 200
    assert "start" in FakeProc.instances[0].commands
    assert owner.delete(f"/api/instances/{game['id']}", headers=hdr).status_code == 200
    assert create_game(owner).status_code == 201  # slot freed after stop


def test_invalid_map_rejected(lobby):
    c = lobby.app.test_client()
    register(c)
    r = c.post("/api/instances", json={"map": "../../etc/passwd"},
               headers={"X-CSRF-Token": csrf_of(c)})
    assert r.status_code == 400


# --- idle reaper ------------------------------------------------------------

def test_reaper_stops_game_without_humans_after_timeout(lobby):
    c = lobby.app.test_client()
    register(c)
    game = create_game(c).get_json()
    inst = lobby.manager.get(game["id"])
    inst.last_json = {"players": ["HyperPlayer", "Computer"], "started": False}

    t0 = inst.created_at
    lobby.manager.reap(now=t0 + 60, request_status=False)
    assert inst.proc.poll() is None
    lobby.manager.reap(now=t0 + lobby.IDLE_SECONDS + 120, request_status=False)
    assert inst.proc.poll() is not None
    assert lobby.manager.get(game["id"]) is None


def test_reaper_keeps_game_with_humans(lobby):
    c = lobby.app.test_client()
    register(c)
    game = create_game(c).get_json()
    inst = lobby.manager.get(game["id"])
    inst.last_json = {"players": ["HyperPlayer", "Jakub"], "started": True}
    lobby.manager.reap(now=inst.created_at + lobby.IDLE_SECONDS + 120, request_status=False)
    assert inst.proc.poll() is None


def test_reaper_enforces_max_lifetime(lobby):
    c = lobby.app.test_client()
    register(c)
    game = create_game(c).get_json()
    inst = lobby.manager.get(game["id"])
    inst.last_json = {"players": ["HyperPlayer", "Jakub"], "started": True}
    lobby.manager.reap(now=inst.created_at + lobby.MAX_GAME_SECONDS + 1, request_status=False)
    assert inst.proc.poll() is not None


# --- page ---------------------------------------------------------------------

def test_index_escapes_username(lobby):
    c = lobby.app.test_client()
    register(c, name="jakub")
    html = c.get("/").get_data(as_text=True)
    assert 'data-user="jakub"' in html
