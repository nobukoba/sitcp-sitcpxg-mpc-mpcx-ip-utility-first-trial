# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++11 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

The installed command set contains five commands:

- `mpc-mpcx-ip-writer` — write MPC/MPCX EEPROM data and optionally change EEPROM/current IP addresses.
- `mpc-mpcx-ip-reader` — read MPC/MPCX information and always display current/EEPROM MAC and IP addresses.
- `mpc-mpcx-ip-command` — advanced MPC/MPCX, IP, and low-level RBCP operations.
- `sitcp-sitcpxg-ip-writer` — IP-only writer for SiTCP / SiTCP-XG.
- `sitcp-sitcpxg-ip-reader` — IP-only reader for SiTCP / SiTCP-XG.

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
./install/bin/sitcp-sitcpxg-ip-writer
./install/bin/sitcp-sitcpxg-ip-reader
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

The reader determines the device generation first from the documented SiTCPXG Identifier register at `0xFFFFFF08..0xFFFFFF0B`. An exact value of `0x58544350` identifies SiTCP-XG. MPC/MPCX payload classification is handled separately and is not used to determine the device generation.

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

## IP-only commands

Use these when only SiTCP / SiTCP-XG IP configuration is needed and no MPC/MPCX file should be involved:

```bash
./bin/sitcp-sitcpxg-ip-reader 192.168.2.161
./bin/sitcp-sitcpxg-ip-writer 192.168.2.161 192.168.2.170
```

The IP-only commands share the low-level IP register helper but do not read or rewrite MPC/MPCX payload data.

## Advanced command

```bash
./bin/mpc-mpcx-ip-command --help
```

Important subcommands include (MPC_OR_MPCX_FILE means a `.mpc` or `.mpcx` license/configuration file):

```text
inspect MPC_OR_MPCX_FILE
mac MPC_OR_MPCX_FILE
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

- C++11 compiler (`g++` or `clang++`)
- POSIX sockets
- `make`

The default build uses `-std=c++11`. Targets are Linux, macOS, and WSL.

## Source formatting

Source files should use conventional readable C++ formatting. Avoid compressed one-line implementations; put control-flow blocks and logically separate statements on separate lines. Long expressions should be wrapped rather than packed into a single line.

## Implementation notes

The public commands use shared IP configuration code in `src/ip-config.hpp`. MPC/MPCX payload handling and IP register handling remain logically separated internally even though some commands expose both functions.

IP/MAC register addresses used by the implementation are:

```text
current MAC : 0xFFFFFF12..0xFFFFFF17
current IP  : 0xFFFFFF18..0xFFFFFF1B
EEPROM MAC  : 0xFFFFFC12..0xFFFFFC17
EEPROM IP   : 0xFFFFFC18..0xFFFFFC1B
EEPROM WE   : 0xFFFFFCFF
```

This is an experimental implementation and is not an official Bee Beans Technologies utility. Proprietary executables, libraries, and user-specific MPC/MPCX files are not included.


## Documentation

- [For developers](FOR_DEVELOPERS.md) — architecture, build/development notes, technical evidence, MPC/MPCX EEPROM mappings, device-generation detection, references, and testing.
- [Agent instructions](AGENTS.md) — constraints for automated development.

The developer guide includes the relevant Bee Beans Technologies documentation references, including the SiTCPXG manual and MPC Writer XG guide.
