CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic

PREFIX ?= $(CURDIR)/install
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

BIN_DIR := bin
SRC_DIR := src

MPC_MPCX_WRITER := $(BIN_DIR)/mpc-mpcx-writer

all: $(MPC_MPCX_WRITER)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(MPC_MPCX_WRITER): $(SRC_DIR)/mpc-mpcx-writer.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(MPC_MPCX_WRITER) $(DESTDIR)$(BINDIR)/mpc-mpcx-writer

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mpc-mpcx-writer

clean:
	rm -rf $(BIN_DIR)

.PHONY: all install uninstall clean
