# AGENTS.md

## Project goal

Provide small C++17 command-line utilities for SiTCP and SiTCP-XG configuration over RBCP, without requiring the official Windows GUI or Python at runtime.

## Commands

The intended public command set is:

- `mpc-mpcx-writer`
- `mpc-mpcx-reader`
- `mpc-mpcx-command`
- `sitcp-sitcpxg-ip-reader`
- `sitcp-sitcpxg-ip-writer`

Keep command names, option defaults, and output formatting consistent.

All `--help` / usage output must show the actual default value whenever an option has a default. For example, show `--port N ... (default: 4660)` and `--timeout SEC ... (default: 3)` rather than documenting only the option name. For choice options, explicitly mark the default choice, e.g. `--eeprom ... (default)`.

`mpc-mpcx-command` is the advanced/diagnostic interface for MPC/MPCX and low-level RBCP work. Preserve the subcommands `inspect`, `read`, `verify`, `mpcx-plan`, `probe`, `rbcp-read`, `rbcp-write`, and `clear`. The high-level MPC/MPCX `write` path should continue to use the verified `mpc-mpcx-writer` implementation rather than creating a second divergent programming path.

## Critical separation: MPC/MPCX vs IP utilities

The IP utilities are **not** MPC/MPCX utilities.

- `sitcp-sitcpxg-ip-reader` and `sitcp-sitcpxg-ip-writer` correspond only to the IP-address functionality of the SiTCP Utility.
- They must not classify, reconstruct, validate, read for presentation, or modify MPC/MPCX license payloads.
- Do not implement an IP reader by including or wrapping `mpc-mpcx-reader`.
- Shared low-level RBCP transport code is acceptable, but MPC/MPCX payload logic and IP-setting logic must remain separate modules.
- The IP reader/writer must always display current MAC, current IP, EEPROM MAC, and EEPROM IP when communicating with a target.
- The IP writer changes only IP configuration. EEPROM is the default target; current/runtime IP change requires an explicit option.
- After changing the current/runtime IP, reconnect to the new address and perform read-back verification when the SiTCP/SiTCP-XG behavior supports it.

## Compatibility rules

- Support Linux, macOS, and WSL using POSIX sockets.
- C++17 is the baseline.
- RBCP default UDP port: 4660 where RBCP is the verified transport for the operation.
- Default timeout: 3 seconds; retain `--timeout` where applicable.
- Detect MPC versus MPCX from the 22-byte payload, never from the filename extension.
- Detect SiTCP versus SiTCP-XG automatically where appropriate.
- EEPROM writes must restore write protection even after failures where that mechanism applies.
- Read-back verification is mandatory after destructive configuration writes.
- Do not blindly retry destructive writes after a lost UDP acknowledgement; the write may already have occurred.
- Read operations may retry timeouts.

## Installation

Default `PREFIX` is `$(CURDIR)/install`, not `/usr/local`. `make install` must work without root privileges and install all five public commands. System installation remains available with `PREFIX=/usr/local`.

## Safety

Do not guess register/protocol mappings for destructive operations. Read-only probes are preferred while reconstructing behavior.

`mpc-mpcx-command rbcp-write` and `clear` are intentionally low-level/destructive. Keep explicit command names and the `--yes-really-clear` guard for clear.

Never commit proprietary MPC/MPCX files, official proprietary executables/libraries, credentials, or device-specific secrets.

## Documentation

Keep these documents synchronized with implementation changes:

- `README.md`: user-facing quick start and command usage.
- `FOR_DEVELOPERS.md`: architecture, build/development notes, testing, and implementation status.
- `REVERSE_ENGINEERING.md`: evidence and reconstructed protocol/register behavior, clearly distinguishing MPC/MPCX analysis from SiTCP Utility IP analysis.
- `AGENTS.md`: constraints future automated development must preserve.

## Development priorities

1. Preserve the verified C++ `mpc-mpcx-writer` behavior.
2. Keep `mpc-mpcx-command` compatible with the previous Python utility's advanced command set.
3. Keep IP utilities completely independent of MPC/MPCX payload handling.
4. Keep current/EEPROM MAC and IP reporting available in the IP tools.
5. Implement and verify EEPROM IP writing as the default operation.
6. Implement an explicit runtime/current IP option and reconnect to the new address for read-back verification.
7. Factor truly shared low-level transport code into common modules without merging the two functional domains.
8. Add CI builds for Linux and macOS.
