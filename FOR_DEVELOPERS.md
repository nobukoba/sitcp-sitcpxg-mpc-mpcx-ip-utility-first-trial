# For developers

## Scope

This repository contains two related but functionally separate C++11 utility groups for SiTCP / SiTCP-XG:

1. MPC/MPCX license/configuration utilities.
2. SiTCP Utility compatible IP-address-only utilities.

They may share low-level RBCP transport code, but they must not share domain logic. Keep three evidence classes distinct when documenting behavior: public Bee Beans Technologies documentation, static analysis of official utilities, and controlled verification with real files/hardware.

## Build and compatibility

```bash
make
make install
```

The default build uses `-std=c++11`. Do not introduce C++14/17/20-only features without explicitly raising the minimum requirement. In particular, avoid `std::optional`, `std::string_view`, `std::filesystem`, structured bindings, `if constexpr`, and assumptions that `std::string::data()` is writable.

Default installation is local to the checkout:

```text
./install/bin/
```

Override with, for example:

```bash
make install PREFIX=$HOME/.local
sudo make install PREFIX=/usr/local
```

Targets are Linux, macOS, and WSL using POSIX sockets.

## Current programs

- `mpc-mpcx-ip-writer` — high-level MPC/MPCX writer; requires an MPC/MPCX file and may additionally set EEPROM/default and current/runtime IP addresses.
- `mpc-mpcx-ip-reader` — MPC/MPCX reader; also reports current/EEPROM MAC and IP values.
- `mpc-mpcx-ip-command` — advanced/diagnostic interface with MPC/MPCX, IP, and low-level RBCP operations.
- `sitcp-sitcpxg-ip-reader` — IP-only reader, independent of MPC/MPCX payload decoding.
- `sitcp-sitcpxg-ip-writer` — IP-only writer; must never rewrite MPC/MPCX license data as part of an IP-only operation.

## Architectural boundary

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

## Public references

For generation detection, the authoritative public reference is the Bee Beans Technologies SiTCP-XG manual, section 4.2.3. It defines the read-only SiTCP-XG Identifier register as `0x58544350`.

- Bee Beans Technologies SiTCP downloads: https://www.bbtech.co.jp/download-files/sitcp/index_en.html
- SiTCP MPC Writer XG guide: https://www.bbtech.co.jp/download-files/sitcp/SiTCP-MPC-Writer-XG-en.0.1.1.pdf
- Bee Beans Technologies `sitcpy`: https://github.com/BeeBeansTechnologies/sitcpy
- SiTCP Forum: https://sitcp.bbtech.co.jp/

## MPC/MPCX RBCP behavior

The implementation uses an 8-byte RBCP header:

```text
FF CMD ID LEN ADDR[31:24] ADDR[23:16] ADDR[15:8] ADDR[7:0]
```

Commands are `0xC0` for read and `0x80` for write. The reply packet ID must match; bit 0 in the reply command/status byte is treated as an RBCP bus error.

Static analysis of `SiTcpMpcWriteXG.exe 0.4.1-2-gc782` identified the RBCP write routine around `0x409960` and read routine around `0x409e50`.

Reads may retry after timeout. Destructive writes must not be blindly retried because a missing UDP ACK does not prove that the device failed to perform the write.

## MPC/MPCX EEPROM and write protection

The relevant EEPROM window begins at `0xFFFFFC00`. MPC/MPCX programming uses:

```text
0xFFFFFCFF = 0x00  enable writes
0xFFFFFCFF = 0xFF  disable writes
```

The official MPC Writer clear path was observed to fill `FC00..FC7F` with `FF` in 16-byte writes while EEPROM writing is enabled, then restore protection.

## MPC/MPCX payload classification

The selected payload must be exactly 22 (`0x16`) bytes. The filename extension is not authoritative.

Recovered classifier behavior:

```text
normal SiTCP candidate:
  payload[6:13]
  subtract 0x34 from every non-zero byte
  valid seven-byte tag -> internal type 2

SiTCP-XG candidate:
  payload[0:7]
  subtract 0x2C from every non-zero byte
  valid seven-byte tag -> internal type 1
```

The normal candidate is tested first. Valid tag characters include NUL, space, hyphen, digits, and ASCII letters. Known matching files decode to `Other  `.

Payload classification validates MPC/MPCX data; it does not determine the target hardware generation.

## MPC/MPCX device generation detection

Determine the target generation first and independently by reading:

```text
0xFFFFFF08..0xFFFFFF0B
```

Only an exact value of `0x58544350` identifies SiTCP-XG. A bus error or identifier mismatch selects normal SiTCP. A timeout leaves the generation unresolved.

Do not infer generation from FC00/FC40 payload-looking contents; verified hardware can contain stale or alternate-layout data that makes both reconstructed payloads look valid. The former experimental `0xFFFFFF50` probe must not be used.

## Verified MPC/MPCX EEPROM mappings

SiTCP-XG:

```text
payload[0:16]  -> FC00..FC0F
FC10..FC11     -> preserve current device bytes
payload[16:22] -> FC12..FC17
```

The final six payload bytes correspond to the target MAC in the verified pair. FC10..FC11 are not fixed constants and must be preserved.

Normal SiTCP:

```text
payload[0:6]  -> FC12..FC17
payload[6:22] -> FC40..FC4F
```

Hardware/file pairs verified on 2026-09-19:

```text
SiTCP-XG 192.168.2.187:
  .mpcx = FC00..FC0F + FC12..FC17

normal SiTCP 192.168.2.161:
  .mpc  = FC12..FC17 + FC40..FC4F
```

These mappings belong only to MPC/MPCX handling and must not be used as the basis of IP-only operations.

## SiTCP-XG EEPROM parameter area

The public SiTCP-XG manual documents `0xFFFFFC10..0xFFFFFC4F` as the
EEPROM initial values for runtime registers `0xFFFFFF10..0xFFFFFF4F`.
Therefore, bytes in this range are device/network configuration and must not
be normalized merely because two boards differ.

Relevant documented runtime fields include:

```text
FC18..FC1B  IP address
FC1C..FC1D  TCP port
FC20..FC21  TCP maximum segment size
FC22..FC23  UDP port
FC24..FC25  TCP keepalive time (buffer not empty)
FC26..FC27  TCP keepalive time (buffer empty)
FC28..FC29  TCP timeout (connecting)
FC2A..FC2B  TCP timeout (disconnect)
FC2C..FC2D  TCP maximum segment lifetime
FC2E..FC2F  TCP retransmission time
FC32..FC37  TCP server MAC address
FC38..FC3B  TCP server IP address
FC3C..FC3D  TCP server port
FC40..FC41  transmission rate
```

For MPCX programming, the verified 22-byte payload mapping changes only
`FC00..FC0F` and `FC12..FC17`; `FC10..FC11` are preserved. The writer
must also preserve `FC18` and later configuration bytes. In particular,
different values observed at FC20/FC2A/FC40 on different SiTCP-XG boards are
not evidence of a bad MPCX payload.

## MPC/MPCX write sequence

1. Read the current EEPROM image needed for the target generation.
2. Preserve bytes that must not be replaced.
3. Release EEPROM write protection.
4. Write the image in 16-byte blocks.
5. Restore protection, including error paths where possible.
6. Read the programmed image again.
7. Compare byte-for-byte and fail on mismatch.

## IP utility design

The standalone IP-only CLI is:

```text
sitcp-sitcpxg-ip-reader CURRENT_IP [options]
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

The IP tools must not classify, reconstruct, validate, or modify MPC/MPCX license payloads merely to read or change an IP address.

Requirements:

- reader reports current/runtime and EEPROM/default network configuration;
- writer changes only IP-related state;
- EEPROM is the default write target;
- changing current/runtime IP requires an explicit option;
- writer performs read-back verification;
- after changing current/runtime IP, reconnect to `NEW_IP` and verify when possible;
- normal SiTCP and SiTCP-XG behavior must be tested independently.

## Refactoring direction

Shared code is separated into `sitcp-sitcpxg-rbcp.hpp` (RBCP transport), `sitcp-sitcpxg-network-config.hpp` (network configuration), and `sitcp-sitcpxg-mpc-mpcx.hpp` (MPC/MPCX operations). Keep these boundaries explicit; do not create a generic abstraction that silently mixes MPC/MPCX license layout with IP configuration layout.

## Testing

At minimum, test:

- C++11 builds with GCC and Clang where available;
- all five executables are built and installed by `make install`;
- MPC/MPCX classifier and verified EEPROM mappings remain unchanged;
- generation detection uses the documented SiTCP-XG Identifier;
- `mpc-mpcx-ip-command` subcommands and safety guards;
- standalone IP reader/writer do not depend on an MPC/MPCX file;
- EEPROM IP write read-back;
- current/runtime IP change followed by reconnect/read-back;
- normal SiTCP and SiTCP-XG independently.

Hardware-destructive tests should only be run on a controlled target where recovery is possible.

## Evidence summary

| Item | Public docs | Utility/static analysis | Hardware verification |
| --- | --- | --- | --- |
| MPC/MPCX RBCP access | yes | yes | yes |
| MPC/MPCX EEPROM FC00..FCFF | yes | yes | yes |
| MPC/MPCX payload exactly 22 bytes | not found | yes | yes |
| MPC/MPCX content classifier | not found | yes | yes |
| XG 16 + preserve 2 + 6 mapping | not found | yes | yes |
| normal 6 + 16 mapping | not found in reviewed material | investigated | yes |
| SiTCP-XG Identifier generation detection | yes | implemented | verified on XG hardware |
| IP-only current/runtime read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM/default read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM write mechanism | to investigate | to investigate | not yet verified |
| IP-only runtime write/reconnect | to investigate | to investigate | not yet verified |

## Remaining work

### MPC/MPCX

- verify exact semantic meaning of all license bytes;
- determine the meaning of preserved XG FC10..FC11 bytes;
- compare the C++ readers/command against known hardware and the previous Python implementation.

### IP utility

- identify and analyze official SiTCP Utility behavior specific to IP changes;
- verify EEPROM IP write/read-back;
- verify optional current/runtime IP change and reconnect;
- test normal SiTCP and SiTCP-XG independently.

## MPC/MPCX file inspection and MAC extraction

`mpc-mpcx-ip-command` uses `MPC_OR_MPCX_FILE` in help text instead of the ambiguous `FILE`. It means a 22-byte `.mpc` or `.mpcx` license/configuration file.

```text
mpc-mpcx-ip-command inspect MPC_OR_MPCX_FILE
mpc-mpcx-ip-command mac MPC_OR_MPCX_FILE
```

For verified payload layouts, the embedded MAC address is `payload[0:6]` for normal SiTCP/MPC and `payload[16:22]` for SiTCP-XG/MPCX. `inspect` reports the detected payload type, embedded MAC, and payload; `mac` reports the detected type and embedded MAC.
