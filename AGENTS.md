# AGENTS.md

## Project goal

Provide small C++11 command-line utilities for SiTCP and SiTCP-XG configuration over RBCP, without requiring the official Windows GUI or Python at runtime.

## Public commands

The intended public command set is:

- `mpc-mpcx-ip-writer`
- `mpc-mpcx-ip-reader`
- `mpc-mpcx-ip-command`
- `sitcp-sitcpxg-ip-writer`
- `sitcp-sitcpxg-ip-reader`

The older `mpc-mpcx-writer`, `mpc-mpcx-reader`, and `mpc-mpcx-command` sources may remain as internal implementation units while the unified commands are being refactored, but they are not public installed names.

Keep command names, option defaults, and output formatting consistent. All `--help` / usage output must show the actual default value whenever an option has a default. For example, show `--port N ... (default: 4660)` and `--timeout SEC ... (default: 3)`.

## Writer CLI

The high-level writer requires an MPC/MPCX file:

```text
mpc-mpcx-ip-writer CURRENT_IP MPC_OR_MPCX_FILE [options]
```

IP rewriting is optional and uses explicit destination-specific options:

- `--set-eeprom-ip IP` sets the EEPROM/default IP address.
- `--set-current-ip IP` sets the current/runtime IP address.
- Do not replace these with an ambiguous `--set-ip` plus a separate destination switch.
- Both IP options may be specified in one invocation.
- When both are specified, perform EEPROM IP writing before current/runtime IP writing so the original address remains reachable until the final network-address-changing operation.
- After changing current/runtime IP, reconnect to the new address and perform read-back verification.

The writer must always program/verify the supplied MPC/MPCX file. IP-only operation through `mpc-mpcx-ip-writer` is not part of the public CLI. Keep the standalone `sitcp-sitcpxg-ip-writer` and `sitcp-sitcpxg-ip-reader` commands for IP-only use.

## Unified CLI, separated internals

MPC/MPCX payload handling and SiTCP IP-register handling may be exposed through the same public commands, but must remain logically separated internally.

- Shared IP register logic lives in `src/ip-config.hpp` or a future equivalent shared module.
- MPC/MPCX payload classification/reconstruction must not be used to determine IP/MAC values.
- IP/MAC register reads must not modify or reinterpret MPC/MPCX license payloads.
- The MPC/MPCX reader must always display current MAC, current IP, EEPROM MAC, and EEPROM IP.
- The MPC/MPCX writer must display those four values before and after the operation whenever the target remains reachable.
- The standalone IP reader/writer must remain available and must not require an MPC/MPCX file.

## Advanced command

`mpc-mpcx-ip-command` is the advanced/diagnostic interface. Preserve the MPC/MPCX and low-level RBCP subcommands `inspect`, `read`, `verify`, `mpcx-plan`, `probe`, `rbcp-read`, `rbcp-write`, and `clear`, and also provide IP read/write operations.

The high-level MPC/MPCX programming path should continue to use the verified writer implementation rather than creating a second divergent destructive programming path.

## Source formatting

Keep C++ source readable and conventionally formatted.

- Do not compress multiple control-flow statements, declarations, or operations onto one long line.
- Put function bodies, `if`/`else` branches, loops, and exception handling on normal separate lines.
- Wrap long expressions and argument lists with consistent indentation.
- Prefer descriptive local variable names in public/refactored code over single-letter names except for small conventional loop indices.
- Existing compressed legacy code should be reformatted as it is touched.
- New code must not introduce minified or one-line C++ formatting.

## Compatibility rules

- Support Linux, macOS, and WSL using POSIX sockets.
- C++11 is the baseline; do not introduce dependencies on C++14/17/20 language or library features without explicit approval.
- In particular, avoid `std::optional`, structured bindings, `std::string_view`, filesystem APIs, and assumptions that `std::string::data()` returns writable `char*`.
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

Default `PREFIX` is `$(CURDIR)/install`, not `/usr/local`. `make install` must work without root privileges and install all five public commands. System installation remains available with `PREFIX=/usr/local`.

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
2. Keep IP/MAC handling independent internally while exposing it through the appropriate CLI.
3. Always report current/EEPROM MAC and IP in the MPC/MPCX reader and around writer operations.
4. Keep `--set-eeprom-ip` and `--set-current-ip` explicit and independent.
5. Reconnect to a newly assigned runtime IP for read-back verification.
6. Preserve the standalone `sitcp-sitcpxg-ip-writer` and `sitcp-sitcpxg-ip-reader` commands.
7. Keep the entire public build compatible with C++11.
8. Refactor temporary wrapper/include structure into shared implementation modules when stable.
9. Reformat compressed legacy C++ as it is touched and keep new code readable.
