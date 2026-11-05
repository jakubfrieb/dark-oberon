# Dark Oberon web (product page). Context = project root, this file at
# docker/Dockerfile. Python standard library only.
FROM python:3.13-alpine
RUN adduser -D -u 1000 web
WORKDIR /app
COPY src/ /app/
USER web
ENV PORT=8080 PYTHONUNBUFFERED=1
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --start-interval=2s CMD wget -qO- http://127.0.0.1:8080/ > /dev/null || exit 1
CMD ["python3", "/app/server.py"]
