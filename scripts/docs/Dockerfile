# The docs website, for Railway (railway.json): built from the markdown, then served as static files.
FROM python:3.12-slim AS build
WORKDIR /repo
RUN pip install --no-cache-dir markdown==3.7 pygments==2.18.0
COPY . .
RUN python3 scripts/docs/build.py

FROM caddy:2.8.4-alpine
COPY --from=build /repo/website /srv
CMD caddy file-server --root /srv --listen :${PORT:-8080}
