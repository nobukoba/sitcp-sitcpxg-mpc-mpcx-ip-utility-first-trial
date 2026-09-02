# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++ utilities for SiTCP and SiTCP-XG configuration over RBCP.

The repository is intended to contain:

- `mpc-mpcx-writer` — write MPC/MPCX data to EEPROM with automatic MPC/MPCX and target-type detection
- `mpc-mpcx-reader` — planned
- `sitcp-sitcpxg-ip-writer` — planned
- `sitcp-sitcpxg-ip-reader` — planned

## Quick start

```bash
git clone https://github.com/nobukoba/sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial.git
cd sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial
make
```

The executable is created in `bin/`:

```bash
./bin/mpc-mpcx-writer --help
```

## MPC / MPCX writer

```bash
./bin/mpc-mpcx-writer IP FILE
```

Examples:

```bash
./bin/mpc-mpcx-writer 192.168.2.161 2F20880E6E.mpc
./bin/mpc-mpcx-writer 192.168.2.169 2F20880E82.mpcx
```

The default RBCP port is `4660` and the default timeout is `3` seconds.

```bash
./bin/mpc-mpcx-writer 192.168.2.161 2F20880E6E.mpc --timeout 5
./bin/mpc-mpcx-writer 192.168.2.161 2F20880E6E.mpc --port 4660
```

The file type is detected from the 22-byte payload; the filename extension is not used for detection. The target is also checked as SiTCP or SiTCP-XG before programming. A mismatch is refused.

After writing, the EEPROM contents are read back and compared byte-for-byte. EEPROM write protection is re-enabled after programming.

## Build requirements

- C++17 compiler (`g++` or `clang++`)
- POSIX sockets
- `make`

The current implementation targets Linux, macOS, and WSL.

## Repository layout

```text
.
├── Makefile
├── README.md
└── src/
    └── mpc-mpcx-writer.cpp
```

## Notes

This is an experimental implementation and is not an official Bee Beans Technologies utility. Proprietary executables, libraries, and user-specific MPC/MPCX files are not included.
