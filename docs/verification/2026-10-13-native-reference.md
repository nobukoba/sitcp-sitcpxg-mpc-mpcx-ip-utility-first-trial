# Verification record: Normal MPC: official writer comparison (2026-10-13)

[Verification index](README.md) | [Current technical guide](../../FOR_DEVELOPERS.md)

These are dated observations for the stated builds and targets, not a current
specification or a new test run. Original conditions, results, recovery and
limitations are retained below. Source-relative references have been updated;
historical prose such as "above/below" refers to the original report order.
Capture/backup paths name private session storage, not files in this checkout.

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
protection before read-back. At that stage MPCX pacing/chunking was unchanged. This is
observed software behavior, not a claim that physical EEPROM settling was tested.

The captured data and narrowly scoped static analysis establish this normal
record layout:

| EEPROM bytes | Source |
| --- | --- |
| FC00..FC0F | untouched: outside the normal programming record |
| FC10 | always 01 |
| FC11 | retained from EEPROM in these successful-RAM/preservation captures; 00 in fixed fallback |
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
At that comparison stage MPCX image preparation and its 16-byte write chunks
were unchanged; the later transfer-size option below supersedes the chunk default.

### Explicit compatibility limits

The official legacy normal branch at `0x407cac..0x407cef` substitutes a fixed
40-byte table at `0x417bc0` if runtime reading fails, including default EEPROM
IP 192.168.10.16. At that historical stage this custom tool deliberately **failed before enabling writes**
when FF18..FF3F cannot be obtained. It did not automatically substitute that
table or claim identical error/fallback behavior. The forced-initialization GUI
checkbox and legacy fallback were not exercised by this synthetic capture.
At that historical stage, XG optional extension copying was not implemented;
the later update implements it with simulator coverage.
At that stage physical EEPROM timing was not checked. Later actual captures
measure write-start timing. Normal-SiTCP reboot acceptance was subsequently
tested on 2026-10-15; other firmware variants remain outside that evidence.

### Normal-SiTCP fixed fallback settings (static evidence)

The installed native Writer 0.4.1-2-gc782 table at VA 0x417BC0 contains 40
bytes used for FC18..FC3F if the normal initialization RAM read fails.
Decoded values below use the normal-SiTCP register map. This failure branch
was originally supported by static evidence, then by synthetic native execution.
The later LRTDC test exercised native and CLI fallback through an injected short
response from real hardware; it did not induce a physical RAM fault.
The current custom writer uses this table by default; --no-fallback stops instead.

| Setting | Native fixed value |
| --- | --- |
| IP | 192.168.10.16 |
| TCP data port / command port | 24 / 23 |
| UDP/RBCP port | 4660 |
| Maximum segment size | 1460 bytes |
| Keep Alive buffer nonempty / empty | 1000 / 60000 ms |
| Connect / disconnect timeout | 5000 / 2880000 ms |
| Reconnect / retransmission timer | 500 / 500 ms |

MAC/license overlays still come from the MPC file. This table is not evidence
of an XG fallback and is never reused for XG initialization.

### MPCX regression capture

The actual official XG erased-record branch read FF00/80, wrote FC00/80,
attempted the optional extension reads at FF50, FF60 and FF70 (16 bytes each),
and restored protection. The simulator rejected those extension reads; the
GUI reported that the extension areas could not be read. Its confirmation
message used the phrase "default values", but the actual captured FC10..FC11
and FC18..FC4F bytes came directly from the supplied runtime image, including
its unusual rate bytes. The protocol bytes, not that generic label, establish
the image behavior. `tests/reference/official_mpcx_erased.json` independently
checks the custom MPCX image against that capture. At that stage the custom
XG path used 16-byte chunks. The current default is a single 80-byte transfer
when initializing; see the later bulk-default update.

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
write. At that stage ports remained unchanged; standalone port options were added in the later update below.
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
