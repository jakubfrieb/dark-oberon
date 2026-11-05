#!/usr/bin/env python3
"""Dark Oberon web: the product page.

Standard library only. Serves:
  /, /en/        static/index.html, static/en/index.html and the files next to them
  /api/status    the newest GitHub release (version + download links per platform)
                 and how many games run in the lobby right now

Both upstreams are cached, so the page costs GitHub at most one request per hour
(the unauthenticated API allows 60 per hour). When an upstream fails the last good
answer is kept; with none, the field is null and the page falls back to static links.

Configuration (environment):
  GITHUB_REPO  owner/name with the releases (jakubfrieb/dark-oberon)
  LOBBY_URL    the Oberon Cloud lobby (https://oberon-game.cloud.digitalmind.cz)
  PORT         HTTP port (8080)
"""

import json
import mimetypes
import os
import re
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

STATIC_DIR = Path(__file__).resolve().parent / "static"
GITHUB_REPO = os.environ.get("GITHUB_REPO", "jakubfrieb/dark-oberon")
LOBBY_URL = os.environ.get("LOBBY_URL", "https://oberon-game.cloud.digitalmind.cz").rstrip("/")
PORT = int(os.environ.get("PORT", "8080"))

RELEASE_TTL = 3600
LOBBY_TTL = 30
ASSET_RE = re.compile(r"^dark-oberon-[0-9][0-9A-Za-z.\-]*-(linux-x86_64\.tar\.gz|win64\.zip|haiku-x86_64\.zip)$")
PLATFORMS = {"linux-x86_64.tar.gz": "linux", "win64.zip": "windows", "haiku-x86_64.zip": "haiku"}
ORDER = ["linux", "windows", "haiku"]


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={
        "User-Agent": "dark-oberon-web",
        "Accept": "application/json",
    })
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.load(resp)


def load_release() -> dict:
    data = fetch_json(f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest")
    files = []
    for asset in data.get("assets", []):
        m = ASSET_RE.match(asset.get("name", ""))
        if m:
            files.append({
                "platform": PLATFORMS[m.group(1)],
                "name": asset["name"],
                "url": asset["browser_download_url"],
                "size": asset.get("size", 0),
            })
    files.sort(key=lambda f: ORDER.index(f["platform"]))
    return {
        "version": data.get("tag_name", "").lstrip("v"),
        "page": data.get("html_url", f"https://github.com/{GITHUB_REPO}/releases/latest"),
        "files": files,
    }


def load_lobby() -> dict:
    instances = [i for i in fetch_json(f"{LOBBY_URL}/api/instances").get("instances", []) if i.get("alive")]
    return {
        "games": len(instances),
        "players": sum(len(i.get("players", [])) for i in instances),
    }


class Cached:
    """Keeps the last good value of a slow upstream for `ttl` seconds."""

    def __init__(self, loader, ttl: int):
        self.loader = loader
        self.ttl = ttl
        self.value = None
        self.stamp = None  # monotonic() counts from boot, so 0.0 may still look fresh
        self.lock = threading.Lock()

    def get(self):
        with self.lock:
            if self.stamp is None or time.monotonic() - self.stamp >= self.ttl:
                try:
                    self.value = self.loader()
                except Exception as e:  # network, HTTP or JSON error: keep the last value
                    print(f"{self.loader.__name__}: {e}", flush=True)
                # Failed fetches also wait a full ttl, so a dead upstream is not hammered.
                self.stamp = time.monotonic()
            return self.value


release = Cached(load_release, RELEASE_TTL)
lobby = Cached(load_lobby, LOBBY_TTL)


class Handler(BaseHTTPRequestHandler):
    server_version = "dark-oberon-web"
    sys_version = ""

    def do_HEAD(self) -> None:
        self.do_GET(head=True)

    def do_GET(self, head: bool = False) -> None:
        path = self.path.split("?", 1)[0]
        if path == "/api/status":
            self._json({"release": release.get(), "lobby": lobby.get(), "lobby_url": LOBBY_URL}, head)
            return
        if path == "/en":
            self._redirect("/en/")
            return
        name = path.lstrip("/")
        if name == "" or name.endswith("/"):
            name += "index.html"
        target = (STATIC_DIR / name).resolve()
        if STATIC_DIR in target.parents and target.is_file():
            self._file(target, head)
        else:
            self._not_found(head)

    def _redirect(self, location: str) -> None:
        self.send_response(301)
        self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def _not_found(self, head: bool) -> None:
        body = b"Not found. The Dark Oberon page is at /"
        self.send_response(404)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if not head:
            self.wfile.write(body)

    def _json(self, data: dict, head: bool) -> None:
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if not head:
            self.wfile.write(body)

    def _file(self, target: Path, head: bool) -> None:
        try:
            f = target.open("rb")
        except OSError:
            self._not_found(head)
            return
        with f:
            size = os.fstat(f.fileno()).st_size
            ctype = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
            if ctype.startswith("text/") or ctype in ("application/javascript", "image/svg+xml"):
                ctype += "; charset=utf-8"
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(size))
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Cache-Control", "public, max-age=300")
            self.end_headers()
            if not head:
                while chunk := f.read(1 << 16):
                    self.wfile.write(chunk)

    def log_message(self, fmt: str, *args) -> None:
        pass  # quiet: Traefik keeps the access log


def main() -> None:
    mimetypes.add_type("image/webp", ".webp")
    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    server.daemon_threads = True
    print(f"dark-oberon web on :{PORT}, releases {GITHUB_REPO}, lobby {LOBBY_URL}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
