CXX ?= g++
SRC = ../../src

SDL2_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL2_LIBS := $(shell pkg-config --libs sdl2)

test: test_ai_logic test_dofile
	./test_ai_logic
	./test_dofile

test_ai_logic: test_ai_logic.cpp $(SRC)/doai_logic.cpp $(SRC)/doai_logic.h
	$(CXX) -std=gnu++17 -Wall -Wextra -O1 -I$(SRC) -o $@ test_ai_logic.cpp $(SRC)/doai_logic.cpp

# Config-file parser, built with ASan so a buffer overflow fails the test.
test_dofile: test_dofile.cpp $(SRC)/dofile.cpp $(SRC)/dofile.h
	$(CXX) -std=gnu++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
	  -w -DUNIX=1 -DSOUND=0 -DDEBUG=0 -DHEADLESS=0 -I$(SRC) $(SDL2_CFLAGS) \
	  -o $@ test_dofile.cpp $(SRC)/dofile.cpp $(SDL2_LIBS)

clean:
	rm -f test_ai_logic test_dofile

.PHONY: test clean
