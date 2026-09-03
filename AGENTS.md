# AGENTS.md

## Project goal

Provide small C++17 command-line utilities for SiTCP and SiTCP-XG configuration over RBCP, without requiring the official Windows GUI or Python at runtime.

## Public commands

The intended public command set is:

- `mpc-mpcx-ip-writer`
- `mpc-mpcx-ip-reader`
- `mpc-mpcx-ip-command`

The older `mpc-mpcx-writer`, `mpc-mpcx-reader`, and `mpc-mpcx-command` sources may remain as internal implementation units while the unified commands are being refactored, but they are not the public installed names.

Keep command names, option defaults, and output formatting consistent. All `--help` / usage output must show the actual default value whenever an option has a default. For example, show `--port N ... (default: 4660)` and `--timeout SEC ... (default: 3)`. For choice options, explicitly mark the default choice, e.g. EEPROM as the default IP write destination.

## Unified CLI, separated internals

MPC/MPCX payload handling and SiTCP IP-register handling are exposed through the same public commands, but must remain logically separated internally.

- Shared IP register logic lives in `src/ip-config.hpp` or a future equivalent shared module.
- MPC/MPCX payload classification/reconstruction must not be used to determine IP/MAC values.
- IP/MAC register reads must not modify or reinterpret MPC/MPCX license payloads.
- The reader must always display current MAC, current IP, EEPROM MAC, and EEPROM IP.
- The writer must display those four values before and after the operation whenever the target remains reachable.
- IP rewriting is optional. Use `--set-ip NEW_IP`; EEPROM is the default destination and `--current` explicitly selects current/runtime IP.
- IP-only operation must be supported without requiring an MPC/MPCX file.
- After changing current/runtime IP, reconnect to the new address and perform read-back verification.

## Advanced command

`mpc-mpcx-ip-command` is the advanced/diagnostic interface. Preserve the MPC/MPCX and low-level RBCP subcommands `inspect`, `read`, `verify`, `mpcx-plan`, `probe`, `rbcp-read`, `rbcp-write`, and `clear`, and also provide `ip-read` and `ip-write`.

The high-level MPC/MPCX programming path should continue to use the verified writer implementation rather than creating a second divergent destructive programming path.

## Compatibility rules

- Support Linux, macOS, and WSL using POSIX sockets.
- C++17 is the baseline.
- RBCP default UDP port: 4660.
- Default timeout: 3 seconds; retain `--timeout` where applicable.
- Detect MPC versus MPCX from the 22-byte payload, never from the filename extension.
- Detect SiTCP versus SiTCP-XG automatically where appropriate.
- EEPROM writes must restore write protection even after failures where that mechanism applies.
- Read-back verification is mandatory after destructive configuration writes.
- Do not blindly retry destructive writes after a lost UDP acknowledgement; the write may already have occurred.
- Read operations may retry timeouts.

## IP/MAC register map

Verified shared register locations:

- current MAC: `0xFFFFFF12..0xFFFFFF17`
- current IP: `0xFFFFFF18..0xFFFFFF1B`
- EEPROM MAC: `0xFFFFFC12..0xFFFFFC17`
- EEPROM IP: `0xFFFFFC18..0xFFFFFC1B`
- EEPROM write enable/protect: `0xFFFFFCFF`

Do not guess additional destructive register mappings.

## Installation

Default `PREFIX` is `$(CURDIR)/install`, not `/usr/local`. `make install` must work without root privileges and install the three public commands. System installation remains available with `PREFIX=/usr/local`.

## Safety

`mpc-mpcx-ip-command rbcp-write` and `clear` are intentionally low-level/destructive. Keep explicit command names and the `--yes-really-clear` guard for clear.

Never commit proprietary MPC/MPCX files, official proprietary executables/libraries, credentials, or device-specific secrets.

## Documentation

Keep these documents synchronized with implementation changes:

- `README.md`: user-facing quick start and command usage.
- `FOR_DEVELOPERS.md`: architecture, build/development notes, testing, and implementation status.
- `REVERSE_ENGINEERING.md`: evidence and reconstructed protocol/register behavior.
- `AGENTS.md`: constraints future automated development must preserve.

## Development priorities

1. Preserve verified MPC/MPCX EEPROM programming behavior.
2. Keep IP/MAC handling independent internally while exposing it through the unified CLI.
3. Always report current/EEPROM MAC and IP in the reader and around writer operations.
4. Keep EEPROM as the default IP write destination and `--current` explicit.
5. Reconnect to a newly assigned runtime IP for read-back verification.
6. Refactor temporary wrapper/include structure into shared implementation modules when stable.
7. Add CI builds for Linux and macOS.
