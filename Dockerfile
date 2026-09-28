FROM debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --parallel

FROM gcr.io/distroless/cc-debian12:nonroot

WORKDIR /app

COPY --chown=65532:65532 --from=builder /src/build/TcgServer /app/TcgServer

COPY --chown=65532:65532 config.ini /app/config/config.ini

EXPOSE 8080

ENTRYPOINT ["/app/TcgServer"]