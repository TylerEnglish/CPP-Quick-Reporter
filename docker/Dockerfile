# ---------- build stage ----------
FROM debian:stable-slim AS build

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git curl ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# configure & build Release
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --config Release

# ---------- runtime stage ----------
FROM debian:stable-slim

# minimal runtime deps
RUN apt-get update && apt-get install -y ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# app layout
WORKDIR /app
COPY --from=build /src/build/csv_quick_report /usr/local/bin/csvqr
# stage assets/templates/config into image
COPY templates /app/templates
COPY config    /app/config

# ensure writable dirs for data/artifacts
RUN mkdir -p /app/data /app/artifacts

COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

