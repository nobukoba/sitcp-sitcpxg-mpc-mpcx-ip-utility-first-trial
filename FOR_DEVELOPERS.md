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

These mappings belong to MPC/MPCX handling and must not be used as the basis of the standalone IP utility implementation.

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
