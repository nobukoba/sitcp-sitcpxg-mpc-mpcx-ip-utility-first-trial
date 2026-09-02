# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++17 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

- `mpc-mpcx-writer` — write MPC/MPCX data to EEPROM, automatically detect MPC/MPCX and target generation, then verify by read-back.
- `mpc-mpcx-reader` — read EEPROM, automatically detect SiTCP/SiTCP-XG, reconstruct MPC/MPCX information, and show the raw EEPROM image.
- `mpc-mpcx-command` — advanced MPC/MPCX inspection, verification, planning, low-level RBCP access, and destructive clear operations.
- `sitcp-sitcpxg-ip-reader` — SiTCP Utility compatible IP-only reader. This is a separate tool from MPC/MPCX handling.
- `sitcp-sitcpxg-ip-writer` — SiTCP Utility compatible IP-only writer. This is a separate tool from MPC/MPCX handling.

The IP reader/writer do **not** read, reconstruct, validate, or modify MPC/MPCX license payloads. Their scope is only the SiTCP/SiTCP-XG IP address configuration.

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

The reader is non-destructive and reads/decodes the MPC/MPCX EEPROM image.

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

## SiTCP / SiTCP-XG IP reader

This is an **IP-only utility**, corresponding to the IP-address function of the SiTCP Utility. It is independent of MPC/MPCX license handling.

Intended behavior:

```bash
./bin/sitcp-sitcpxg-ip-reader CURRENT_IP
```

It should report the current/runtime IP and the EEPROM/default IP as appropriate for SiTCP/SiTCP-XG. The current placeholder intentionally performs no MPC/MPCX decoding while the exact SiTCP Utility compatible access method is being verified.

## SiTCP / SiTCP-XG IP writer

This is also an **IP-only utility**. It changes only the SiTCP/SiTCP-XG IP address configuration and must not touch MPC/MPCX license information.

Intended behavior:

```text
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

EEPROM is intended to be the default destination. An explicit option will be used for changing the current/runtime IP. After changing the runtime IP, the utility should reconnect to the new IP and perform read-back verification when supported by the device.

Write support is currently disabled until the exact SiTCP Utility compatible IP access method has been verified for both SiTCP and SiTCP-XG.

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
