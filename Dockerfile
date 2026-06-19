# ─── Build stage ────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential       \
    cmake                 \
    git                   \
    libcurl4-openssl-dev  \
    libboost-all-dev      \
    libssl-dev            \
    zlib1g-dev            \
    ca-certificates       \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source first (layer-cache CMakeLists separately for faster rebuilds)
COPY CMakeLists.txt .
COPY src/            src/
COPY include/        include/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel "$(nproc)"

# ─── Runtime stage ───────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libcurl4              \
    libboost-system1.74.0 \
    ca-certificates       \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/gemini_monitor ./gemini_monitor

# Non-root user for security
RUN useradd -r -s /bin/false appuser && chown appuser /app
USER appuser

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/api/health || exit 1

CMD ["./gemini_monitor"]
