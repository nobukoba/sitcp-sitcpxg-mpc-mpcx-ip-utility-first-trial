# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++17 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

- `mpc-mpcx-writer` — write MPC/MPCX data to EEPROM, automatically detect MPC/MPCX and target generation, then verify by read-back.
- `mpc-mpcx-reader` — read EEPROM, automatically detect SiTCP/SiTCP-XG, reconstruct MPC/MPCX information, and show the raw EEPROM image.
- `sitcp-sitcpxg-ip-reader` — read-only IP-oriented inspection. The initial implementation shares the EEPROM decoder with `mpc-mpcx-reader`; runtime-IP reporting will be expanded after register verification.
- `sitcp-sitcpxg-ip-writer` — command is present, but IP writes are deliberately disabled until EEPROM and current/runtime IP mappings have been verified on both SiTCP generations.

## Quick start

```bash
git clone https://github.com/nobukoba/sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial.git
cd sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial
make
make install
```

`make` creates all executables under `bin/`. The default `make install` installs them under this checkout:

```text
./install/bin/mpc-mpcx-writer
./install/bin/mpc-mpcx-reader
./install/bin/sitcp-sitcpxg-ip-reader
./install/bin/sitcp-sitcpxg-ip-writer
```

No root privileges are needed for the default installation.

## Install prefix

The default is:

```text
PREFIX=$(CURDIR)/install
```

For a user-local or system installation:

```bash
make install PREFIX=$HOME/.local
sudo make install PREFIX=/usr/local
```

`BINDIR` and `DESTDIR` may also be overridden. `make uninstall` accepts the same variables.

## MPC / MPCX writer

```bash
./bin/mpc-mpcx-writer IP FILE
```

Examples:

```bash
./bin/mpc-mpcx-writer 192.168.2.161 2F20880E6E.mpc
./bin/mpc-mpcx-writer 192.168.2.169 2F20880E82.mpcx
```

Default RBCP UDP port is `4660`; default timeout is `3` seconds:

```bash
./bin/mpc-mpcx-writer 192.168.2.161 file.mpc --timeout 5
./bin/mpc-mpcx-writer 192.168.2.161 file.mpc --port 4660
```

The 22-byte payload is classified by content; the filename extension is not used. The target is checked as normal SiTCP or SiTCP-XG before programming. A mismatch is refused. EEPROM write protection is restored and the programmed bytes are verified by read-back.

## MPC / MPCX reader

```bash
./bin/mpc-mpcx-reader 192.168.2.161
```

Options:

```bash
./bin/mpc-mpcx-reader 192.168.2.161 --port 4660 --timeout 3
```

The reader is non-destructive. It reads EEPROM in small chunks, detects the target generation, reconstructs the corresponding 22-byte payload, displays relevant fields, and dumps FC00..FC4F.

## SiTCP / SiTCP-XG IP reader

```bash
./bin/sitcp-sitcpxg-ip-reader 192.168.2.161
```

This is currently read-only. The initial version reports EEPROM-side information through the same verified decoder used by the MPC/MPCX reader. Reading and clearly separating both the current/runtime IP and EEPROM/default IP is the next hardware-verification step.

## SiTCP / SiTCP-XG IP writer

The intended interface is:

```text
sitcp-sitcpxg-ip-writer IP NEW_IP [options]
```

The final design will use EEPROM as the default write destination and provide an explicit option for changing the current/runtime IP. Read-back verification is required; after a runtime IP change the utility should reconnect to the new address and verify it when the device behavior permits.

**Destructive IP writing is currently disabled.** The executable intentionally refuses writes until the exact EEPROM and runtime register mappings have been independently verified for normal SiTCP and SiTCP-XG. See `REVERSE_ENGINEERING.md`.

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
    ├── mpc-mpcx-reader.cpp
    ├── mpc-mpcx-writer.cpp
    ├── sitcp-sitcpxg-ip-reader.cpp
    └── sitcp-sitcpxg-ip-writer.cpp
```

`FOR_DEVELOPERS.md` describes architecture, implementation status, testing, and planned refactoring. `REVERSE_ENGINEERING.md` records the evidence behind the reconstructed MPC/MPCX and EEPROM behavior. `AGENTS.md` records constraints that future development should preserve.

## Notes

This is an experimental implementation and is not an official Bee Beans Technologies utility. Proprietary executables, libraries, and user-specific MPC/MPCX files are not included.
