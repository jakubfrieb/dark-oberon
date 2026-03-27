# Multi-stage: build headless server on Ubuntu 22.04 (stable glibc), run Flask + server in one image.
FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ make pkg-config \
    libsdl2-dev libgl-dev libglu1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ src/
RUN make -C src server -j"$(nproc)"

FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip \
    libsdl2-2.0-0 libgl1 libglu1-mesa \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY server/web/ /app/web/
RUN pip3 install --no-cache-dir -r /app/web/requirements.txt \
    && mkdir -p /root/.dark-oberon/logs

COPY dat/ /app/dat/
COPY races/ /app/races/
COPY schemes/ /app/schemes/
COPY maps/ /app/maps/
COPY --from=builder /build/dark-oberon-server /app/dark-oberon-server

ENV DATA_DIR=/app
ENV SERVER_BINARY=/app/dark-oberon-server
ENV WEB_PORT=8080
ENV PORT_RANGE_START=17000
ENV PORT_RANGE_END=17009
# `-` = log access to stdout; `/dev/null` = quiet (bots still hit Traefik / public URL)
ENV GUNICORN_ACCESS_LOG=-

EXPOSE 8080
EXPOSE 17000/tcp 17001/tcp 17002/tcp 17003/tcp 17004/tcp 17005/tcp 17006/tcp 17007/tcp 17008/tcp 17009/tcp

# Single worker: in-memory InstanceManager must stay in one process.
CMD ["sh", "-c", "exec gunicorn --chdir /app/web --bind 0.0.0.0:${WEB_PORT:-8080} --workers 1 --threads 4 --timeout 120 --access-logfile ${GUNICORN_ACCESS_LOG:--} --error-logfile - app:app"]
