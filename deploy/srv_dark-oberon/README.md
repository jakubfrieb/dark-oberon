# Dark Oberon – product page (DigiCloud)

Project `srv_dark-oberon` in `/server/projects/srv_dark-oberon/`: the game's page at
https://oberon.cloud.digitalmind.cz (Czech at `/`, English at `/en/`) behind Traefik.

The lobby and the game servers are a separate stack (`app_dark-oberon`) at
https://oberon-game.cloud.digitalmind.cz. This page only links to it and shows how many
games run there.

```
.
├── bin/            logs, shell
├── docker/         docker-compose.yml, Dockerfile, .env.example
├── src/            server.py (Python standard library only) + static/ (the page)
├── up / down / status
└── README.md
```

`server.py` serves the static page and `/api/status`: the newest GitHub release
(version and download links per platform, cached for an hour) and the live game
count from the lobby's `/api/instances` (cached for 30 s). When either fails the page
keeps its static links to GitHub Releases. Downloads come straight from GitHub,
nothing is stored on the server.

## Deploy from the development machine

```bash
rsync -a --delete --exclude docker/.env deploy/srv_dark-oberon/ lolbit:/server/projects/srv_dark-oberon/
ssh lolbit /server/projects/srv_dark-oberon/up
```

`./up` creates `docker/.env` from `.env.example` on the first run.

## Environment (`docker/.env`)

| Variable | Default | Meaning |
|---|---|---|
| `COMPOSE_PROJECT_NAME` | `srv_dark-oberon` | container/image names and the Traefik router |
| `PROJECT_WEBSITE` | `oberon.cloud.digitalmind.cz` | host name (DNS points to the VPS) |
| `LOBBY_URL` | `https://oberon-game.cloud.digitalmind.cz` | lobby for the live game count |
| `GITHUB_REPO` | `jakubfrieb/dark-oberon` | repository with the releases |

## Run locally

```bash
PORT=8080 python3 deploy/srv_dark-oberon/src/server.py   # http://localhost:8080/
```

Traffic is measured by the self-hosted Plausible (site `oberon.cloud.digitalmind.cz`).
