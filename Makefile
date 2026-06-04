.PHONY: all clean test engine server install uninstall examples linux-release deploy-vps

REMOTE ?= root@135.181.201.184
PODMAN ?= podman
LINUX_IMAGE ?= ubuntu:24.04
LINUX_PLATFORM ?= linux/amd64
LINUX_BIN := release/truco_server
LINUX_IMAGE_TAG := truco-linux-build
LINUX_EXTRACT := truco-linux-extract

all: engine server

engine:
	$(MAKE) -C engine

server:
	$(MAKE) -C server

test: engine-test server-test

engine-test:
	$(MAKE) -C engine test

server-test:
	$(MAKE) -C server test

examples:
	$(MAKE) -C engine examples

install:
	$(MAKE) -C engine install

uninstall:
	$(MAKE) -C engine uninstall

clean:
	$(MAKE) -C engine clean
	$(MAKE) -C server clean

linux-release:
	mkdir -p release
	$(PODMAN) build --platform $(LINUX_PLATFORM) -t $(LINUX_IMAGE_TAG) -f Containerfile .
	-$(PODMAN) rm -f $(LINUX_EXTRACT) 2>/dev/null
	$(PODMAN) create --name $(LINUX_EXTRACT) $(LINUX_IMAGE_TAG)
	$(PODMAN) cp $(LINUX_EXTRACT):/src/server/build/truco_server $(LINUX_BIN)
	$(PODMAN) rm -f $(LINUX_EXTRACT)
	chmod +x $(LINUX_BIN)
	file $(LINUX_BIN)

deploy-vps:
	test -f $(LINUX_BIN)
	scp $(LINUX_BIN) $(REMOTE):truco_server
