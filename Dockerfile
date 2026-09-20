# ── Build stage ──
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make \
    libsqlite3-dev \
    nlohmann-json3-dev \
    libcurl4-openssl-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ── Runtime stage ──
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/freshmate_server .
COPY --from=builder /app/public ./public

ENV PORT=8080
ENV DB_PATH=/data/freshmate.db

RUN mkdir -p /data

EXPOSE 8080
CMD ["./freshmate_server"]
