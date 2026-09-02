# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++17 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

- `mpc-mpcx-writer` — write MPC/MPCX data to EEPROM, automatically detect MPC/MPCX and target generation, then verify by read-back.
- `mpc-mpcx-reader` — read EEPROM, automatically detect SiTCP/SiTCP-XG, reconstruct MPC/MPCX information, and show the raw EEPROM image.
- `mpc-mpcx-command` — advanced inspection, verification, planning, low-level RBCP access, and destructive clear operations.
- `sitcp-sitcpxg-ip-reader` — read-only IP-oriented inspection. The initial implementation shares the EEPROM decoder with `mpc-mpcx-reader`; runtime-IP reporting will be expanded after register verification.
- `sitcp-sitcpxg-ip-writer` — command is present, but IP writes are deliberately disabled until EEPROM and current/runtime IP mappings have been verified on both SiTCP generations.

## Quick start

```bash
git clone https://github.com/nobukoba/sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial.git
cd sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial
make
make install
```

The default `make install` installs all five commands under:

```text
./install/bin/
```

including:

```text
./install/bin/mpc-mpcx-writer
./install/bin/mpc-mpcx-reader
./install/bin/mpc-mpcx-command
./install/bin/sitcp-sitcpxg-ip-reader
./install/bin/sitcp-sitcpxg-ip-writer
```

The default prefix is `$(CURDIR)/install`. Override it with, for example:

```bash
make install PREFIX=$HOME/.local
sudo make install PREFIX=/usr/local
```

## MPC / MPCX writer

```bash
./bin/mpc-mpcx-writer IP FILE
```

Default RBCP UDP port is `4660`; default timeout is `3` seconds. The 22-byte payload is classified by content, not filename extension. Target generation is checked before programming, write protection is restored, and programmed bytes are verified by read-back.

## MPC / MPCX reader

```bash
./bin/mpc-mpcx-reader 192.168.2.161
```

The reader is non-destructive and reads/decode the EEPROM image.

## MPC / MPCX command

```bash
./bin/mpc-mpcx-command --help
```

Available subcommands are:

```text
inspect FILE
read IP [--port N] [--timeout SEC]
verify IP FILE [--port N] [--timeout SEC]
mpcx-plan IP FILE [--port N] [--timeout SEC]
probe IP ADDRESS [LENGTH] [--port N] [--timeout SEC]
rbcp-read IP ADDRESS LENGTH [--port N] [--timeout SEC]
rbcp-write IP ADDRESS HEX-BYTES [--port N] [--timeout SEC]
clear IP --yes-really-clear [--port N] [--timeout SEC]
write IP FILE ...
```

`write` intentionally directs users to the verified `mpc-mpcx-writer` path instead of duplicating the high-level destructive programming implementation. `clear` and `rbcp-write` are destructive low-level operations and should be used carefully.

Examples:

```bash
./bin/mpc-mpcx-command inspect file.mpcx
./bin/mpc-mpcx-command verify 192.168.2.169 file.mpcx
./bin/mpc-mpcx-command probe 192.168.2.169 0xFFFFFF50 1
./bin/mpc-mpcx-command rbcp-read 192.168.2.169 0xFFFFFC00 8
```

## SiTCP / SiTCP-XG IP reader

```bash
./bin/sitcp-sitcpxg-ip-reader 192.168.2.161
```

This is currently read-only. Reading and clearly separating current/runtime IP and EEPROM/default IP is the next hardware-verification step.

## SiTCP / SiTCP-XG IP writer

The intended interface is:

```text
sitcp-sitcpxg-ip-writer IP NEW_IP [options]
```

EEPROM will be the default destination, with an explicit option for current/runtime IP changes. Destructive IP writing is currently disabled until mappings are verified on both generations.

## Build requirements

- C++17 compiler (`g++` or `clang++`)
- POSIX sockets
- `make`

Current targets are Linux, macOS, and WSL.

## Repository layout

```text
.
├── AGENTS.md
├── FOR_DEVELOPERS.md
├── Makefile
├── README.md
├── REVERSE_ENGINEERING.md
└── src/
    ├── mpc-mpcx-command.cpp
    ├── mpc-mpcx-reader.cpp
    ├── mpc-mpcx-writer.cpp
    ├── sitcp-sitcpxg-ip-reader.cpp
    └── sitcp-sitcpxg-ip-writer.cpp
```

## Notes

This is an experimental implementation and is not an official Bee Beans Technologies utility. Proprietary executables, libraries, and user-specific MPC/MPCX files are not included.
