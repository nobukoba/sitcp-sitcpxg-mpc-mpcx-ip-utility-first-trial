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

Keep command names, option defaults, and output formatting consistent. All `--help` / usage output must show the actual default value whenever an option has a default. For example, show `--port N ... (default: 4660)` and `--timeout SEC ... (default: 3)`.

## Writer CLI

- --check is a separate read-only mode with optional file; validate a supplied
  payload locally, read FF08/4 and FC10/1 only, report generation/status flag and
  optional file generation match. Never enable/protect/write during Check.
  Exit 7 on a file/device type mismatch; never call that license verification.
  FC10 bit7 is a status flag on normal SiTCP, not its automatic IP endpoint rule.
- Check successful reports use native GUI wording and line order: decoded
  MPC(X) File tag, indented Interface, Device with Matched/Mismatch, EEPROM state.
  Interface is file generation when a file is supplied. Without a file print
  MPC(X) File : Not Selected. and omit Interface and the match suffix.
  Preserve native spelling in the uninitialized message. Keep failure diagnostics
  and exit codes meaningful; fileless Check still succeeds on successful reads.
  Normal SiTCP mismatch ends with `Mismatch.`; XG ends with `Mismatch`.
- --initialize-eeprom requires file programming and forces XG preparation from
  complete safe RAM even when FC10 bit7 is clear. Normal MPC keeps its existing
  automatic rule; native checked-box capture on initialized normal MPC did not
  force a RAM copy. Preserve that explicit generation limitation in help/docs.
- Reject --check with Clear or programming/IP options, and --clear with either
  new option, before access. Force mode must retain default/numeric transfer
  sizing, optional XG extension handling, cleanup, no retries and verification.

The high-level programming mode requires an MPC/MPCX file:

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

The programming mode must always program/verify the supplied MPC/MPCX file.
The explicit `CURRENT_IP --clear` mode requires no file and only erases/verifies
EEPROM FC00..FC7F, restoring protection. Reject file/IP-change combinations
before any device access. Never clear automatically before programming.
IP-only operation through `mpc-mpcx-ip-writer` is not part of the public CLI. Keep the standalone `sitcp-sitcpxg-ip-writer` and `sitcp-sitcpxg-ip-reader` commands for IP-only use.

## Unified CLI, separated internals

MPC/MPCX payload handling and SiTCP IP-register handling may be exposed through the same public commands, but must remain logically separated internally.

- Shared RBCP transport lives in `src/sitcp-sitcpxg-rbcp.hpp`.
- Shared IP/MAC register logic lives in `src/sitcp-sitcpxg-network-config.hpp`.
- Shared MPC/MPCX payload logic lives in `src/sitcp-sitcpxg-mpc-mpcx.hpp`.
- MPC/MPCX payload classification/reconstruction must not be used to determine IP/MAC values.
- IP/MAC register reads must not modify or reinterpret MPC/MPCX license payloads.
- The MPC/MPCX reader must always display current MAC, current IP, EEPROM MAC, and EEPROM IP.
- Keep common MPC/MPCX reader fields in the same order for normal SiTCP and SiTCP-XG. In particular, the `MAC` line must occupy the same output position; generation-specific FC00/FC40 detail lines must not move common fields around.
- MPC/MPCX programming and Clear must display those four values before and after
  the operation whenever the target remains reachable. Check uses its separate
  native-style read-only report and does not take network snapshots.
- The standalone IP reader/writer must remain available and must not require an MPC/MPCX file.

## Advanced command

`mpc-mpcx-ip-command` is the advanced/diagnostic interface. Preserve the MPC/MPCX and low-level RBCP subcommands `inspect`, `mac`, `read`, `verify`, `mpcx-plan`, `probe`, `rbcp-read`, `rbcp-write`, and `clear`, and also provide IP read/write operations.

The high-level MPC/MPCX programming path should continue to use the verified writer implementation rather than creating a second divergent destructive programming path.

## Source formatting

Keep C++ source readable and conventionally formatted.

- Do not compress multiple control-flow statements, declarations, or operations onto one long line.
- Put function bodies, `if`/`else` branches, loops, and exception handling on normal separate lines.
- Wrap long expressions and argument lists with consistent indentation.
- Follow `.clang-format`'s 4-space indentation and 100-column limit. Keep a
  final newline, consistent line endings within each file and no trailing spaces.
- Splitting source string literals must preserve output text, section blank
  lines and raw dumps; source wrapping must not introduce display newlines.
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
- Determine the target generation first from the documented read-only SiTCP-XG Identifier register at `0xFFFFFF08..0xFFFFFF0B`; only an exact value of `0x58544350` identifies SiTCP-XG. Keep generation detection separate from MPC/MPCX payload classification.
- Do not use the former experimental `0xFFFFFF50` probe as a SiTCP-XG generation identifier.
- Do not infer the device generation solely from FC00/FC40 payload-looking data: verified hardware shows stale/alternate-layout data can make both reconstructed payloads classify as valid.
- High-level EEPROM writes must attempt to restore write protection after
  failures. A network outage may prevent restoration; never promise success
  when cleanup or read-back cannot be completed.
- Read-back verification is mandatory after high-level configuration writes.
  Advanced rbcp-write remains a raw diagnostic transaction; it does not perform
  automatic read-back or EEPROM protection handling.
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

Default `PREFIX` is `$(CURDIR)`, not `/usr/local`. `make install` must work without root privileges and install all five public commands. System installation remains available with `PREFIX=/usr/local`.

README usage examples must consistently use the default installed path
`./bin/`. Explain `./src/` separately as build output for development.
Document that `make` alone does not refresh installed copies and that
`make install` rebuilds and copies them after source updates.
`make clean` removes only the five generated executables in `src/`; it must
never remove the source directory or installed binaries.

## Safety

- Shared clear operations must leave at least 500 ms between write starts,
  including enable, each 16-byte block and protection restoration on success or
  failure. Use a monotonic clock; ACK time counts toward the interval. Retain
  mandatory read-back and do not retry erase writes.
  This is the timing requirement, not a verified wire guarantee: the 2026-10-15
  Cygwin capture contains gaps below 500 ms. Preserve that unresolved qualification
  until measured wire timing is explained or corrected.

`mpc-mpcx-ip-command rbcp-write` and `clear` are intentionally low-level/destructive. Keep explicit command names and the `--yes-really-clear` guard for the advanced clear command.
Writer `--clear` is an explicit standalone clear-only request and needs no
additional guard. Both entry points share `sitcp-sitcpxg-eeprom-clear.hpp`.

Never commit proprietary MPC/MPCX files, official proprietary executables/libraries, credentials, or device-specific secrets.

## Documentation

Keep these documents synchronized with implementation changes:

- `README.md` and `README.ja.md`: English and Japanese user-facing quick start and command usage. Keep both versions synchronized, preserve identical command examples, and maintain the `Language: English | 日本語` navigation at the top of each file.
- `FOR_DEVELOPERS.md`: current architecture, build/test instructions, option contracts,
  register behavior, native differences and known limits. Link to dated evidence.
- `docs/verification/`: sanitized dated test/investigation records, indexed by
  [verification records](docs/verification/README.md). Include target/tool/revision,
  conditions, results, restoration and untested paths. Preserve earlier evidence
  when implementation changes; never present an old run as validation of new code.
- Keep license-bearing raw logs/captures, EEPROM backups and proprietary tools
  outside Git. Markdown verification records are distinct from private raw evidence.
- `AGENTS.md`: constraints future automated development must preserve.

Keep README content focused on installation and command usage, with direct Bee Beans Technologies reference links. Put source-formatting rules, shared-header architecture, register maps, and implementation provenance in `FOR_DEVELOPERS.md` or `AGENTS.md`, not the READMEs.

Preserve all unique information when reorganizing documentation: move technical content to its canonical developer section and replace duplicated explanations with links. README translations must retain the same user-facing instructions. Keep detailed byte maps, initialization algorithms, generation detection, diagnostic internals, evidence, and formatting constraints out of README. Preserve user-visible warnings, option semantics, examples, and exit codes there.

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

## Diagnostic report consistency

- Show General, TCP, Client/server, Control and Transmission groups for both
  normal SiTCP and XG. Retain decimal/hexadecimal values, raw dumps and partial
  read handling. The standalone IP reader and compact writer output stay compact.
- Normal server fields start at 30/36/3A; XG uses 32/38/3C. Normal disconnect
  timeout is N*256 ms; XG is (N+1)*256 ms. Normal 2C is reconnect interval in
  ms; XG 2C is MSL in 0.5 ms units. Decode control bits per generation.
- Show normal transmission-rate setting as unsupported, without reading FF40
  or interpreting its EEPROM license bytes as a rate. Do not present an assumed
  link speed as a measurement. XG rate is a configured line-rate limit.

- Select diagnostic runtime length by the exact XG identifier, not file suffix
  or EEPROM payload: normal SiTCP reads FF00..FF3F (64 bytes), XG reads
  FF00..FF4F (80 bytes). Never request FF40..FF4F for normal SiTCP reports.
- EEPROM stays FC00..FC4F (80 bytes) for both generations; normal MPC needs
  its FC40..FC4F license data. Keep the two regions separately labeled with
  the actual range/length; COMPLETE means all bytes in those selected ranges.
- If generation detection times out, fail before reading a guessed range.
- Share detailed report logic across the MPC/MPCX reader and advanced read/ip-read.
- Writer success output must stay within five nonempty lines: two compact MAC/IP lines
  before, two after, a blank line, then exactly
  `Success! All operations completed and verified.`, followed by a blank line.
  Align labels using one space after `before:` and two after `after:`. Only print success after
  all requested operations and verification succeed; omit target/status metadata. Apply this to MPC/MPCX programming,
  writer --clear, standalone IP writer, and advanced ip-write. Use shared
  network snapshots instead of full diagnostic dumps; preserve verification
  and failure reporting. Reader output remains detailed.
- Display decoded numeric register values in decimal and hexadecimal together;
  retain units and conventional IP/MAC notation. Append eight uppercase hex
  digits in network byte order to displayed register IPs, including server IP
  and compact IP-only views: `192.168.10.10 (0xC0A80A0A)`. Raw dumps remain hex.
- Decode each region from its own bytes. Only apply the XG parameter map after
  exact identifier detection, never to normal SiTCP license bytes.
- Diagnostic block bus errors must fall back to byte reads, preserve readable
  data, mark rejected bytes `??`, and report their addresses. Decode only fields
  whose bytes are all readable. Read views return 3 for partial reports; never
  invent bytes or claim a complete read. Timeouts/short replies remain fatal.
- Partial diagnostic bus errors must not block the existing verified writer
  path. Keep standalone IP-only commands compact.
- Run `python3 tests/test_register_report.py` after changing diagnostic reports.

## MPCX EEPROM initialization

- Use `src/sitcp-sitcpxg-eeprom-init.hpp` for both writer image preparation and
  read-only `mpcx-plan`. Validate the file and exact XG identifier before use.
- EEPROM FC10 bit7 selects automatic initialization, including FF after clear;
  explicit --initialize-eeprom also selects it. Do not use
  license classification, rate validity, or an all-FF-image test instead.
- If initialization is needed, require a complete runtime FF00..FF4F image,
  verify its XG identifier and reject a set runtime RESET bit. Overlay MPCX
  bytes at FC00..FC0F and FC12..FC17; retain RAM at FC10..FC11 and FC18..FC4F.
  Write/read-back verify the resulting 80 bytes, including FC40..FC4F.
- If FC10 bit7 is clear and --initialize-eeprom is absent, keep the 24-byte MPCX
  programming path and preserve EEPROM configuration. Do not replace an existing
  rate because it looks odd.
- Never program diagnostic placeholder bytes or mix partial RAM with invented
  defaults. No verified MPCX default image exists in our evidence. The official
  normal-SiTCP fallback table must not be reused for SiTCP-XG.
- Official Writer 0.4.1-2-gc782 XG preflight was checked with bus-error and
  short FF00/80 replies: zero writes; static length-mismatch path returns failure
  without a fixed table. Do not use one board's runtime/EEPROM settings as proof
  of an official XG fallback. See the 2026-10-15 developer investigation.
- During XG initialization only, attempt 16-byte runtime reads at FF50, FF60, FF70 after the base image write. Copy each complete readable block to the corresponding EEPROM block; skip failed reads without changing that block. Extension writes are never optional failures: restore protection and fail, without retry. Verify every copied block. Keep success output unchanged; do not claim timing equivalence.
- Always prepare the complete mandatory base image before enabling writes.
  Optional extension source reads occur later, after the base data write. Attempt protection
  restoration even after an ambiguous enable ACK; never retry data writes.
- Keep EEPROM IP overrides after image programming, and runtime IP changes
  last with reconnect/read-back verification. Never clear implicitly.
- Keep public documentation, static-analysis findings, and physical-device
  verification distinct. Run `python3 -m unittest discover -s tests -v`.

## Normal MPC programming compatibility

- Normal MPC must use a separate FC10..FC4F (64-byte) record, after independent
  generation detection. Do not read or program FC00..FC0F for this writer path.
- Match the captured legacy writer: control FC10 is always 01, FC11 is retained
  except that the fixed-default fallback initializes it to 00,
  payload MAC maps to FC12..FC17 and the remaining 16 bytes to FC40..FC4F.
- Normal initialization is selected when both EEPROM IP endpoint octets
  FC18 and FC1B are FF. This is not the MPCX FC10-bit7 condition.
- On normal-MPC initialization, try complete runtime FF18..FF3F first. On read
  failure use the exact official 40-byte fallback table by default, as requested
  by the user. --no-fallback must abort before writes instead. Never apply this
  normal-SiTCP table to XG, generation detection, snapshots or IP-only writes.
- By default issue the captured single 64-byte normal-MPC data write at FC10; an explicit chunk size may split it. Retain
  protection cleanup, no destructive retries, and full read-back verification.
- MPC/MPCX programming both wait 500 ms after each successful enable/data/protect
  ACK, including numeric chunks and copied optional extensions. After an ambiguous
  enable/data failure wait 500 ms before attempting protection; never retry data.
  This user-requested pacing supersedes the earlier ACK-paced XG behavior.
- Preserve MPCX image preparation. Default programming is one complete image write; explicit `--write-chunk-size 16` selects the previous 16-byte chunking.
- The checked-in official reference contains synthetic data only. Never replace
  it with a proprietary license or commit the official executable/disassembly.
- IP-only EEPROM writes must attempt protection restoration even after a lost
  enable ACK; keep the enable operation inside the cleanup scope.
- Run `python3 -m unittest discover -s tests -v` after changing these paths.

## Utility-compatible IP programming

- User-requested official Utility compatibility supersedes the previous four-byte IP write policy. Use the shared network helper for all IP write entry points.
- Read FC00/64 before EEPROM writes; enable FCFF=00, write FC00/17 and FC18/40, restore FCFF=FF, and verify the entire 64-byte image.
- Read FF00/64 twice before runtime writes; write FF00/17, then FF18/40, reconnect to the requested IP and verify all 64 bytes.
- Replace only IP bytes by default. The standalone IP writer may explicitly set TCP data port (--tcp-port) and UDP/RBCP port (--udp-port) in the same captured settings image. Preserve every other value, including unchanged license bytes in the EEPROM prefix. Do not parse license payloads or require license files. Leave offsets 11..17 and regions beyond 3F unchanged.
- Abort on incomplete settings reads before writes. Never retry configuration writes. A runtime prefix failure is fatal; a timeout of the final address-changing block permits read-back at the new IP. Always attempt EEPROM protection cleanup after an enable/data failure.
- Keep documentation honest: simulator agreement does not establish real-device IP migration or complete packet-for-packet GUI equivalence.

- Reader TCP parameter labels must match the confirmed official Utility strings; keep the Keep Alive enable flag next to both buffer-state timers. Preserve decimal/hex and use a 36-character settings-label column. Compact writer formatting is unchanged.

## MPC/MPCX transfer-size option

- Default `--write-chunk-size` is automatic whole-image programming: normal
  MPC 64 bytes at FC10, initialized MPCX 24 bytes at FC00, initialization MPCX
  80 bytes at FC00. Do not expand the preservation image or change its bytes.
- Accept the explicit value official for the default whole-image lengths.
  Otherwise N must be a complete positive decimal integer in 1..255 bytes.
  Reject any explicit sizing option, including official, and --no-fallback with
  --clear before access. Document current defaults, examples and restrictions in
  the developer option reference; README keeps sizing details out.
  `--write-chunk-size 16` selects 16-byte writes with a shorter final block.
- Reject invalid sizes, missing values and clear/size combinations before
  any device access. Forward the option through the high-level writer wrapper.
- Keep image validation, generation checks, mandatory complete read-back,
  protection cleanup, IP override order and no destructive retries in both modes.
- Chunk size affects MPC/MPCX data programming only, not IP or clear sequences.

## Standalone Utility port options

- --tcp-port and --udp-port accept complete decimal integers in 1..65535;
  omitted values preserve the existing setting. --port is the transport port
  used to reach the device, not a request to change its stored port.
- Keep native 17+40-byte settings writes for both generations and destinations.
  Change only IP at 18..1B and explicitly requested TCP at 1C..1D / UDP at 22..23.
- After runtime UDP changes, verification and the final compact snapshot must
  use the requested UDP port; EEPROM UDP changes keep the old runtime endpoint.
- Do not add port options to MPC programming or advanced commands implicitly.
  Preserve existing CLI behavior and compact success formatting.

## Documentation placement requested by the user

- Keep README introductions and main sections focused on installation and basic usage.
- Place TCP/UDP port options and examples only in the README appendix.
- Keep transfer chunk sizing (including 16-byte examples) in CLI help and the
  developer guide, not README, including its option list.
- Keep decimal/hex formatting, raw-byte layout, timing, register maps, detailed
  algorithms and evidence in FOR_DEVELOPERS.md, not README.
- Mark historical implementation/test statements explicitly. Current summaries
  must reflect later physical verification and the current whole-image default.
- Preserve identical English/Japanese command examples and valid heading links.
