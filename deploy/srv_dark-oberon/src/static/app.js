// Fills the page from /api/status: the newest release's download links and the
// live game count. Without it the static links to GitHub Releases stay in place.
(function () {
  const cs = document.documentElement.lang === "cs";
  const T = cs ? {
    version: (v) => `Verze ${v}`,
    games: (n) => n === 1 ? "Právě běží 1 hra" : n >= 2 && n <= 4 ? `Právě běží ${n} hry` : `Právě běží ${n} her`,
    idle: "Lobby je otevřené, založ první hru",
    download: { linux: "Stáhnout pro Linux", windows: "Stáhnout pro Windows", haiku: "Stáhnout pro Haiku" },
  } : {
    version: (v) => `Version ${v}`,
    games: (n) => n === 1 ? "1 game running right now" : `${n} games running right now`,
    idle: "The lobby is open, start the first game",
    download: { linux: "Download for Linux", windows: "Download for Windows", haiku: "Download for Haiku" },
  };

  const ua = navigator.userAgent;
  const platform = /Haiku/i.test(ua) ? "haiku" : /Windows/i.test(ua) ? "windows" : /Linux|X11/i.test(ua) && !/Android/i.test(ua) ? "linux" : null;

  const mib = (bytes) => (bytes / 1048576).toFixed(0) + " MB";

  fetch("/api/status").then((r) => r.ok ? r.json() : null).then((s) => {
    if (!s) return;
    const rel = s.release;
    if (rel && rel.files.length) {
      document.querySelectorAll("[data-version]").forEach((el) => { el.textContent = T.version(rel.version); });
      for (const f of rel.files) {
        const a = document.querySelector(`.get[data-platform="${f.platform}"]`);
        if (a) {
          a.href = f.url;
          a.querySelector("small").textContent = ` ${f.name.replace(/^dark-oberon-/, "")}, ${mib(f.size)}`;
        }
        if (f.platform === platform) {
          const main = document.getElementById("dl-main");
          main.href = f.url;
          main.textContent = T.download[platform];
        }
      }
    }
    const live = document.getElementById("live");
    if (s.lobby && live) {
      live.textContent = s.lobby.games > 0 ? T.games(s.lobby.games) : T.idle;
      live.hidden = false;
    }
  }).catch(() => {});
})();
