# For developers

This guide describes current design, CLI contracts, register maps and known
limitations. Start with [implementation status](#current-implementation-and-verification-status-2026-10-15),
[option details](#additional-options-reference) and
[native differences](#official-versus-custom-comparison-current-conclusions-2026-10-15).

Test runs and investigation history are kept separately in
[verification records](docs/verification/README.md). Raw license files,
captures and device backups stay outside the source repository.

## Check and forced initialization options

`mpc-mpcx-ip-writer IP [FILE] --check [--port N] [--timeout SEC]` is a
read-only mode, disabled by default. Validate an optional 22-byte payload before
network access, then read FF08/4 and FC10/1. Print the decoded file tag, optional
Interface and generation match, Device, and EEPROM status in native GUI order.
Without a file print `MPC(X) File : Not Selected.` and omit Interface/match.
Do not send snapshots, enable, protect or write commands in this mode.
Exit 0 means successful reads and optional match, 7 means generation mismatch,
1 means file/read/option error, and missing required arguments use exit 2.
This checks generation/status, not license validity or installed payload equality.

FC10 bit7 controls Check's initialization report for both generations. Normal
MPC programming uses its separate saved-IP endpoint rule. Preserve the native
uninitialized message `EEOROM needs to be inbitialized.` and the normal-only
period in `Mismatch.`; XG uses `Mismatch` without a period. GUI icons and modal
framing are not reproduced; successful fileless CLI reads exit 0.

`--initialize-eeprom` is a programming option, disabled by default, requiring
a file. On XG it forces an 80-byte RAM-derived image even when FC10 bit7 is
clear; validate the complete identifier/control image before enabling writes.
It does not Clear first. Overlay the selected file, verify every programmed
base/extension byte and preserve optional-extension failure policy. Required
XG RAM failure is fatal; no XG fixed fallback exists in the reviewed evidence.
On normal MPC this option has no additional effect, matching the tested native
checkbox: retain the automatic saved-IP rule and normal fallback policy.

Reject Check combined with Clear or programming/IP options before access.
Clear rejects initialization and all other programming options. Advanced
`mpcx-plan` and standalone IP commands gain neither new flag. Native Check
with its Initialize box checked adds a forced-initialization line; the CLI
intentionally rejects that mode combination. Programming still permits saved
IP overrides followed by current-IP overrides, with verification and cleanup.

File programming waits 500 ms after each enable/data/extension/protection ACK,
and 500 ms before protection cleanup after an ambiguous enable/data failure.
Do not retry destructive writes. Clear and standalone IP writing retain their
separate timing policies; elapsed-time equivalence is not claimed.

```bash
./bin/mpc-mpcx-ip-writer DEVICE_IP --check
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx --check
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx --initialize-eeprom
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx --initialize-eeprom \
  --write-chunk-size official --set-eeprom-ip 192.168.10.11
```

See the [verification index](docs/verification/README.md) for native reports,
normal/XG forced-image comparisons, pacing captures and validation revisions.

## Official-versus-custom comparison: current conclusions (2026-10-15)

The following summarizes current behavior and the scope of its evidence.
Use the [verification index](docs/verification/README.md) to select the tested
revision and board: [LRTDC normal](docs/verification/2026-10-15-lrtdc-normal.md),
[earlier normal AMANEQ](docs/verification/2026-10-15-amaneq-normal.md),
[updated AMANEQ XG](docs/verification/2026-10-15-amaneq-xg.md) and
[later XG mode rechecks](docs/verification/2026-10-15-xg-modes.md).
An older successful run is not validation of a later source change.

| Item | Official tools | Current custom tools | Evidence and limits |
| --- | --- | --- | --- |
| Normal MPC preservation/initialization, Clear, EEPROM and runtime settings | Reference write sequences | Same addresses, lengths, payloads and final images in five paired operations | Real normal-SiTCP board; full starting/final RAM 64 and EEPROM 128 bytes compared |
| Read-back | No equivalent complete post-write verification observed in the compared successful sequences | Mandatory read-back; extra generation/snapshot/verification reads | Write equality does not imply all packets are identical |
| Normal MPC fallback | Confirmed fixed table after short RAM reply, GUI confirmation before programming | Same fixed table by default; --no-fallback stops before writes | Synthetic reference plus real LRTDC through a short-response proxy: exact FC10/64 and full EEPROM/128 equality; healthy physical RAM |
| FC11 during fallback | 00 after zeroing prepared record | Corrected to 00; other normal paths retain EEPROM FC11 | First custom fallback implementation retained FF; discrepancy fixed and regression added |
| XG mandatory RAM read failure | Zero writes for bus-error and short FF00/80 replies; static failure branch has no fixed-table substitution | Stops; normal fallback table is never used for XG | Synthetic native execution and static analysis of Writer 0.4.1-2-gc782; no real-device RAM fault induced |
| Enable ACK loss | No data, retry, protection write or error dialog during roughly 108 seconds observed | Bounded failure; protection restoration attempted and acknowledged in tested cases | Normal real-device proxy test; no claim about all native failure paths |
| Transfer length | Normal MPC 64; preserved MPCX 24; initialized MPCX 80 | Same default; explicit --write-chunk-size official; optional numeric chunks | Matching write images; changing transfer size is not a claim of identical timing |
| Clear pacing | Earlier normal capture 501.569..516.942 ms | Earlier normal 492.171..514.823 ms; current LRTDC 489.684..511.392 ms despite monotonic 500 ms scheduling | Strict wire minimum remains unresolved; clocks/scheduling/capture effects not isolated |
| MPC/MPCX programming pacing | Native intervals vary by path, ACK latency and GUI interaction | Wait 500 ms after each enable/data/protect ACK and before failure cleanup | New XG force capture: 502.312 ms enable-to-data, 1765.535 ms data-to-protect; elapsed-time equivalence is not claimed |
| Check | File optional; fileless output says Not Selected. with no Interface or match suffix | Same report text/order, including normal-only Mismatch period; two read requests, no writes; meaningful CLI exit codes | Fileless native XG and normal hardware inspected; LRTDC fileless/matched/mismatched/erased pairs verified |
| Forced initialization | XG checkbox copies RAM even when initialized; checked normal MPC retained its automatic rule | --initialize-eeprom follows those generation-specific paths | Synthetic references plus real XG and normal LRTDC full-image/exact-write comparisons; Check combined with force is CLI-invalid |
| Reader output | Native GUI settings | Detailed raw bytes, decimal/hex and settings retained | More detailed diagnostics are intentional; do not remove information for visual compatibility |
| Power-cycle outage | No controlled attribution established | One outage after runtime migration; recovered by existing-flash JTAG reload | Six follow-up cycles did not reproduce; cause remains undetermined |

See the [fallback investigation](docs/verification/2026-10-15-fallback.md) for
the FC11 correction and its validation. Native fallback execution covers the
short-reply case; native bus-error/timeout fallback behavior is not established
by the custom simulator tests.

### Evidence locations and unresolved work

- External `normal-comprehensive-test.json` report:
  17 cases, five paired comparisons, ACK-loss observations and recovery.
- External `boot-reproduction.json` report: three
  baseline and three runtime-migration power cycles, no reproduction.
- External `xg-fallback-investigation.json` report: installed
  official executable hash, two valid synthetic failure traces and static path.
- [Synthetic native MPC fallback reference](tests/reference/official_mpc_fallback.json):
  complete expected data write and EEPROM image; suitable for repository tests.

The first three reports are external session deliverables and may be absent
from an ordinary source checkout. Real licensed captures, proprietary native
binaries, disassembly and device backups remain outside Git in private work
storage. No actual license file or license-bearing capture belongs in a public
source archive. The fallback fixture contains synthetic data only.

Remaining investigations: reproduce the power-cycle outage while preserving
pre-recovery evidence; separate Clear client-clock timing from wire timing;
extend native failure testing; verify readable optional extensions and additional
XG firmware/boards. XG runtime migration and post-boot acceptance are now covered
by the [updated AMANEQ comparison](docs/verification/2026-10-15-amaneq-xg.md),
not by a new test on 192.168.2.187.
None of these unresolved items should be described as confirmed SiTCP bugs.

## Additional options reference

This section describes the current CLI. Dated evidence sections describe the
version tested at that time. Sizes are bytes, not bits or 16-bit words.
Run `make install` after updating sources to refresh installed binaries.

### MPC/MPCX writer options

| Option | Default | Scope and behavior |
| --- | --- | --- |
| --check | Off | Read-only target/status flag check; optional file generation match; separate mode, no programming options |
| --initialize-eeprom | Off | Force XG RAM-to-EEPROM initialization even if already initialized; normal MPC behavior unchanged, matching the observed native checkbox |
| --no-fallback | Off: normal-MPC fixed fallback enabled | Stop before writes if the normal initialization FF18/40 read fails; no effect on successful reads or records needing no initialization |
| --write-chunk-size official | official, also when omitted | One complete prepared base image; native lengths below, not native GUI delays or all native packets |
| --write-chunk-size N | No numeric override | Complete decimal integer 1..255; data blocks of at most N bytes with a shorter final block; never pad the image |
| --set-eeprom-ip IP | No override | Change saved IP after file programming/verification; preserve ports |
| --set-current-ip IP | No override | Last operation; change active IP, reconnect and verify; preserve ports |
| --clear | Off | File-free erase/verify FC00..FC7F; never clear implicitly |
| --port N | 4660 | Current RBCP destination; does not rewrite a port |
| --timeout SEC | 3 seconds per transaction | Shared transport accepts finite (0,86400]; not a total operation deadline |
| -h, --help | Off | Show help; standalone --help exits 0 |

| Prepared base image | Address | official/default | N=16 |
| --- | --- | --- | --- |
| Normal MPC, including initialization/fallback | FC10 | 64 | 16+16+16+16 |
| MPCX, already initialized / preserve settings | FC00 | 24 | 16+8 |
| MPCX, initialization required | FC00 | 80 | 16+16+16+16+16 |

Normal fallback is considered only when saved IP endpoint octets FC18 and FC1B
are both FF. Complete RAM settings are tried first. Timeout reads use up to
three total attempts; bus errors/short replies are not retried by that helper.
On failure, default behavior uses the complete native 40-byte settings table
(saved IP 192.168.10.16, TCP 24, UDP 4660), zeroes FC11 and overlays the selected
MAC/license. A successful RAM read preserves FC11. --no-fallback stops instead.
This flag neither creates an XG fallback nor disables optional extension reads.
Generation detection, initial MAC/IP snapshots, EEPROM reads and final
verification remain mandatory. Fallback cannot make an unreachable board writable.

For XG initialization, optional FF50/60/70 source reads remain 16 bytes each.
The default writes each readable block as 16 bytes; numeric N may split that
block. Chunk size changes neither Clear's fixed 16-byte blocks nor IP writes'
17+40-byte blocks. MPC/MPCX file programming now waits 500 ms after each chunk
ACK, independently of chunk sizing. Every programmed base/extension byte is verified.

Either transfer mode and --no-fallback can be combined with IP overrides.
Order is file programming/verification, saved IP override, active IP override.
An explicit saved IP therefore replaces the fallback IP after programming.
These stages are not atomic: earlier writes remain applied if a later stage
fails. Cleanup is best effort if communication is lost; never retry data writes
blindly. Normal fallback runs without a separate CLI confirmation prompt.
--clear rejects a file, IP overrides, --no-fallback and any explicit chunk-size
option (including official) before device access. Invalid/missing chunk sizes
also fail before access.

```bash
# Explicit native-length default.
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpc --write-chunk-size official

# Forbid normal-MPC fallback; no effect on ordinary successful RAM reads.
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpc --no-fallback

# Optional split data writes with the same final image.
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx --write-chunk-size 16

# Save a chosen IP after programming, including after normal fallback.
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpc \
  --write-chunk-size official --set-eeprom-ip 192.168.10.17
```

### Standalone IP writer options

| Option | Default | Scope and behavior |
| --- | --- | --- |
| --eeprom | Selected if neither mode is specified | Save settings; active endpoint unchanged |
| --current | Off | Change active settings; reconnect to NEW_IP and requested UDP port and verify |
| --tcp-port N | Preserve destination-image value | TCP data port; complete decimal integer 1..65535 |
| --udp-port N | Preserve destination-image value | UDP/RBCP port; complete decimal integer 1..65535 |
| --port N | 4660 | Existing communication port, independent of new --udp-port |
| --timeout SEC | 3 seconds per transaction | Same shared transport bounds; read retries extend total duration |
| -h, --help | Off | Show help |

--eeprom and --current are mutually exclusive. NEW_IP remains required for
port edits: specify the existing IP of the destination image to keep it.
Omitted port values come from EEPROM for --eeprom and RAM for --current.
Other settings and license bytes are preserved. Port-edit options belong only
to the standalone IP writer, not the MPC wrapper or advanced ip-write.
Neither --no-fallback nor --write-chunk-size is an IP-only option. EEPROM
read-back uses CURRENT_IP and --port; it alone does not prove post-boot loading.

```bash
# Keep the saved IP at 192.168.10.17 while changing saved ports.
./bin/sitcp-sitcpxg-ip-writer DEVICE_IP 192.168.10.17 \
  --eeprom --tcp-port 25 --udp-port 4661

# Reach UDP 4660, then verify at the new IP and UDP 4661.
./bin/sitcp-sitcpxg-ip-writer DEVICE_IP 192.168.10.18 \
  --current --port 4660 --tcp-port 25 --udp-port 4661
```

Readers and advanced read/ip-read retain their --port/--timeout options;
advanced raw writes do not gain automatic verification/protection. See the
[advanced syntax](#advanced-command-reference) and
[current comparison](#official-versus-custom-comparison-current-conclusions-2026-10-15)
for evidence limits and deliberate differences from the native tools.

## Scope

This repository contains two related but functionally separate C++11 utility groups for SiTCP / SiTCP-XG:

1. MPC/MPCX license/configuration utilities.
2. SiTCP Utility compatible IP-address utilities, with optional TCP/UDP port changes in the standalone writer.

They may share low-level RBCP transport code, but they must not share domain logic. Keep three evidence classes distinct when documenting behavior: public Bee Beans Technologies documentation, static analysis of official utilities, and controlled verification with real files/hardware.

## Current implementation and verification status (2026-10-15)

Current defaults are whole-image data writes: normal MPC 64 bytes, preserved
MPCX 24 bytes, initialized MPCX 80 bytes. Explicit numeric chunks are optional.
XG initialization attempts optional FF50/60/70 reads and copies only complete
blocks. Check is read-only with an optional file. Forced initialization affects
XG; normal MPC retains its saved-IP rule and default fixed fallback.
Every high-level programmed byte is read back; destructive writes are not retried.

File programming waits 500 ms after write ACKs. Clear schedules 500 ms between
client write starts, but captured Cygwin wire intervals can be shorter. A strict
500 ms wire minimum remains unresolved. IP Utility writes retain their own
ACK-paced sequence. Matching write data does not imply identical traffic/timing.

Normal and XG have physical native/custom write-image comparisons. Boot and
runtime migration evidence identifies the tested board/revision; LRTDC was not
power-cycled in its latest pass. Readable XG extensions remain simulator-only.
The earlier normal-SiTCP startup outage has no established cause and was not
reproduced in the recorded follow-up. Native failure behavior is only partially
characterized; cleanup and bounded CLI failures are deliberate differences.

Detailed test conditions, revisions, counts, timings, restoration and limitations
are in [verification records](docs/verification/README.md). Those records retain
historical observations; later source changes do not alter what was tested.
Raw captures, license files and device backups remain private outside Git.

## Build and compatibility

Build requirements are a C++11 compiler (`g++` or `clang++`), POSIX sockets, and `make`.


```bash
make
make install
```

The default build uses `-std=c++11`. Do not introduce C++14/17/20-only features without explicitly raising the minimum requirement. In particular, avoid `std::optional`, `std::string_view`, `std::filesystem`, structured bindings, `if constexpr`, and assumptions that `std::string::data()` is writable.

The build produces `./src/`; developers can run those binaries directly.
`make install` depends on the build and copies the five programs into
`$(BINDIR)`, defaulting to `$(PREFIX)/bin`, with
`PREFIX=$(CURDIR)`. After source updates, run `make install` to
refresh installed copies; `make` alone only updates `./src/`.
README usage examples consistently use the installed copy.

Default installation is local to the checkout:

| Command | Output location | How to run |
| --- | --- | --- |
| `make` | `./src/` (build output) | `./src/mpc-mpcx-ip-reader DEVICE_IP` |
| `make install` | `./bin/` (installed copy) | `./bin/mpc-mpcx-ip-reader DEVICE_IP` |

Override with, for example:

```bash
make install PREFIX=$HOME/.local
sudo make install PREFIX=/usr/local
```

Targets are Linux, macOS, and WSL using POSIX sockets. Both `src/` and `bin/` contain the same five commands after successful installation. `make clean` removes generated executables from `src/` but retains source files and installed copies.

For development without installation, replace the README's `./bin/` prefix with
`./src/`, for example `./src/mpc-mpcx-ip-reader DEVICE_IP`.

## Current programs

- `mpc-mpcx-ip-writer` — file programming with optional saved/current IP changes;
  separate file-free `--clear` and optional-file read-only `--check` modes.
- `mpc-mpcx-ip-reader` — MPC/MPCX reader; also reports current/EEPROM MAC and IP values.
- `mpc-mpcx-ip-command` — advanced/diagnostic interface with MPC/MPCX, IP, and low-level RBCP operations.
- `sitcp-sitcpxg-ip-reader` — IP-only reader, independent of MPC/MPCX payload decoding.
- `sitcp-sitcpxg-ip-writer` — IP writer using the official Utility setting-block sequence; no license decoding; only IP and explicitly requested TCP/UDP ports change.

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
  optional TCP data / UDP-RBCP port changes in the standalone writer
  reconnect/read-back after runtime IP or UDP-port change
```

A common RBCP transport helper may be shared, but MPC/MPCX payload logic must not leak into the standalone IP-only commands.

## Source formatting

See [AGENTS.md — Source formatting](AGENTS.md#source-formatting) for the complete formatting rules.

## Implementation notes

The public commands use shared transport/network/MPC-MPCX code in `src/sitcp-sitcpxg-rbcp.hpp`, `src/sitcp-sitcpxg-network-config.hpp`, and `src/sitcp-sitcpxg-mpc-mpcx.hpp`. MPC/MPCX payload handling and IP register handling remain logically separated internally even though some commands expose both functions. Do not create a generic abstraction that silently mixes MPC/MPCX license layout with IP configuration layout.

IP/MAC register addresses used by the implementation are:

```text
current MAC : 0xFFFFFF12..0xFFFFFF17
current IP  : 0xFFFFFF18..0xFFFFFF1B
EEPROM MAC  : 0xFFFFFC12..0xFFFFFC17
EEPROM IP   : 0xFFFFFC18..0xFFFFFC1B
EEPROM WE   : 0xFFFFFCFF
```

This is an experimental implementation and is not an official Bee Beans Technologies utility. Proprietary executables, libraries, and user-specific MPC/MPCX files are not included.


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
FC10..FC11     -> preserve EEPROM bytes unless initialization is required
payload[16:22] -> FC12..FC17
```

The final six payload bytes correspond to the target MAC in the verified pair.
FC10..FC11 are not fixed constants: preserve EEPROM for ordinary programming;
use runtime bytes for initialization (see the initialization section below).

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

### Normal MPC prepared record

Normal MPC programs FC10..FC4F as a separate 64-byte record. Control FC10 is
always 01. FC11 is retained except on fixed fallback, which zeroes the complete
record before applying the settings table and file overlays. Saved IP endpoint
bytes FC18 and FC1B both equal to FF select initialization: first try complete
runtime FF18/40, otherwise use the official 40-byte table unless --no-fallback.
Never mix a partial RAM read into defaults. MAC maps to FC12..FC17 and license
data to FC40..FC4F. FC00..FC0F and FC50..FC7F remain outside this write window.
See the [fixed table and execution evidence](docs/verification/2026-10-15-fallback.md)
for the recovered values and their verification limits.

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

According to SiTCP-XG Manual 1.8.1, disconnect timeout value `N` represents
`(N + 1) * 256 ms`. The documented transmission-rate range is 1..10000
Mbps in 1-Mbps units. Values outside that range should be reported as such,
not interpreted as a normal configured rate.

For MPCX programming on an already initialized EEPROM, the verified 22-byte payload mapping changes only
the following bytes when --initialize-eeprom is absent:
`FC00..FC0F` and `FC12..FC17`; `FC10..FC11` are preserved. The writer
must also preserve `FC18` and later configuration bytes. In particular,
different values observed at FC20/FC2A/FC40 on different SiTCP-XG boards are
not evidence of a bad MPCX payload.

## MPC/MPCX write sequence

When both IP options are given, EEPROM IP is written first and current/runtime IP last, preserving reachability at the original IP until all earlier operations finish. After runtime IP changes, reconnect to the new IP and verify by read-back. A lost acknowledgement may mean the IP has already changed; do not blindly retry the write.


1. Independently detect the target generation, then read its EEPROM programming record.
2. For MPCX, prepare either the 24-byte preservation image or the complete
   80-byte RAM initialization image as described below. For normal MPC, prepare
   the separate 64-byte FC10 record described in the official-writer comparison.
3. Release EEPROM write protection.
4. By default write the complete MPC/MPCX image in one transfer. Explicit
   `--write-chunk-size 16` selects 16-byte blocks; image contents/ranges are unchanged.
5. For XG initialization only, read FF50/60/70 in 16-byte blocks and copy each
   complete readable block to FC50/60/70; skip unreadable source blocks. Restore
   protection, including error paths where possible.
6. Read the programmed base image and every copied extension block again.
7. Compare byte-for-byte and fail on mismatch.

## Advanced command reference

Important subcommands include (MPC_OR_MPCX_FILE means a `.mpc` or `.mpcx` license/configuration file):

```text
inspect MPC_OR_MPCX_FILE
mac MPC_OR_MPCX_FILE
read IP [--port N] [--timeout SEC]
verify IP FILE [--port N] [--timeout SEC]
mpcx-plan IP FILE [--port N] [--timeout SEC]
probe IP ADDRESS [LENGTH] [--port N] [--timeout SEC]
rbcp-read IP ADDRESS LENGTH [--port N] [--timeout SEC]
rbcp-write IP ADDRESS HEX-BYTES [--port N] [--timeout SEC]
clear IP --yes-really-clear [--port N] [--timeout SEC]
ip-read IP [--port N] [--timeout SEC]
ip-write CURRENT_IP NEW_IP [--eeprom|--current] [--port N] [--timeout SEC]
```

`read` and `ip-read` display the same full runtime/EEPROM report. `ip-write` and the standalone IP writer print before/after MAC/IP and result summaries. The standalone IP reader retains its compact MAC/IP view. `ip-write` defaults to EEPROM and accepts `--current` for the runtime/current address.

Raw `rbcp-write` is a single diagnostic write transaction, not a high-level
configuration operation: it does not automatically restore EEPROM protection
or perform read-back. The clear guard applies to advanced `clear`, not arbitrary
`rbcp-write`. High-level writers and clear retain their verification/cleanup.

## IP utility design

The standalone IP-only CLI is:

```text
sitcp-sitcpxg-ip-reader CURRENT_IP [options]
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

The IP tools must not classify, reconstruct, or validate MPC/MPCX license payloads. Utility-compatible IP writes include an unchanged settings prefix; they must not alter the values of license bytes.

Requirements:

- reader reports current/runtime and EEPROM/default network configuration;
- writer changes IP and, only when explicitly requested in the standalone CLI, TCP/UDP ports;
- EEPROM is the default write target;
- changing current/runtime IP requires an explicit option;
- writer performs read-back verification;
- after changing current/runtime IP or UDP port, reconnect to the new endpoint and require complete read-back verification;
- normal SiTCP and SiTCP-XG behavior must be tested independently.

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
| Current/runtime IP/MAC read | register map | implemented | both generations |
| EEPROM/default IP/MAC read | register map | implemented | both generations |
| EEPROM IP write | guide distinguishes EEPROM access | native 17+40 sequence | both generations; XG actual IP change |
| EEPROM TCP/UDP port write | General-tab settings | native 17+40 sequence | both generations; actual port change |
| Runtime IP/UDP write and reconnect | current access described | captured sequence; simulator migration | normal SiTCP and updated AMANEQ XG verified; other firmware untested |

## Remaining work

### MPC/MPCX

- verify exact semantic meaning of all license bytes;
- investigate remaining reserved FC11 semantics; FC10 bit7 initialization
  selection is now supported by static analysis;
- compare the C++ readers/command against known hardware and the previous Python implementation.

### IP utility

- expand XG runtime migration and post-boot settings tests beyond the updated AMANEQ;
- investigate the normal-SiTCP power-cycle network outage and wire timing;
- extend native failure characterization beyond the observed enable-ACK loss.

## MPC/MPCX file inspection and MAC extraction

`mpc-mpcx-ip-command` uses `MPC_OR_MPCX_FILE` in help text instead of the ambiguous `FILE`. It means a 22-byte `.mpc` or `.mpcx` license/configuration file.

```text
mpc-mpcx-ip-command inspect MPC_OR_MPCX_FILE
mpc-mpcx-ip-command mac MPC_OR_MPCX_FILE
```

For verified payload layouts, the embedded MAC address is `payload[0:6]` for normal SiTCP/MPC and `payload[16:22]` for SiTCP-XG/MPCX. `inspect` reports the detected payload type, embedded MAC, and payload; `mac` reports the detected type and embedded MAC.

## Runtime / EEPROM diagnostic reports

Compact writer labels use `before: ` and `after:  ` to align MAC/IP fields.


The report labels current MAC, current IP, EEPROM MAC, and EEPROM IP in separate Runtime and EEPROM sections, followed by MPC/MPCX information reconstructed from EEPROM. Raw dumps use 16 hexadecimal bytes per row: four runtime rows for normal SiTCP, five for XG, and five EEPROM rows for either generation. MAC addresses use colon-separated hexadecimal notation. Numeric examples include `10000 (0x2710) Mbps` and `4660 (0x1234)`; timeout conversions retain units alongside decimal/hexadecimal raw values. Missing fields are displayed as `unavailable`, never decoded using placeholder zeros.


`src/sitcp-sitcpxg-register-report.hpp` supplies the common report used by the
MPC/MPCX reader and advanced `read` / `ip-read`. It reads runtime `0xFFFFFF00..0xFFFFFF3F` (64 bytes) for normal SiTCP or `0xFFFFFF00..0xFFFFFF4F`
(80 bytes) for SiTCP-XG, plus EEPROM `0xFFFFFC00..0xFFFFFC4F` (80 bytes) for both. Reads use
8-byte RBCP chunks and separate hex dumps. Identifier timeout aborts range
selection. The display and completion check use
the selected runtime length. MAC/IP values come directly from their region's registers, never
from reconstructed payloads. Generation detection still uses only FF08..FF0B.
The XG parameter decoder is shared by the two regions. Numeric register values
are decimal plus uppercase zero-padded hex; converted timeout units remain
visible. Register IP fields use the shared network helper to append eight
uppercase hexadecimal digits in network byte order (including zero padding),
e.g. `192.168.10.10 (0xC0A80A0A)`; this includes server and standalone IP views.
Out-of-range rates retain their raw value without claiming Mbps.
Normal SiTCP uses its own parameter decoder and raw dumps, never the XG map.

A diagnostic block bus error triggers byte-by-byte reads of that block. Each
rejected byte is marked unreadable and printed as `??`, with its address on
stderr. Only complete fields/payloads are decoded. This supports maps with
unreadable bytes within the selected range without suppressing other bytes
or EEPROM output. Normal SiTCP runtime FF40..FF4F is not requested. Short replies
and timeouts remain fatal and include the request address.

Read views return 3 for PARTIAL, 0 for COMPLETE, or 1 for fatal errors. Writers
use shared compact network snapshots (two lines before, two after, a blank separator, then
`Success! All operations completed and verified.`, followed by a blank line) instead of full diagnostic reads. They retain mandatory
programming/read-back verification and write-protection handling. Required
MPCX initialization still reads the complete runtime image. No writes to the
runtime tail are introduced.

Validation:

```bash
make
python3 tests/test_register_report.py
```

The tests use a local UDP RBCP simulator with synthetic data, checking all dump
bytes and source separation, both generations, decimal/hex rate and timeout
values, invalid rates, short reads, byte recovery after block bus errors,
unreadable runtime/EEPROM bytes, missing rate bytes, read-only traffic, and both
writer before/after views with EEPROM protection restored. They do not replace
physical-device validation. Python is needed only for these tests, not the CLI.

## MPCX initialization: source and limits (2026-09-26)

### EEPROM image layout

| EEPROM range | Source when initialization is needed |
| --- | --- |
| `FC00..FC0F` | MPCX file, first 16 bytes |
| `FC10..FC11` | Runtime `FF10..FF11` |
| `FC12..FC17` | MPCX file, final 6 bytes (MAC) |
| `FC18..FC4F` | Runtime `FF18..FF4F`, including the transmission rate |

Explicit EEPROM IP overrides are applied after programming; current/runtime IP changes are last, followed by reconnect and read-back verification. Without an EEPROM IP override, initialization saves the current runtime IP, which may be the device's ForceDefault address. It copies runtime values without automatic rate repair. Diagnostic `??` bytes must never be used as programming data. Normal MPC programming is unchanged.

### Evidence and implementation


Public guide: [SiTCP MPC Writer XG User Guide, section 3.3](https://www.bbtech.co.jp/download-files/sitcp/SiTCP-MPC-Writer-XG-en.0.1.1.pdf)
reports RAM-based initialization or built-in defaults depending on RAM access.
It does not define byte ranges or distinguish all internal write paths.
[SiTCP-XG manual, sections 4.1 and 6](https://www.bbtech.co.jp/download-files/sitcp/SiTCPXG_Manual_1.2_E_20201228.pdf)
maps runtime FF10..FF4F to EEPROM FC10..FC4F.

Static analysis used the official distribution
`sitcpmpcwritexg.win.0.4.1-2-gc782.zip`, downloaded from the BBT download page.
The SHA-256 of `SiTcpMpcWriteXG.exe` is
`0c01fa932830f74fc996dda56f81cea8c3fa11cd222d1b6f8f6212c0294294a4`.
The executable and disassembly are not repository deliverables.

Observed x86 virtual addresses:

- `0x40a0a0..0x40a0e0`: reads FC10 and returns bit7 as initialization state.
- `0x402fe7..0x40304f`: reads 80 bytes at FF00; records FC10 bit7 and
  checks the XG identifier in that RAM image.
- `0x403180..0x403224`: retains RAM for initialization; otherwise loads the
  80-byte EEPROM image. RAM read failure returns failure on this XG path.
- `0x408f81..0x40903a`: overlays 16 payload bytes at offset 0 and 6 at
  offset 0x12, selects 80 bytes when initializing versus 24 when preserving,
  and writes to FC00.
- `0x40928f..0x4093a6`: also attempts optional extension reads FF50..FF7F
  in 16-byte blocks and writes corresponding EEPROM blocks if readable.
  This extension behavior is implemented; see [Optional XG extensions](#optional-xg-extensions).
- The fixed-default branch at `0x407c7f..0x407cef` is in the legacy normal
  SiTCP writer: it reads 40 bytes at FF18, or uses a table at `0x417bc0`.
  That is not evidence for a SiTCP-XG fallback table. The earlier broad claim
  that the MPCX path always has a built-in default fallback was unsupported.

Implemented policy: for XG with FC10 bit7 set or explicit --initialize-eeprom,
obtain all 80 RAM bytes before any write, verify its identifier/reset state,
overlay the supplied MPCX file,
and write/verify FC00..FC4F. This includes the rate and reserved bytes in the
verified 80-byte copy; no rate normalization occurs. A failure to obtain the
image aborts before enabling writes. XG with bit7 clear and no forced option
retains the 24-byte path, even if the rate looks invalid. The
[normal-MPC record](#normal-mpc-prepared-record) is separate and does not change
this MPCX initialization policy.
The writer and `mpcx-plan` share mandatory base-image preparation. The read-only
plan has no force option and does not preview forced initialization of an
already initialized record. It does not probe or predict optional extension
copies; writer-time reads determine those separately. `verify` still checks the
license payload only, whereas the writer verifies every byte it programs.

Additional safety differences from the official GUI are exact independent XG
identifier validation, runtime RESET-bit rejection, optional 16-byte write chunks,
mandatory read-back verification, and protection restoration on error. The
write-enable operation itself is inside the cleanup scope so a lost enable ACK
still triggers a protection attempt. No destructive write is retried.

Tests in `tests/test_mpcx_initialization.py` use synthetic payloads and local
UDP emulation: clear-then-write restoration of every image byte (including
FC40), preservation without initialization, bit7 selection, missing RAM,
RESET-bit rejection, type mismatch, explicit IP override order, failed/lost
write ACKs, lost enable ACK, and read-back mismatch. These are not real-hardware
verification or proof of complete equivalence to all official-tool versions.

## Generation-specific diagnostic range

The [SiTCP Internal Register Manual 1.0.2, table 3-1 (printed p.2)](https://www.bbtech.co.jp/download-files/sitcp/SiTCP_Register_Manual_1.0.2.pdf#page=5)
labels runtime +0x40..+0xFF as Access prohibited area. It does not guarantee
that reads always return bus errors. User-provided hardware output showed
normal SiTCP returning bus errors for FF40..FF4F while reading EEPROM
FC40..FC4F successfully; XG returned all 80 runtime bytes. The report now
selects 64 versus 80 bytes from device generation before reading. Both retain
80 EEPROM bytes, including the normal MPC license tail. Explicit low-level
`rbcp-read` and `probe` retain their caller-specified ranges. MPCX initialization
still requires its complete 80-byte runtime image; programming maps are unchanged.

Simulator tests reject the entire normal-SiTCP runtime tail and verify that
reader/advanced read/ip-read and writer diagnostics never request those bytes,
retain EEPROM FC40..FC4F, and report COMPLETE without expected-tail warnings.

## Writer clear-only mode

`mpc-mpcx-ip-writer IP --clear [--port N] [--timeout SEC]` is an explicit
standalone erase operation (default off), not clear-before-programming.
It requires no license file and rejects file/IP-change combinations before
network access. No RAM restoration or subsequent file programming is performed.
For the compact output format, see [Runtime / EEPROM diagnostic reports](#runtime--eeprom-diagnostic-reports).

Both writer `--clear` and advanced `clear IP --yes-really-clear` call the shared
`src/sitcp-sitcpxg-eeprom-clear.hpp` helper. It enables EEPROM writes, writes FF
to `0xFFFFFC00..0xFFFFFC7F` in 16-byte blocks, restores protection, then verifies all 128
bytes in 8-byte reads. An enable/write failure triggers a protection attempt;
no destructive data write is retried. Verification failure must never print the success message.
The advanced command retains its existing guard; writer `--clear` itself is
the explicit clear-only selector. The operation does not write runtime space.

`tests/test_writer_clear.py` covers both generations, exact write ranges and
sequence, unchanged runtime, conflict rejection before device access, failed
or lost ACKs, read-back mismatch, protection cleanup, and the advanced guard.

## Standalone IP writer usage reference

TCP/UDP port option descriptions and examples are in the README appendix
([English](README.md#tcpudp-port-options), [Japanese](README.ja.md#tcpudpポートの指定)).
Protocol sequences and verification evidence remain in the technical sections above.
## Optional XG extensions

Only XG initialization (FC10 bit7 set or explicit --initialize-eeprom) attempts
FF50, FF60, FF70, each 16 bytes,
after programming the mandatory 80-byte image, before restoring protection.
Each completely read block is copied to FC50, FC60, FC70 respectively. A bus
error, exhausted read timeout, short reply or other transport read failure
skips that source block; no fabricated bytes or erase is substituted. Other
blocks are still tried. Preservation MPCX and normal MPC never copy extensions.
The meanings of these optional registers remain unidentified; raw copying
follows observed native behavior, without decoding or assigning guessed names.
Success output remains unchanged. A destination write failure is fatal, causes
protection cleanup, and is never retried. Every copied block is read back after
protection; a mismatch or failed verification prevents success. Explicit chunk
size applies to extension data too, defaulting to a whole 16-byte block.

## Unreachable targets and ambiguous operations

Shared transport validates a finite timeout in (0, 86400] seconds. UDP sockets
are connected to the resolved peer, filtering responses from other endpoints
and allowing available network errors to be surfaced. Resolution, send and
receive failures identify the target; no response produces a bounded timeout.
Read timeouts may be retried up to three times per read, not indefinitely.
Initial snapshot or generation detection failure stops before enabling writes.
A successful snapshot does not guarantee the device stays reachable: later
write failures still require best-effort protection restoration, no destructive
retry, and no success message. Protection cannot be guaranteed if the network
remains down. Optional XG source failures are tolerated only for extension reads;
mandatory XG base-image reads and final verification remain fatal. Normal MPC
now has its separately documented initialization fallback unless --no-fallback
is supplied; it cannot bypass initial snapshots or generation/EEPROM reads. No-response
writer tests verify a nonzero exit, bounded completion and zero write packets.
