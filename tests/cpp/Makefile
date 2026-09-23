CXX ?= g++
SRC = ../../src

test: test_ai_logic
	./test_ai_logic

test_ai_logic: test_ai_logic.cpp $(SRC)/doai_logic.cpp $(SRC)/doai_logic.h
	$(CXX) -std=gnu++17 -Wall -Wextra -O1 -I$(SRC) -o $@ test_ai_logic.cpp $(SRC)/doai_logic.cpp

clean:
	rm -f test_ai_logic

.PHONY: test clean
