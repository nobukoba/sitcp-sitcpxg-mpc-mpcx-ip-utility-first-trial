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

`mpc-mpcx-command` is the advanced/diagnostic interface. Preserve the subcommands `inspect`, `read`, `verify`, `mpcx-plan`, `probe`, `rbcp-read`, `rbcp-write`, and `clear`. The high-level `write` path should continue to use the verified `mpc-mpcx-writer` implementation rather than creating a second divergent programming path.

## Compatibility rules

- Support Linux, macOS, and WSL using POSIX sockets.
- C++17 is the baseline.
- RBCP default UDP port: 4660.
- Default timeout: 3 seconds; retain `--timeout`.
- Detect MPC versus MPCX from the 22-byte payload, never from the filename extension.
- Detect SiTCP versus SiTCP-XG automatically where possible.
- EEPROM writes must restore write protection even after failures.
- Read-back verification is mandatory after destructive EEPROM writes.
- Do not blindly retry EEPROM writes after a lost UDP acknowledgement; the write may already have occurred.
- Read operations may retry timeouts.

## Installation

Default `PREFIX` is `$(CURDIR)/install`, not `/usr/local`. `make install` must work without root privileges and install all five public commands. System installation remains available with `PREFIX=/usr/local`.

## Safety

Do not guess register mappings for destructive operations. Read-only probes are preferred while reconstructing behavior. In particular, IP writing must remain disabled until EEPROM and current/runtime IP mappings and their behavior are verified for both normal SiTCP and SiTCP-XG.

`mpc-mpcx-command rbcp-write` and `clear` are intentionally low-level/destructive. Keep explicit command names and the `--yes-really-clear` guard for clear.

Never commit proprietary MPC/MPCX files, official proprietary executables/libraries, credentials, or device-specific secrets.

## Documentation

Keep these documents synchronized with implementation changes:

- `README.md`: user-facing quick start and command usage.
- `FOR_DEVELOPERS.md`: architecture, build/development notes, testing, and implementation status.
- `REVERSE_ENGINEERING.md`: evidence and reconstructed protocol/register behavior, clearly distinguishing public documentation, static analysis, and hardware verification.
- `AGENTS.md`: constraints future automated development must preserve.

## Development priorities

1. Preserve the verified C++ `mpc-mpcx-writer` behavior.
2. Keep `mpc-mpcx-command` compatible with the previous Python utility's advanced command set.
3. Complete and test the read-only C++ readers.
4. Factor duplicated RBCP and MPC/MPCX logic into shared C++ modules without changing behavior.
5. Verify EEPROM and runtime IP mappings on real SiTCP and SiTCP-XG hardware.
6. Only then enable `sitcp-sitcpxg-ip-writer`, with EEPROM as the default destination and an explicit option for current/runtime IP changes.
7. Runtime IP writes should reconnect to the new address for read-back verification when hardware behavior permits it.
8. Add CI builds for Linux and macOS.
