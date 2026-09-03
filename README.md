# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++17 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

The public command set is now unified into three commands:

- `mpc-mpcx-ip-writer` — write MPC/MPCX EEPROM data and optionally change the IP address.
- `mpc-mpcx-ip-reader` — read MPC/MPCX information and always display current/EEPROM MAC and IP addresses.
- `mpc-mpcx-ip-command` — advanced MPC/MPCX, IP, and low-level RBCP operations.

The former `mpc-mpcx-writer`, `mpc-mpcx-reader`, and `mpc-mpcx-command` implementations remain in `src/` as internal legacy implementation units, but they are no longer installed as public commands.

## Quick start

```bash
git clone https://github.com/nobukoba/sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial.git
cd sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial
make
make install
```

Default installation directory:

```text
./install/bin/
```

Installed commands:

```text
./install/bin/mpc-mpcx-ip-writer
./install/bin/mpc-mpcx-ip-reader
./install/bin/mpc-mpcx-ip-command
```

Default RBCP UDP port is `4660`; default timeout is `3` seconds. These defaults are also shown by `--help`.

## Reader

```bash
./bin/mpc-mpcx-ip-reader 192.168.2.161
```

The reader always reports:

```text
current MAC
current IP
EEPROM MAC
EEPROM IP
```

and then reads/decodes the MPC/MPCX EEPROM information.

## Writer

Write only MPC/MPCX information:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx
```

Change only the EEPROM IP address:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 --set-ip 192.168.2.170
```

Write MPC/MPCX information and also change the EEPROM IP:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx --set-ip 192.168.2.170
```

Change the current/runtime IP instead of EEPROM:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 --set-ip 192.168.2.170 --current
```

EEPROM is the default IP destination. `--eeprom` can be given explicitly. For a current/runtime IP change, the writer does not blindly retry a timed-out destructive write; it reconnects to `NEW_IP` and verifies the current IP there.

The writer displays current/EEPROM MAC and IP values before and after the operation. MPC/MPCX payload type is determined from the 22-byte contents, not the filename extension.

## Advanced command

```bash
./bin/mpc-mpcx-ip-command --help
```

Important subcommands include:

```text
inspect FILE
read IP [--port N] [--timeout SEC]
verify IP FILE [--port N] [--timeout SEC]
mpcx-plan IP FILE [--port N] [--timeout SEC]
probe IP ADDRESS [LENGTH] [--port N] [--timeout SEC]
rbcp-read IP ADDRESS LENGTH [--port N] [--timeout SEC]
rbcp-write IP ADDRESS HEX-BYTES [--port N] [--timeout SEC]
clear IP --yes-really-clear [--port N] [--timeout SEC]
ip-read IP [--port N] [--timeout SEC]
ip-write CURRENT_IP NEW_IP [--eeprom|--current] [--port N] [--timeout SEC]
```

`read` also displays current/EEPROM MAC and IP information. `ip-read` provides only the network configuration view. `ip-write` defaults to EEPROM and accepts `--current` for the runtime/current address.

## Build requirements

- C++17 compiler (`g++` or `clang++`)
- POSIX sockets
- `make`

Targets are Linux, macOS, and WSL.

## Implementation notes

The public unified commands use shared IP configuration code in `src/ip-config.hpp`. MPC/MPCX payload handling and IP register handling remain logically separated internally even though they are exposed through the same command-line utilities.

IP/MAC register addresses used by the implementation are:

```text
current MAC : 0xFFFFFF12..0xFFFFFF17
current IP  : 0xFFFFFF18..0xFFFFFF1B
EEPROM MAC  : 0xFFFFFC12..0xFFFFFC17
EEPROM IP   : 0xFFFFFC18..0xFFFFFC1B
EEPROM WE   : 0xFFFFFCFF
```

This is an experimental implementation and is not an official Bee Beans Technologies utility. Proprietary executables, libraries, and user-specific MPC/MPCX files are not included.
