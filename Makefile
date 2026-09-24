# Sound (SDL2_mixer) is on by default; build without it with `make SOUND=0`.
SOUND ?= 1
# Graphical client must stay HEADLESS=0 so Editor() and menu code stay in doengine.o.
HEADLESS ?= 0

build:
	cd src && make SOUND=$(SOUND) HEADLESS=$(HEADLESS)

# Windows cross-build (MinGW-w64). One-time setup:
#   sudo scripts/setup-windows-toolchain.sh && scripts/fetch-windows-deps.sh
windows:
	$(MAKE) -C src build_info.h
	$(MAKE) -C src -f Makefile.win
	scripts/package-windows.sh

# All release packages at once; Windows (obj-win/), Haiku (VM) and Linux (Docker)
# build in separate places, so they run in parallel.
dist:
	$(MAKE) -C src build_info.h
	$(MAKE) -j3 windows haiku linux-dist

# Portable Linux release: built in an Ubuntu 22.04 container (needs Docker).
linux-dist:
	scripts/package-linux.sh

# Haiku build: compiled natively on a Haiku box over SSH (HAIKU_HOST, default `haiku`).
haiku:
	scripts/package-haiku.sh

windows-clean:
	$(MAKE) -C src -f Makefile.win clean
	rm -rf dist/dark-oberon-*-win64*

test-ai:
	$(MAKE) -C tests/cpp

clean:
	cd src && make clean

.PHONY: build windows haiku linux-dist dist windows-clean test-ai clean
