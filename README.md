# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

Experimental C++11 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

The installed command set contains five commands:

- `mpc-mpcx-ip-writer` — write MPC/MPCX EEPROM data and optionally change EEPROM/current IP addresses.
- `mpc-mpcx-ip-reader` — read MPC/MPCX information and always display current/EEPROM MAC and IP addresses.
- `mpc-mpcx-ip-command` — advanced MPC/MPCX, IP, and low-level RBCP operations.
- `sitcp-sitcpxg-ip-writer` — IP-only writer for SiTCP / SiTCP-XG.
- `sitcp-sitcpxg-ip-reader` — IP-only reader for SiTCP / SiTCP-XG.

## Quick start

```bash
git clone https://github.com/nobukoba/sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial.git
cd sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial
make
make install
```

Run these commands from the repository root. The default `PREFIX` is
`$(CURDIR)`, and `BINDIR` defaults to `$(PREFIX)/bin`.
No administrator privileges are needed for the default installation.

| Command | Output location | How to run |
| --- | --- | --- |
| `make` | `./src/` (build output) | `./src/mpc-mpcx-ip-reader DEVICE_IP` |
| `make install` | `./bin/` (installed copy) | `./bin/mpc-mpcx-ip-reader DEVICE_IP` |

**All usage examples below use the installed copy in `./bin/`.**
For development without installation, replace that prefix with `./src/`.
Both directories contain the same five commands after a successful installation.
Running `make` alone does not refresh an existing installed copy.
Older versions installed into `./install/bin/`; those copies are no longer
updated by the default installation. Use `./bin/` after running `make install`.
`make clean` removes generated executables from `src/` while retaining source
files and installed binaries.

After updating the source, rebuild and refresh the installation:

```bash
git pull --ff-only
make install
```

`make install` builds any outdated binaries before copying them.

To choose another installation prefix:

```bash
make install PREFIX="$HOME/.local"
"$HOME/.local/bin/mpc-mpcx-ip-reader" DEVICE_IP
```

If `$HOME/.local/bin` is on your `PATH`, you can also run the command by name.
With a custom prefix, replace `./bin/` in the examples with your
chosen `PREFIX/bin/`. For a system installation, use
`sudo make install PREFIX=/usr/local` (administrator privileges required).

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

in separate Runtime and EEPROM sections, followed by MPC/MPCX information
reconstructed from EEPROM. The runtime length follows the detected device generation:

- Normal SiTCP runtime: `0xFFFFFF00..0xFFFFFF3F` (64 bytes)
- SiTCP-XG runtime: `0xFFFFFF00..0xFFFFFF4F` (80 bytes)
- EEPROM (both generations): `0xFFFFFC00..0xFFFFFC4F` (80 bytes)

Raw dumps use 16 hexadecimal bytes per row: four runtime rows for normal SiTCP,
five for XG, and five EEPROM rows for either generation. SiTCP-XG parameters are
decoded separately from each region; numeric register values include decimal and
hexadecimal forms, for example `10000 (0x2710) Mbps` or `4660 (0x1234)`.
Timeout conversions retain their units alongside the decimal/hex raw value.
IP addresses include network-byte-order hexadecimal notation, for example
`192.168.10.10 (0xC0A80A0A)`, in current, EEPROM, server, and compact IP-only
views. MAC addresses retain their usual colon-hex notation.
Normal SiTCP does not interpret its license bytes as XG transmission rates.

The same report is available with:

```bash
./bin/mpc-mpcx-ip-command read 192.168.2.161
```

Normal SiTCP reports do not request runtime `0xFFFFFF40..0xFFFFFF4F`, which
the SiTCP register manual lists as access-prohibited. Its EEPROM `FC40..FC4F`
remains readable and is required for MPC payload reconstruction. XG reports
include runtime `FF40..FF4F`. If generation detection times out, the report
fails before selecting a read range. On a block bus error, the diagnostic report reads that block byte by
byte, displays readable values, and marks rejected bytes as `??`. Warnings
identify each rejected address. Fields with missing bytes are `unavailable`;
they are never decoded using placeholder zeros. Runtime and EEPROM remain
separate, and readable EEPROM information is still displayed.

Read commands return `0` for a complete report, `3` for a partial report, and
`1` for a fatal error such as a timeout or short reply. Writers use compact
MAC/IP snapshots and retain mandatory write/read-back verification.

The reader determines the device generation first from the documented SiTCP-XG Identifier register at `0xFFFFFF08..0xFFFFFF0B`. An exact value of `0x58544350` identifies SiTCP-XG. MPC/MPCX payload classification is handled separately and is not used to determine the device generation.

## Writer

For programming, the MPC/MPCX file is a required positional argument:

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

To **clear only**, without programming a file:

```bash
./bin/mpc-mpcx-ip-writer 192.168.10.10 --clear
```

This erases license and saved settings in EEPROM `0xFFFFFC00..0xFFFFFC7F`
(128 bytes) to `FF`, restores write protection, and verifies every erased byte.
It displays compact before/after MAC/IP values and a success message in five nonempty lines plus a blank separator. It does not program
an MPC/MPCX file, initialize from RAM, or change runtime/IP registers. Do not
combine `--clear` with a file, `--set-eeprom-ip`, or `--set-current-ip`.
`--port` and `--timeout` are supported. Clearing is off by default. Reprogram
an appropriate license before returning the device to normal boot mode.

Writer options:

```text
--clear              Clear EEPROM only; no file/IP changes (default: off)
--set-eeprom-ip IP   Set EEPROM/default IP address
--set-current-ip IP  Set current/runtime IP address
--port N             RBCP UDP port (default: 4660)
--timeout SEC        RBCP timeout in seconds (default: 3)
-h, --help           Show help
```

When both IP options are given, EEPROM IP is written first and current/runtime IP is changed last. This keeps the original address reachable until all operations that require it have finished. After a current/runtime IP change, the writer reconnects to the new IP and performs read-back verification. It does not blindly retry a timed-out destructive current-IP write because the address may already have changed before the acknowledgement is received.

The writer prints five nonempty lines: runtime and EEPROM MAC/IP before (two lines),
after (two lines), then a blank line and
`Success! All operations completed and verified.`, followed by another blank line.
Use one space after `before:` and two after `after:` to align the fields. IPs retain hexadecimal
notation. Success is printed only after all requested operations and verification complete. Use the reader for full register dumps. MPC/MPCX payload type is determined from the 22-byte contents, not the filename extension.

## MPCX writing after clearing EEPROM

`mpc-mpcx-ip-writer` automatically checks EEPROM `0xFFFFFC10` bit7 for
SiTCP-XG. When this bit is set (including `FF` after `clear`), it reads the
complete runtime `0xFFFFFF00..0xFFFFFF4F` and builds an 80-byte EEPROM image:

| EEPROM range | Source when initialization is needed |
| --- | --- |
| `FC00..FC0F` | MPCX file, first 16 bytes |
| `FC10..FC11` | Runtime `FF10..FF11` |
| `FC12..FC17` | MPCX file, final 6 bytes (MAC) |
| `FC18..FC4F` | Runtime `FF18..FF4F`, including the transmission rate |

It writes and verifies all 80 bytes. When bit7 is clear, the existing 24-byte
MPCX write path preserves EEPROM settings, including `FC40..FC4F`. This is not
an automatic rate repair: runtime values are copied as read. Explicit
`--set-eeprom-ip` and `--set-current-ip` operations still follow programming,
in that order. Without an EEPROM IP override, initialization saves the current
runtime IP, which may be the device's ForceDefault address.

Preview without writing:

```bash
./bin/mpc-mpcx-ip-command mpcx-plan DEVICE_IP FILE.mpcx
```

Then program using the existing CLI:

```bash
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx
```

A diagnostic `??` is not usable as programming data. If the required runtime
image cannot be read in full, or its identifier/reset bit is inconsistent,
initialization stops before releasing EEPROM write protection. No incomplete
image, guessed default value, or automatic clear is written.

The official guide describes both RAM-based initialization and default-value
fallback. Analysis of version `0.4.1-2-gc782` confirms a fixed-default fallback
in its normal-SiTCP path, but does not establish a fallback image for MPCX.
Consequently this implementation does not substitute normal-SiTCP defaults
into SiTCP-XG. Normal MPC programming is unchanged. Optional official extension
copying beyond `FC4F` is not implemented; `FC50..FC7F` remain unchanged.

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

`read` and `ip-read` display the same full runtime/EEPROM report. `ip-write` and the standalone IP writer print five-nonempty-line before/after MAC/IP and result summaries. The standalone IP reader retains its compact MAC/IP view. `ip-write` defaults to EEPROM and accepts `--current` for the runtime/current address.

## Build requirements

- C++11 compiler (`g++` or `clang++`)
- POSIX sockets
- `make`

The default build uses `-std=c++11`. Targets are Linux, macOS, and WSL.

## Source formatting

Source files should use conventional readable C++ formatting. Avoid compressed one-line implementations; put control-flow blocks and logically separate statements on separate lines. Long expressions should be wrapped rather than packed into a single line.

## Implementation notes

The public commands use shared transport/network/MPC-MPCX code in `src/sitcp-sitcpxg-rbcp.hpp`, `src/sitcp-sitcpxg-network-config.hpp`, and `src/sitcp-sitcpxg-mpc-mpcx.hpp`. MPC/MPCX payload handling and IP register handling remain logically separated internally even though some commands expose both functions.

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

The developer guide includes the relevant Bee Beans Technologies documentation references, including the SiTCP-XG manual and MPC Writer XG guide.
