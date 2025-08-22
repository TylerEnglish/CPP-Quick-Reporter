# syntax=docker/dockerfile:1

# ---- builder (not used yet; placeholder) ----
FROM debian:bookworm-slim AS builder
# (Later: install build-essential cmake ninja, fetch deps, build in Release)

# ---- runner ----
FROM debian:bookworm-slim
# (Later: copy /app binaries + templates + config)
WORKDIR /app
# ENTRYPOINT ["./csv_quick_report"]
