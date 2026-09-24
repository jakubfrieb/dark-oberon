SOUND ?= 0
# Graphical client must stay HEADLESS=0 so Editor() and menu code stay in doengine.o.
HEADLESS ?= 0

build:
	cd src && make SOUND=$(SOUND) HEADLESS=$(HEADLESS)

test-ai:
	$(MAKE) -C tests/cpp

clean:
	cd src && make clean
