# For developers

## Scope

This repository contains two related but functionally separate C++11 utility groups for SiTCP / SiTCP-XG:

1. MPC/MPCX license/configuration utilities.
2. SiTCP Utility compatible IP-address-only utilities.

They may share low-level transport code, but they must not share domain logic.

## Build

```bash
make
make install
```

The default build uses:

```text
-std=c++11
```

Do not introduce C++14/17/20-only language or library features without explicitly raising the minimum requirement.

Default installation is local to the checkout:

```text
./install/bin/
```

Override with, for example:

```bash
make install PREFIX=$HOME/.local
sudo make install PREFIX=/usr/local
```

## Current programs

### `mpc-mpcx-ip-writer`

Public high-level MPC/MPCX writer. It requires an MPC/MPCX file and may additionally set EEPROM/default and current/runtime IP addresses.

### `mpc-mpcx-ip-reader`

Public MPC/MPCX reader. It also reports current/EEPROM MAC and IP values.

### `mpc-mpcx-ip-command`

Advanced/diagnostic interface with MPC/MPCX, IP, and low-level RBCP operations.

### `sitcp-sitcpxg-ip-reader`

IP-only SiTCP / SiTCP-XG reader. It must remain independent of MPC/MPCX payload decoding.

### `sitcp-sitcpxg-ip-writer`

IP-only SiTCP / SiTCP-XG writer. It changes only IP configuration and must never rewrite MPC/MPCX license data as part of an IP-only operation.

The older `mpc-mpcx-writer`, `mpc-mpcx-reader`, and `mpc-mpcx-command` source files remain internal implementation units and are not installed public names.

## Architectural boundary

Keep these domains separate:

```text
MPC/MPCX domain:
  payload classification
  license/MAC payload reconstruction
  MPC/MPCX EEPROM layout
  MPC/MPCX writer/read/verify/clear

IP utility domain:
  current/runtime IP read
  EEPROM/default IP read
  EEPROM IP write
  optional current/runtime IP write
  reconnect/read-back after runtime IP change
```

A common RBCP transport helper may be shared, but MPC/MPCX payload logic must not leak into the standalone IP-only commands.

## C++11 compatibility

The full installed command set must compile as C++11.

Avoid C++17-only facilities such as:

```text
std::optional
std::string_view
std::filesystem
structured bindings
if constexpr
```

Also remember that writable `std::string::data()` is not available in C++11. When a legacy `char**` interface is unavoidable and the string is non-empty, use writable storage such as `&value[0]` with the lifetime kept stable for the call.

C++11 facilities such as `auto`, range-based `for`, `nullptr`, initializer lists, `std::thread`, and `std::chrono` are allowed.

## MPC/MPCX RBCP implementation

Current wire format used by the MPC/MPCX implementation:

```text
FF CMD ID LEN ADDR[31:24] ADDR[23:16] ADDR[15:8] ADDR[7:0]
```

Commands used:

```text
0xC0 read
0x80 write
```

The reply packet ID must match. Bit 0 in the reply command/status byte is treated as an RBCP bus error.

Reads may be retried after timeout. EEPROM writes must not be blindly retried because a missing UDP ACK does not prove that the device failed to perform the write.

## MPC/MPCX EEPROM mappings

Current verified mappings are:

```text
SiTCP-XG:
  payload[0:16]  -> FC00..FC0F
  preserve       -> FC10..FC11
  payload[16:22] -> FC12..FC17

normal SiTCP:
  payload[0:6]   -> FC12..FC17
  payload[6:22]  -> FC40..FC4F
```

These mappings belong to MPC/MPCX handling and must not be used as the basis of the standalone IP utility implementation.

Determine the target generation first by reading the documented SiTCPXG Identifier register at `0xFFFFFF08..0xFFFFFF0B`; an exact value of `0x58544350` identifies SiTCP-XG. Keep this independent of MPC/MPCX payload classification. Do not infer generation from FC00/FC40 contents, and do not restore the former experimental `0xFFFFFF50` probe.

## IP utility design

The standalone IP-only CLI is:

```text
sitcp-sitcpxg-ip-reader CURRENT_IP [options]
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

Requirements:

- reader displays current/runtime and EEPROM/default network configuration;
- writer changes only IP-related state;
- EEPROM is the default write destination;
- an explicit option selects current/runtime IP modification;
- writer performs read-back verification;
- after a current/runtime IP change, reconnect to the new IP and verify;
- automatically handle normal SiTCP and SiTCP-XG where their behavior differs;
- do not inspect or modify MPC/MPCX license payloads as part of these commands.

## Refactoring direction

Shared low-level pieces may eventually look like:

```text
src/rbcp.hpp
src/rbcp.cpp
src/mpc_mpcx.hpp
src/mpc_mpcx.cpp
src/sitcp_ip.hpp
src/sitcp_ip.cpp
```

Do not create a generic abstraction that silently mixes MPC/MPCX license layout with IP configuration layout.

## Testing

At minimum, test:

- C++11 build with GCC and Clang where available;
- all five executables are built and installed by `make install`;
- MPC/MPCX classifier and verified EEPROM mappings remain unchanged;
- `mpc-mpcx-command` subcommands and safety guards;
- standalone IP reader/writer do not depend on an MPC/MPCX file;
- EEPROM IP write read-back;
- current/runtime IP change followed by reconnect/read-back;
- normal SiTCP and SiTCP-XG behavior tested independently.

Hardware-destructive tests should only be run on a controlled target where recovery is possible.


---

# Technical evidence and protocol notes

This document records the technical evidence behind behavior implemented by this project. Two independent functional domains are covered here:

1. MPC/MPCX license/configuration handling.
2. SiTCP Utility compatible IP-address handling.

Do not infer IP behavior from MPC/MPCX EEPROM layout merely because both concern SiTCP/SiTCP-XG.

For each domain, keep three evidence classes distinct:

1. public official Bee Beans Technologies documentation;
2. static analysis of the relevant official Windows utility;
3. read-only or controlled verification against real files/hardware.

## Public references

For generation detection, the authoritative public reference is the Bee Beans Technologies SiTCPXG manual, section 4.2.3. It defines the read-only SiTCPXG Identifier register as `0x58544350`. The current implementation reads that identifier at `0xFFFFFF08..0xFFFFFF0B` first, independently of MPC/MPCX payload classification. The former experimental `0xFFFFFF50` probe must not be used as a generation identifier.


- Bee Beans Technologies SiTCP downloads: https://www.bbtech.co.jp/download-files/sitcp/index_en.html
- SiTCP MPC Writer XG guide: https://www.bbtech.co.jp/download-files/sitcp/SiTCP-MPC-Writer-XG-en.0.1.1.pdf
- Bee Beans Technologies `sitcpy`: https://github.com/BeeBeansTechnologies/sitcpy
- SiTCP Forum: https://sitcp.bbtech.co.jp/

## MPC/MPCX: RBCP recovered/confirmed behavior

The MPC Writer uses an 8-byte RBCP header:

```text
FF CMD ID LEN ADDR[31:0]
```

with `0xC0` for read and `0x80` for write.

Static analysis of `SiTcpMpcWriteXG.exe 0.4.1-2-gc782` identified the RBCP write routine around `0x409960` and read routine around `0x409e50`.

## MPC/MPCX: EEPROM

The relevant EEPROM window begins at:

```text
0xFFFFFC00
```

EEPROM write protection control observed for MPC/MPCX programming is:

```text
0xFFFFFCFF = 0x00  enable writes
0xFFFFFCFF = 0xFF  disable writes
```

The official MPC Writer clear path was observed to fill `FC00..FC7F` with `FF` in 16-byte writes while EEPROM writing is enabled, then restore protection.

## MPC/MPCX payload classifier

The selected payload must be exactly 22 (`0x16`) bytes.

Recovered classifier behavior:

```text
normal-SiTCP candidate:
  payload[6:13]
  subtract 0x34 from every non-zero byte
  if the resulting seven-byte tag is valid -> internal type 2

SiTCP-XG candidate:
  payload[0:7]
  subtract 0x2C from every non-zero byte
  if the resulting seven-byte tag is valid -> internal type 1
```

The normal candidate is tested first. Valid tag characters include NUL, space, hyphen, digits, and ASCII letters. Known matching files decode to `Other  `.

The filename extension is therefore not authoritative; detection must use file contents.

## MPC/MPCX verified SiTCP-XG mapping

Static analysis plus matching real file/device verification supports:

```text
payload[0:16]  -> EEPROM FC00..FC0F
FC10..FC11     -> preserve current device bytes
payload[16:22] -> EEPROM FC12..FC17
```

The final six payload bytes correspond to the target MAC in the verified pair. FC10..FC11 are not fixed constants and must be preserved.

## MPC/MPCX verified normal SiTCP mapping

Matching file/device verification supports:

```text
payload[0:6]  -> EEPROM FC12..FC17
payload[6:22] -> EEPROM FC40..FC4F
```

This is a separate layout from SiTCP-XG and must remain a distinct MPC/MPCX implementation path.

## MPC/MPCX device generation detection

Target-generation detection is independent of MPC/MPCX payload classification. The implementation first reads the documented SiTCPXG Identifier register:

```text
0xFFFFFF08..0xFFFFFF0B = 0x58544350
```

An exact identifier match selects SiTCP-XG. A bus error or identifier mismatch selects normal SiTCP. A timeout leaves the generation unresolved. The previous experimental probe at `0xFFFFFF50` must not be used.

EEPROM payload classification is still used to validate/reconstruct MPC/MPCX data, but it is not used to decide whether the target is SiTCP or SiTCP-XG. This separation avoids generation decisions based on stale or alternate-layout EEPROM contents.

Hardware/file pairs verified on 2026-09-19:

```text
SiTCP-XG 192.168.2.187:
  .mpcx = FC00..FC0F + FC12..FC17

normal SiTCP 192.168.2.161:
  .mpc  = FC12..FC17 + FC40..FC4F
```

This is MPC/MPCX-side detection logic. It must not automatically be reused as the design basis for the IP-only utility unless independently verified there.

## MPC/MPCX write sequence used by this project

1. read the current EEPROM image needed for the target generation;
2. preserve bytes that must not be replaced;
3. release EEPROM write protection;
4. write the image in 16-byte blocks;
5. restore protection, including error paths where possible;
6. read the programmed image again;
7. compare byte-for-byte and fail on mismatch.

Reads may retry timeouts. Destructive writes are not blindly retried after timeout because UDP acknowledgement loss does not prove that the write did not occur.

# SiTCP Utility IP-address domain

The IP reader/writer in this repository are intended to reproduce only the IP-address functionality of the SiTCP Utility. This is a separate technical-analysis task from MPC/MPCX.

The IP tools must not classify, reconstruct, validate, or modify MPC/MPCX license payloads merely to read or change an IP address.

The previously attempted implementation that reused the MPC/MPCX reader was incorrect and has been removed.

## IP behavior to verify

Determine independently for normal SiTCP and SiTCP-XG:

- how the SiTCP Utility discovers/contacts the device;
- how it reads the current/runtime IP;
- how it reads the EEPROM/default IP;
- how it writes the EEPROM/default IP;
- how it changes the current/runtime IP without changing EEPROM when requested;
- whether a current/runtime change takes effect immediately;
- whether the existing connection becomes invalid after the change;
- how the official utility reconnects or verifies the new address;
- whether checksums, resets, reload commands, write-enable sequences, or generation-specific commands are involved;
- whether normal SiTCP and SiTCP-XG use different mechanisms.

Until these are verified, both IP source files remain non-destructive placeholders.

## Intended IP CLI behavior after verification

```text
sitcp-sitcpxg-ip-reader CURRENT_IP [options]
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

Design requirements:

- reader reports current/runtime and EEPROM/default IP values;
- writer changes only IP-related configuration;
- EEPROM is the default write target;
- changing current/runtime IP requires an explicit option;
- writer performs read-back verification;
- after changing current/runtime IP, reconnect to `NEW_IP` and verify when possible.

## Evidence summary

| Item | Public docs | Utility/static analysis | Hardware verification |
| --- | --- | --- | --- |
| MPC/MPCX RBCP access | yes | yes | yes |
| MPC/MPCX EEPROM FC00..FCFF | yes | yes | yes |
| MPC/MPCX payload exactly 22 bytes | not found | yes | yes |
| MPC/MPCX content classifier | not found | yes | yes |
| XG 16 + preserve 2 + 6 mapping | not found | yes | yes |
| normal 6 + 16 mapping | not found in reviewed material | investigated | yes |
| IP-only current/runtime read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM/default read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM write mechanism | to investigate | to investigate | not yet verified |
| IP-only runtime write/reconnect | to investigate | to investigate | not yet verified |

## Remaining work

### MPC/MPCX

- verify exact semantic meaning of all license bytes;
- determine the meaning of preserved XG FC10..FC11 bytes;
- strengthen target-generation detection;
- compare the C++ readers/command against known hardware and the previous Python implementation.

### IP utility

- identify and analyze the official SiTCP Utility behavior specific to IP changes;
- implement an IP-only reader after read mechanisms are verified;
- implement EEPROM IP write with read-back;
- implement optional current/runtime IP change;
- verify reconnect/read-back at the new IP;
- test normal SiTCP and SiTCP-XG independently.
