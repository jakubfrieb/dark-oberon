"""Lobby accounts: SQLite user store and a small in-memory rate limiter."""
from __future__ import annotations

import re
import sqlite3
import threading
import time
from collections import defaultdict, deque
from pathlib import Path

from werkzeug.security import check_password_hash, generate_password_hash

USERNAME_RE = re.compile(r"^[A-Za-z0-9_-]{3,20}$")
PASSWORD_MIN = 8
PASSWORD_MAX = 128


class AccountError(ValueError):
    """User-facing validation error (message is shown in the form)."""


class UserStore:
    def __init__(self, db_path: Path):
        db_path.parent.mkdir(parents=True, exist_ok=True)
        self._path = str(db_path)
        self._lock = threading.Lock()
        with self._connect() as db:
            db.execute(
                "CREATE TABLE IF NOT EXISTS users ("
                " id INTEGER PRIMARY KEY,"
                " username TEXT NOT NULL UNIQUE COLLATE NOCASE,"
                " password_hash TEXT NOT NULL,"
                " created_at INTEGER NOT NULL)"
            )

    def _connect(self) -> sqlite3.Connection:
        return sqlite3.connect(self._path, timeout=10)

    def create(self, username: str, password: str, password2: str) -> str:
        username = (username or "").strip()
        if not USERNAME_RE.match(username):
            raise AccountError("Username must be 3–20 letters, digits, _ or -.")
        if len(password or "") < PASSWORD_MIN:
            raise AccountError(f"Password must be at least {PASSWORD_MIN} characters.")
        if len(password) > PASSWORD_MAX:
            raise AccountError(f"Password must be at most {PASSWORD_MAX} characters.")
        if password != password2:
            raise AccountError("Passwords do not match.")
        with self._lock, self._connect() as db:
            try:
                db.execute(
                    "INSERT INTO users (username, password_hash, created_at) VALUES (?, ?, ?)",
                    (username, generate_password_hash(password), int(time.time())),
                )
            except sqlite3.IntegrityError:
                raise AccountError("That username is already taken.") from None
        return username

    def authenticate(self, username: str, password: str) -> str | None:
        """Return the stored username (canonical case) or None."""
        with self._connect() as db:
            row = db.execute(
                "SELECT username, password_hash FROM users WHERE username = ?",
                ((username or "").strip(),),
            ).fetchone()
        if row and check_password_hash(row[1], password or ""):
            return row[0]
        return None


class RateLimiter:
    """At most `limit` hits per `window` seconds per key (e.g. client IP)."""

    def __init__(self, limit: int, window: float):
        self.limit = limit
        self.window = window
        self._hits: dict[str, deque] = defaultdict(deque)
        self._lock = threading.Lock()

    def _trim(self, q: deque, now: float) -> None:
        while q and now - q[0] > self.window:
            q.popleft()

    def blocked(self, key: str) -> bool:
        now = time.monotonic()
        with self._lock:
            q = self._hits[key]
            self._trim(q, now)
            return len(q) >= self.limit

    def hit(self, key: str) -> None:
        now = time.monotonic()
        with self._lock:
            q = self._hits[key]
            self._trim(q, now)
            q.append(now)
