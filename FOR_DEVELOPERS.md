# For developers

## Scope

This repository contains two related but functionally separate C++11 utility groups for SiTCP / SiTCP-XG:

1. MPC/MPCX license/configuration utilities.
2. SiTCP Utility compatible IP-address-only utilities.

They may share low-level RBCP transport code, but they must not share domain logic. Keep three evidence classes distinct when documenting behavior: public Bee Beans Technologies documentation, static analysis of official utilities, and controlled verification with real files/hardware.

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

For development without installation, replace the README's `./bin/` prefix with `./src/`, for example `./src/mpc-mpcx-ip-reader DEVICE_IP`. The default `PREFIX` is `$(CURDIR)` and `BINDIR` is `$(PREFIX)/bin`. `make install` builds outdated binaries before copying them.

## Current programs

- `mpc-mpcx-ip-writer` — high-level MPC/MPCX writer; requires an MPC/MPCX file and may additionally set EEPROM/default and current/runtime IP addresses.
- `mpc-mpcx-ip-reader` — MPC/MPCX reader; also reports current/EEPROM MAC and IP values.
- `mpc-mpcx-ip-command` — advanced/diagnostic interface with MPC/MPCX, IP, and low-level RBCP operations.
- `sitcp-sitcpxg-ip-reader` — IP-only reader, independent of MPC/MPCX payload decoding.
- `sitcp-sitcpxg-ip-writer` — IP writer using the official Utility setting-block sequence; no license decoding, with non-IP bytes retained unchanged.

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
`FC00..FC0F` and `FC12..FC17`; `FC10..FC11` are preserved. The writer
must also preserve `FC18` and later configuration bytes. In particular,
different values observed at FC20/FC2A/FC40 on different SiTCP-XG boards are
not evidence of a bad MPCX payload.

## Observed SiTCP-XG FC40..FC4F case

A SiTCP-XG target at `192.168.2.187` was observed with a valid XG
identifier (`0x58544350`) and an MPCX payload that reconstructs exactly
from `FC00..FC0F + FC12..FC17`.  Its EEPROM nevertheless contains:

```text
FC40..FC4F =
83 A8 9C 99 A6 54 54 35 34 4C 38 39 FD 57 08 09
```

A non-destructive read of `0xFFFFFF40..0xFFFFFF4F` returned the same
16 bytes.  Therefore these bytes are not merely an unread EEPROM remnant;
they are also visible in the corresponding runtime register window.

The first 12 bytes,

```text
83 A8 9C 99 A6 54 54 35 34 4C 38 39
```

also exactly match the 12-byte portion observed in multiple verified normal
SiTCP `.mpc` files, whose layout is `MAC[6] + common[12] + varying[4]`.
This is strong empirical evidence that the observed `FC40..FC4F` contents
have a normal-MPC-formatted origin.  It is not proof of the device's write
history, and the final four bytes have not been decoded.

In particular, this observation does **not** mean that the attached MPCX
file was accidentally interpreted as an MPC file.  If the verified MPCX
payload for this target were mapped using the normal-MPC layout,
`FC40..FC4F` would instead contain `payload[6..21]`, which does not match
the observed bytes.

The public XG register map documents `FC40..FC41` as the EEPROM initial
value for the transmission-rate register, with a documented range of
1..10000.  The observed raw value `0x83A8` is 33704 and is outside that
range.  Do not report it as an effective rate of 33704 Mbps.  More
do not normalize, clear, or replace `FC40..FC4F` during ordinary MPCX
programming. Initialization selected by FC10 bit7 is the explicit exception:
it copies runtime settings through FC4F before overlaying the MPCX payload.

A plausible history is that normal-MPC-formatted data was written to this
area before a later MPCX programming operation.  This remains a hypothesis,
not a verified fact.

## MPC/MPCX write sequence

When both IP options are given, EEPROM IP is written first and current/runtime IP last, preserving reachability at the original IP until all earlier operations finish. After runtime IP changes, reconnect to the new IP and verify by read-back. A lost acknowledgement may mean the IP has already changed; do not blindly retry the write.


1. Read the current EEPROM image and independently detect the target generation.
2. For MPCX, prepare either the 24-byte preservation image or the complete
   80-byte RAM initialization image as described below. For normal MPC, prepare
   the separate 64-byte FC10 record described in the official-writer comparison.
3. Release EEPROM write protection.
4. Write MPCX images in 16-byte blocks; normal MPC uses one 64-byte FC10 write.
5. Restore protection, including error paths where possible.
6. Read the programmed image again.
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

## IP utility design

The standalone IP-only CLI is:

```text
sitcp-sitcpxg-ip-reader CURRENT_IP [options]
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

The IP tools must not classify, reconstruct, or validate MPC/MPCX license payloads. Utility-compatible IP writes include an unchanged settings prefix; they must not alter the values of license bytes.

Requirements:

- reader reports current/runtime and EEPROM/default network configuration;
- writer changes only IP-related state;
- EEPROM is the default write target;
- changing current/runtime IP requires an explicit option;
- writer performs read-back verification;
- after changing current/runtime IP, reconnect to `NEW_IP` and verify when possible;
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
| IP-only current/runtime read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM/default read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM write mechanism | to investigate | to investigate | not yet verified |
| IP-only runtime write/reconnect | to investigate | to investigate | not yet verified |

## Remaining work

### MPC/MPCX

- verify exact semantic meaning of all license bytes;
- investigate remaining reserved FC11 semantics; FC10 bit7 initialization
  selection is now supported by static analysis;
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
Normal SiTCP retains raw dumps without applying the XG parameter map.

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
  This extension behavior is intentionally not implemented in the custom tool.
- The fixed-default branch at `0x407c7f..0x407cef` is in the legacy normal
  SiTCP writer: it reads 40 bytes at FF18, or uses a table at `0x417bc0`.
  That is not evidence for a SiTCP-XG fallback table. The earlier broad claim
  that the MPCX path always has a built-in default fallback was unsupported.

Implemented policy: for XG with FC10 bit7 set, obtain all 80 RAM bytes before
any write, verify its identifier/reset state, overlay the supplied MPCX file,
and write/verify FC00..FC4F. This includes the rate and reserved bytes in the
verified 80-byte copy; no rate normalization occurs. A failure to obtain the
image aborts before enabling writes. XG with bit7 clear retains the existing
24-byte path, even if the rate looks invalid. The later normal-MPC compatibility
update below is separate and does not change this MPCX initialization policy.
The writer and `mpcx-plan` share image preparation. `verify` still checks the
license payload only, whereas the writer verifies every byte it programs.

Additional safety differences from the official GUI are exact independent XG
identifier validation, runtime RESET-bit rejection, 16-byte write chunks,
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

## Normal MPC: official writer comparison (2026-10-13)

The comparison used the locally installed official MPC Writer XG
0.4.1-2-gc782 (SHA-256 above), synthetic 22-byte files, a loopback-only RBCP
simulator on UDP 24660, and Npcap/dumpcap. No physical board was written and no
user license file was copied into the repository. The checked-in independent
references are `tests/reference/official_mpc_erased.json` and
`tests/reference/official_mpc_preserved.json`.

For a normal-SiTCP target with erased EEPROM and the GUI initialization checkbox
off, the actual official request sequence was:

| Operation | Address | Length/data |
| --- | --- | --- |
| Target detection | FFFFFF08 | read 4 bytes |
| Initialization status | FFFFFC10 | read 1 byte |
| Load normal record | FFFFFC10 | read 64 bytes |
| Obtain current configuration | FFFFFF18 | read 40 bytes |
| Enable writing | FFFFFCFF | write 00 |
| Program normal record | FFFFFC10 | write 64 bytes |
| Restore protection | FFFFFCFF | write FF, after dismissing the success dialog |

The separate Check button added an earlier identifier/status pair. RBCP request
IDs, source ports, and GUI pauses are transport details, not part of the image.
The custom writer retains extra MAC/IP snapshots and byte-read verification;
it restores protection before verification rather than leaving it enabled while
the success dialog waits for the user.
The captured normal writer waits approximately 500 ms between RBCP operations.
The custom normal-MPC path now retains a 500 ms interval after enable and data
writing, before protection cleanup following an ambiguous ACK, and after
protection before read-back. MPCX pacing/chunking remains unchanged. This is
observed software behavior, not a claim that physical EEPROM settling was tested.

The captured data and narrowly scoped static analysis establish this normal
record layout:

| EEPROM bytes | Source |
| --- | --- |
| FC00..FC0F | untouched: outside the normal programming record |
| FC10 | always 01 |
| FC11 | retained from EEPROM |
| FC12..FC17 | MPC payload[0:6] |
| FC18..FC3F | retained EEPROM, or runtime FF18..FF3F when initializing |
| FC40..FC4F | MPC payload[6:22] |
| FC50..FC7F | untouched |

The initialized-record capture retained FC11 and the EEPROM IP/ports/settings,
did not read runtime FF18/40, and still programmed control 01 with the same
64-byte FC10 write. Both captured final EEPROM images are regression fixtures.

At executable VA `0x4076d0..0x407c7f`, the legacy normal branch initializes
when **both** IP endpoint bytes FC18 and FC1B equal FF; it does not test that
all four bytes are FF and does not use the MPCX FC10-bit7 rule. VA
`0x407887..0x407913` sets control 01, overlays the payload, enables writes,
and programs one 64-byte record at FC10. Erased and initialized normal records
were both captured from the actual GUI. Partially erased IP endpoint cases
remain static-analysis evidence plus simulator tests, not hardware verification.

The previous implementation preserved erased settings/control and programmed
80 bytes at FC00. The independent capture-based test failed against that
implementation and passes after this correction. Generation detection now
precedes selecting the normal 64-byte versus XG 80-byte EEPROM read window.
The MPCX image preparation and its 16-byte write chunks are unchanged.

### Explicit compatibility limits

The official legacy normal branch at `0x407cac..0x407cef` substitutes a fixed
40-byte table at `0x417bc0` if runtime reading fails, including default EEPROM
IP 192.168.10.16. This custom tool deliberately **fails before enabling writes**
when FF18..FF3F cannot be obtained. It does not automatically substitute that
table or claim identical error/fallback behavior. The forced-initialization GUI
checkbox and legacy fallback were not exercised by the captured branch. Existing
XG optional FF50..FF7F extension behavior remains outside the implementation.
Physical EEPROM timing, subsequent reboot behavior, and all firmware variants
remain hardware acceptance items.

### MPCX regression capture

The actual official XG erased-record branch read FF00/80, wrote FC00/80,
attempted the optional extension reads at FF50, FF60 and FF70 (16 bytes each),
and restored protection. The simulator rejected those extension reads; the
GUI reported that the extension areas could not be read. Its confirmation
message used the phrase "default values", but the actual captured FC10..FC11
and FC18..FC4F bytes came directly from the supplied runtime image, including
its unusual rate bytes. The protocol bytes, not that generic label, establish
the image behavior. `tests/reference/official_mpcx_erased.json` independently
checks the existing custom MPCX image against that capture. Chunking remains
16 bytes in the custom XG path, versus the official single 80-byte transfer.

## Official SiTCP Utility 0.9.0 observation (2026-10-13)

The local `SiTCPUtility.exe` SHA-256 is
`1b50bddb09a84f3adeddd1ef26ac78b3f91a8ba2f1d014e661ab56562d197b72`.
The general tab was exercised against the same normal-SiTCP loopback simulator.
The official user guide distinguishes current-memory access (EEPROM checkbox
off) from EEPROM access (on), and recommends EEPROM programming followed by
reboot when changing the device's network settings:
[SiTCP Utility guide, sections 2.1.3–2.1.5](https://www.bbtech.co.jp/download-files/sitcp/SiTCPUtilityUserGuide.0.9.pdf).

Actual captured operations, with TCP/UDP values retained in the simulator:

| General-tab operation | Actual requests |
| --- | --- |
| Read current settings | read FF00, 64 bytes |
| Read EEPROM settings | read FC00, 64 bytes |
| Write EEPROM settings | read FC00/64; enable FCFF=00; write FC00/17; write FC18/40; protect FCFF=FF |
| Write current settings | two reads FF00/64; write FF00/17; write FF18/40; no EEPROM enable write |

The runtime GUI also changes its target UDP port to the newly selected UDP
setting. No real address transition was tested: the runtime comparison kept
127.0.0.1 and the simulator's existing settings, so it cannot establish physical
IP migration or lost-ACK behavior.

At the user's request, IP writing now follows these captured general-tab block
sequences: read the complete 64-byte settings image (twice for runtime), retain
all non-IP values, then write the 17-byte prefix and 40-byte parameter block.
FC11..FC17 / FF11..FF17 are not written. EEPROM FC00..FC0F is written back
unchanged, including any MPCX license bytes; no license parsing is performed.
EEPROM protection is restored before a complete 64-byte read-back comparison.
Runtime prefix failures stop immediately; only a timeout of the final, address-
changing block is followed by verification at the new IP, without retrying the
write. Ports remain unchanged; port editing is not exposed by this CLI.
The existing before/after snapshots and added read-back make the complete
CLI traffic longer than the GUI trace; the configuration-write sequence matches.
An enable-ACK timeout now triggers a protection-restoration attempt in the
EEPROM IP helper, matching the cleanup guarantee already used by the MPC writer.
The proprietary utility and disassembly are not repository deliverables.

### Validation environment

Baseline: 23 pre-existing tests passed under a private Cygwin GCC C++11 build.
The capture-based normal-MPC test failed before the fix. The expanded suite
covers the reference image, initialization/preservation conditions, no writes
after unreadable RAM, lost enable/data ACKs, read-back mismatch, write protection,
IP override order, and existing MPCX/reader/clear behavior. No proprietary data
is needed to run it:

```bash
make
python3 -m unittest discover -s tests -v
```

On the Windows validation host, Cygwin needs its POSIX declarations enabled in
strict C++11 mode. The build used the following override without changing the
Linux/macOS/WSL defaults:

```bash
make CXXFLAGS="-O2 -std=c++11 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -Wpedantic"
```

Validation before the Utility-compatible IP update: GCC 14.4.0, strict C++11, all 32 integration tests passed.
`make install` rebuilt/copied all five public commands locally. Their help
startup checks passed after correcting the IP-only writer's pre-existing
`--help` exit-code error (help now exits 0; missing required arguments still
exit 2). The official GUI target/file settings were restored and all loopback
simulator/capture processes stopped. SiTCP Utility's on-screen result/log area
still contains the synthetic test results, not readings from a physical board.

### Utility-compatible IP update validation

The independent `tests/reference/official_utility_ip.json` fixture is extracted
from the previously captured official GUI synthetic-device trace. Tests compare
every configuration-write address and payload, including enable/protect, for
normal SiTCP and XG. They also check unchanged MAC, ports, control, license and
unwritten regions, complete-read failure before writes, protection restoration,
no destructive retry, and full-image mismatch detection. The runtime timeout
test retains localhost; it does not establish real-device address migration.
The shared helper also applies to IP overrides after MPC/MPCX programming and
the advanced IP-write command. Extra before/after reads and read-back remain.
GCC 14.4.0 strict C++11 build/install completed without warnings; all 36
integration tests passed (34.901 seconds). No physical device was written and
no GitHub commit or push was performed.

## Reader settings display (2026-10-13)

The MPC/MPCX reader and advanced `read` / `ip-read` share a generation-aware
settings display. General IP/TCP/UDP settings precede TCP timers, client/server
settings, control flags and transmission settings. This organization follows
the official Utility's general/settings conventions while retaining separate
runtime and EEPROM regions, decimal/hex values, raw dumps and bus-error markers.
It is a CLI presentation, not a reproduction of the graphical interface.

Normal decoding is based on [SiTCP Register Manual 1.0.2](https://www.bbtech.co.jp/download-files/sitcp/SiTCP_Register_Manual_1.0.2.pdf),
sections 3.7--3.19; XG decoding uses [SiTCP-XG Manual 1.8.1](https://www.bbtech.co.jp/download-files/sitcp/SiTCPXG_Manual_1.8.1.pdf),
sections 4.2.5--4.2.20. The [Utility guide](https://www.bbtech.co.jp/download-files/sitcp/SiTCPUtilityUserGuide.0.9.pdf)
provides the general IP/TCP/UDP terminology. Key generation differences:

| Field | Normal SiTCP | SiTCP-XG |
| --- | --- | --- |
| TCP sub port | +1E, displayed | Not defined, omitted |
| Disconnect timeout | N*256 ms | (N+1)*256 ms |
| +2C timer | Reconnect interval, N ms | MSL, N*0.5 ms |
| Server MAC/IP/port | +30/+36/+3A | +32/+38/+3C |
| Control flags | ARP, duplicate ACK, MIF, flow control, keepalive, fast retransmission, Nagle, reset | Window scaling, keepalive, fast retransmission, Nagle, reset |
| Transmission-rate setting | Unsupported; no FF40 access or FC40 license decoding | +40, configured line-rate limit in Mbps |

Both regions are decoded from their own readable bytes. An unsupported rate
setting does not establish the negotiated link speed; no PHY interrogation or
throughput benchmark was added. No device writes are performed by the reader.
Tests use known differing server addresses, timers and control values to catch
accidental reuse of the XG map, and an unreadable normal UDP byte to verify that
partial fields are not converted into invented values.

Validation: all 38 integration tests passed (37.564 s). After the final General
heading adjustment, build/install and all 13 register-report tests passed
(7.658 s). The two published reader examples are actual CLI output obtained
from synthetic localhost registers, explicitly labeled as such. Physical
hardware was not accessed; no GitHub changes were made.



### Official Utility parameter labels

Confirmed English label strings in the installed official SiTCP Utility 0.9.0
include TCP Keep Alive Enabled, TCP Keep Alive(buffer not empty), TCP Keep
Alive(buffer empty), TCP Max Segment Size, TCP Timeout(Connecting), TCP
Timeout(Disconnect), TCP Re-Connect Interval, TCP Re-Transmition Timer,
TCP Command Port, TCP Port and UDP Port. These labels are used in the reader;
the official spelling of Re-Transmition is retained. This is static label
evidence, not a new hardware measurement or proof of exact GUI layout.
Keep Alive Enabled precedes its two timer values in TCP settings, making the
switch and interval distinction explicit. Parameter columns allow 36 characters
to align the longer official labels. Decimal/hex, units, time conversions,
generation-specific decoding, raw dumps and partial-read handling are retained.
After this label update, the strict C++11 build/install and all 38 tests passed
(37.463 s). Synthetic reader examples were refreshed from the rebuilt reader.
The Utility-compatible parameter block has a blank line before its TCP Port
entry and after its final entry, separating it from MAC/IP and raw dumps in
both runtime and EEPROM displays. No fields or raw bytes were removed.
