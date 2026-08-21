# DS4 Prometheus and Grafana

Docker Compose deployment for the three-node DS4 fleet. Prometheus scrapes
`GET /metrics` every 15 seconds from:

- `ds4-156` — `192.168.5.156:8000` (CUDA/aarch64)
- `ds4-157` — `192.168.5.157:8000` (ROCm/x86_64)
- `ds4-158` — `192.168.5.158:8000` (CUDA/aarch64)

DS4 servers must be started with `--metrics`. The endpoint remains unavailable
otherwise.

## Start

```sh
cp .env.example .env
# Replace the example password and restrict the file.
chmod 600 .env
docker compose config --quiet
docker compose pull
docker compose up -d
```

The compose project binds to the monitoring host's LAN address:

- Prometheus: `http://192.168.5.161:9090`
- Grafana: `http://192.168.5.161:3000`

Grafana is provisioned with the Prometheus datasource and the **DS4 Serving
Overview** dashboard. Anonymous access and account signup are disabled.
Prometheus retains data for up to 30 days or 50 GB, whichever limit is reached.
Named Docker volumes preserve both TSDB and Grafana state across container
replacement.

## Validate

```sh
curl -fsS http://192.168.5.161:9090/-/ready
curl -fsS http://192.168.5.161:9090/api/v1/targets
curl -fsS http://192.168.5.161:3000/api/health
```

Use `docker compose down` to stop containers without deleting data. Do not add
`--volumes` unless permanent deletion of Prometheus and Grafana state is
intended.
