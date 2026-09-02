# For developers

## Scope

This repository contains two related but functionally separate C++17 utility groups for SiTCP / SiTCP-XG:

1. MPC/MPCX license/configuration utilities.
2. SiTCP Utility compatible IP-address-only utilities.

They may share low-level transport code, but they must not share domain logic.

## Build

```bash
make
make install
```

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

### `mpc-mpcx-writer`

Implemented. It validates the 22-byte payload, classifies MPC/MPCX by content, detects the target generation, preserves device-specific bytes, enables EEPROM writing, writes in small blocks, restores write protection, and verifies by read-back.

### `mpc-mpcx-reader`

Initial C++ implementation. It reads EEPROM in 8-byte chunks, reconstructs the MPC/MPCX payload according to the detected generation, reports relevant MPC/MPCX EEPROM fields, and prints the raw FC00..FC4F image. It is read-only.

### `mpc-mpcx-command`

Advanced/diagnostic C++ interface corresponding to the previous Python utility. Current subcommands are:

```text
inspect
read
verify
mpcx-plan
probe
rbcp-read
rbcp-write
clear
write
```

`inspect`, `read`, `verify`, `mpcx-plan`, `probe`, and `rbcp-read` are read-only. `rbcp-write` is a raw low-level write operation. `clear` requires `--yes-really-clear`. The `write` subcommand intentionally points users to `mpc-mpcx-writer` so the verified high-level write path remains in one implementation.

### `sitcp-sitcpxg-ip-reader`

This is an IP-only SiTCP Utility compatible command. It must not include, wrap, or reuse MPC/MPCX payload decoding. The previous implementation that included `mpc-mpcx-reader.cpp` was incorrect and has been removed.

Current source is intentionally a placeholder until the exact IP-only access method is verified. Final behavior should report the current/runtime IP and EEPROM/default IP, and nothing about MPC/MPCX license payloads.

### `sitcp-sitcpxg-ip-writer`

This is an IP-only SiTCP Utility compatible command. It changes only IP configuration. It must never rewrite MPC/MPCX license data as part of the IP operation.

Current destructive behavior is disabled until the exact IP-only access method is verified for normal SiTCP and SiTCP-XG.

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

A common `RbcpClient` or other transport helper may be shared only when the SiTCP Utility IP operation is verified to use that transport. Do not infer that because MPC/MPCX uses RBCP, the IP utility must use the same addresses or layout.

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

See `REVERSE_ENGINEERING.md` for evidence and details. Current verified mappings are:

```text
SiTCP-XG:
  payload[0:16]  -> FC00..FC0F
  preserve       -> FC10..FC11
  payload[16:22] -> FC12..FC17

normal SiTCP:
  payload[0:6]   -> FC12..FC17
  payload[6:22]  -> FC40..FC4F
```

These mappings belong to MPC/MPCX handling and must not be used as the basis of the IP utility implementation.

## IP utility design target

Reconstruct the behavior of the SiTCP Utility's IP-address function independently. The intended CLI behavior is:

```text
sitcp-sitcpxg-ip-reader CURRENT_IP [options]
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

Requirements:

- reader displays current/runtime IP and EEPROM/default IP as supported by the device;
- writer changes only IP-related state;
- EEPROM is the default write destination;
- an explicit option selects current/runtime IP modification;
- writer performs read-back verification;
- after a current/runtime IP change, reconnect to the new IP and verify when possible;
- automatically handle normal SiTCP and SiTCP-XG where their IP utility behavior differs;
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

Do not create a generic `sitcp_device` abstraction that silently mixes MPC/MPCX license layout with IP configuration layout.

## Testing

At minimum, test:

- C++17 build with GCC and Clang;
- all five executables are built and installed by `make install`;
- MPC/MPCX classifier and verified EEPROM mappings remain unchanged;
- `mpc-mpcx-command` subcommands and safety guards;
- IP reader output contains only IP-related information;
- IP writer never modifies MPC/MPCX payload/license areas except where an independently verified SiTCP Utility IP operation explicitly requires a byte that overlaps physically;
- EEPROM IP write read-back;
- current/runtime IP change followed by reconnect/read-back;
- normal SiTCP and SiTCP-XG behavior tested independently.

Hardware-destructive tests should only be run after the IP-specific mechanism is independently verified.
