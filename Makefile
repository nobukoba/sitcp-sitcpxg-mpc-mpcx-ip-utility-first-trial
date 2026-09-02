CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic

PREFIX ?= $(CURDIR)/install
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

BIN_DIR := bin
SRC_DIR := src
PROGRAMS := mpc-mpcx-writer mpc-mpcx-reader mpc-mpcx-command sitcp-sitcpxg-ip-reader sitcp-sitcpxg-ip-writer
TARGETS := $(addprefix $(BIN_DIR)/,$(PROGRAMS))

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

install: all
	install -d $(DESTDIR)$(BINDIR)
	for program in $(PROGRAMS); do install -m 0755 $(BIN_DIR)/$$program $(DESTDIR)$(BINDIR)/$$program; done

uninstall:
	for program in $(PROGRAMS); do rm -f $(DESTDIR)$(BINDIR)/$$program; done

clean:
	rm -rf $(BIN_DIR)

.PHONY: all install uninstall clean
