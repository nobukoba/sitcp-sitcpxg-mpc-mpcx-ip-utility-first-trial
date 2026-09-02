# For developers

## Scope

This repository is a C++17 utility suite for SiTCP / SiTCP-XG configuration through RBCP. It is intentionally dependency-light and currently uses POSIX UDP sockets directly.

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

Initial C++ implementation. It reads EEPROM in 8-byte chunks, reconstructs the MPC/MPCX payload according to the detected generation, reports MAC/IP-related EEPROM fields, and prints the raw FC00..FC4F image. It is read-only.

### `sitcp-sitcpxg-ip-reader`

Initial read-only implementation using the same EEPROM/RBCP decoder as `mpc-mpcx-reader`. Keep the separate executable name because the IP-specific presentation and runtime-register reading will evolve independently.

### `sitcp-sitcpxg-ip-writer`

Executable exists but destructive write support is deliberately disabled. Do not enable it until both EEPROM and current/runtime IP mappings are verified on normal SiTCP and SiTCP-XG.

## RBCP implementation

Current wire format:

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

## EEPROM MPC/MPCX mappings

See `REVERSE_ENGINEERING.md` for evidence and details. The implementation currently relies on:

```text
SiTCP-XG:
  payload[0:16]  -> FC00..FC0F
  preserve       -> FC10..FC11
  payload[16:22] -> FC12..FC17

normal SiTCP:
  payload[0:6]   -> FC12..FC17
  payload[6:22]  -> FC40..FC4F
```

EEPROM write protection is controlled at `0xFFFFFCFF`, with `0x00` enabling writes and `0xFF` disabling them.

## Target detection

First reconstruct both possible payload layouts from EEPROM and classify them. If both appear valid, use the read-only XG probe at `0xFFFFFF50`: readable is treated as SiTCP-XG; an RBCP bus error is treated as normal SiTCP. A timeout remains unresolved.

## Refactoring direction

The first-trial implementation intentionally prioritized a working standalone writer. The next structural improvement should extract shared components from the command sources, for example:

```text
src/rbcp.hpp
src/rbcp.cpp
src/mpc_mpcx.hpp
src/mpc_mpcx.cpp
src/sitcp_device.hpp
src/sitcp_device.cpp
```

Do this without changing verified behavior.

## IP writer design target

Once register behavior is verified:

- EEPROM is the default write destination.
- Provide an explicit option for changing the current/runtime IP.
- Reader should show both EEPROM and current/runtime values.
- Writer must read back and verify.
- After a runtime IP change, reconnect using the new IP and verify there when possible.
- Automatic SiTCP/SiTCP-XG detection should remain the default.

## Testing

At minimum, test:

- C++17 build with GCC and Clang.
- invalid file sizes and invalid MPC/MPCX classifiers.
- port range and positive timeout validation.
- target mismatch refusal.
- timeout and bus-error paths.
- preservation of FC10..FC11 for XG.
- exact normal-SiTCP FC12..FC17 and FC40..FC4F mapping.
- write protection restoration on exceptions.
- byte-for-byte read-back mismatch reporting.

Hardware-destructive tests should only be run on devices for which the corresponding MPC/MPCX and configuration are known.
