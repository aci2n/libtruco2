FROM ubuntu:24.04

RUN apt-get update -qq \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends build-essential \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN make DEBUG=0 -C engine clean all \
    && make DEBUG=0 LDFLAGS=-static -C server clean all
