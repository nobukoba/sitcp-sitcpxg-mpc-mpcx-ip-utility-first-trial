CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic

BIN_DIR := bin
SRC_DIR := src

MPC_MPCX_WRITER := $(BIN_DIR)/mpc-mpcx-writer

all: $(MPC_MPCX_WRITER)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(MPC_MPCX_WRITER): $(SRC_DIR)/mpc-mpcx-writer.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
