SOUND ?= 0
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

# Haiku build: compiled natively on a Haiku box over SSH (HAIKU_HOST, default `haiku`).
haiku:
	scripts/package-haiku.sh

windows-clean:
	$(MAKE) -C src -f Makefile.win clean
	rm -rf dist

test-ai:
	$(MAKE) -C tests/cpp

clean:
	cd src && make clean
