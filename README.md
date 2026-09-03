# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++17 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

The public command set is unified into three commands:

- `mpc-mpcx-ip-writer` — write MPC/MPCX EEPROM data and optionally change EEPROM/current IP addresses.
- `mpc-mpcx-ip-reader` — read MPC/MPCX information and always display current/EEPROM MAC and IP addresses.
- `mpc-mpcx-ip-command` — advanced MPC/MPCX, IP, and low-level RBCP operations.

The former `mpc-mpcx-writer`, `mpc-mpcx-reader`, and `mpc-mpcx-command` implementations remain in `src/` as internal implementation units, but they are no longer installed as public commands.

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

The MPC/MPCX file is a required positional argument:

```text
mpc-mpcx-ip-writer CURRENT_IP MPC_OR_MPCX_FILE [options]
```

Write MPC/MPCX information only:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx
```

Write MPC/MPCX information and also set the EEPROM/default IP:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-eeprom-ip 192.168.2.170
```

Write MPC/MPCX information and also set the current/runtime IP:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-current-ip 192.168.2.170
```

Set both EEPROM/default and current/runtime IP addresses:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-eeprom-ip 192.168.2.170 \
  --set-current-ip 192.168.2.170
```

Writer options:

```text
--set-eeprom-ip IP   Set EEPROM/default IP address
--set-current-ip IP  Set current/runtime IP address
--port N             RBCP UDP port (default: 4660)
--timeout SEC        RBCP timeout in seconds (default: 3)
-h, --help           Show help
```

When both IP options are given, EEPROM IP is written first and current/runtime IP is changed last. This keeps the original address reachable until all operations that require it have finished. After a current/runtime IP change, the writer reconnects to the new IP and performs read-back verification. It does not blindly retry a timed-out destructive current-IP write because the address may already have changed before the acknowledgement is received.

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

## Source formatting

Source files should use conventional readable C++ formatting. Avoid compressed one-line implementations; put control-flow blocks and logically separate statements on separate lines. Long expressions should be wrapped rather than packed into a single line.

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
