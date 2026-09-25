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

1. Read the current EEPROM image and independently detect the target generation.
2. For MPCX, prepare either the 24-byte preservation image or the complete
   80-byte RAM initialization image as described below. For MPC, retain the
   existing mapping and preservation behavior.
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

`src/sitcp-sitcpxg-register-report.hpp` supplies the common report used by the
MPC/MPCX reader, advanced `read` / `ip-read`, and before/after views of the
MPC/MPCX writer and advanced `ip-write`. It reads runtime FF00..FF3F (64 bytes) for normal SiTCP or FF00..FF4F
(80 bytes) for SiTCP-XG, plus EEPROM FC00..FC4F (80 bytes) for both. Reads use
8-byte RBCP chunks and separate hex dumps. Identifier timeout aborts range
selection. The display, completion check, and writer before/after views use
the selected runtime length. MAC/IP values come directly from their region's registers, never
from reconstructed payloads. Generation detection still uses only FF08..FF0B.
The XG parameter decoder is shared by the two regions. Numeric register values
are decimal plus uppercase zero-padded hex; converted timeout units remain
visible. Out-of-range rates retain their raw value without claiming Mbps.
Normal SiTCP retains raw dumps without applying the XG parameter map.

A diagnostic block bus error triggers byte-by-byte reads of that block. Each
rejected byte is marked unreadable and printed as `??`, with its address on
stderr. Only complete fields/payloads are decoded. This supports maps with
unreadable bytes within the selected range without suppressing other bytes
or EEPROM output. Normal SiTCP runtime FF40..FF4F is not requested. Short replies
and timeouts remain fatal and include the request address.

Read views return 3 for PARTIAL, 0 for COMPLETE, or 1 for fatal errors. Writer
views label partial diagnostic reports but keep the existing mandatory
programming/read-back verification and write-protection path. No writes to the
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
24-byte path, even if the rate looks invalid. Normal MPC writes are unchanged.
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
