FROM alpine:latest AS builder

RUN apk update && \
    apk add --no-cache git cmake build-base openssh

RUN mkdir -p /root/.ssh && \
    chmod 0700 /root/.ssh && \
    ssh-keyscan github.com >> /root/.ssh/known_hosts

COPY ssh_config /root/.ssh/config

RUN chmod 0600 /root/.ssh/config

RUN --mount=type=ssh git clone git@github.com:cielmobilityDev/alns-pdp.git /alns-pdp && \
    cd /alns-pdp && \
    cmake -S . -B build && \
    cmake --build build && \
    cmake --install build

RUN --mount=type=ssh git clone git@github.com:cielmobilityDev/lnsModRoute.git /lnsModRoute && \
    cd /lnsModRoute && \
    cmake -S . -B build && \
    cmake --build build && \
    cmake --install build

FROM alpine:latest AS runstage

RUN apk update && \
    apk add --no-cache libstdc++ libgcc

COPY --from=builder /usr/local /usr/local

RUN apk update && \
    ldconfig /usr/local/lib

CMD ["ash"]
