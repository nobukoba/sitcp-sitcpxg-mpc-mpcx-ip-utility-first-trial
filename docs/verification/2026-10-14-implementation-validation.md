# Verification record: Clear write pacing: official comparison and update

[Verification index](README.md) | [Current technical guide](../../FOR_DEVELOPERS.md)

These are dated observations for the stated builds and targets, not a current
specification or a new test run. Original conditions, results, recovery and
limitations are retained below. Source-relative references have been updated;
historical prose such as "above/below" refers to the original report order.
Capture/backup paths name private session storage, not files in this checkout.

## Clear write pacing: official comparison and update

Official Writer 0.4.1-2-gc782 was exercised on normal SiTCP at 192.168.10.16.
Its captured clear sequence matches this repository's erase addresses, sizes
and values: FCFF=00, eight 16-byte FF writes over FC00..FC7F, then FCFF=FF.
All ten write ACKs succeeded, and an independent read confirmed 128 erased
bytes. The selected MPC and all 128 pre-clear bytes were restored afterward;
runtime stayed unchanged and 100 subsequent pings succeeded. License-bearing
backups and captures remain private and are not repository contents.

The official write-start gaps were 502.6--516.2 ms. The previous repository
clear immediately proceeded after ACKs: about 183--184 ms on that device.
Clear was changed to use steady_clock and sleep_until to schedule at least
500 ms between client write starts, including enable and protection writes.
The later 2026-10-15 capture measured shorter wire gaps, so this is not a verified
500 ms wire guarantee; see the current timing qualification above. Device ACK time
counts toward the 500 ms; slow ACKs do not cause an additional full 500 ms wait.
Failure cleanup uses the same pacing without retrying erase writes.
Mandatory 128-byte read-back remains; official read-back was not observed
during erase or within two seconds after protection in this successful capture.
Failure/lost-ACK behavior of the official GUI and power-cycle acceptance were
not tested. At that stage only the earlier version had been exercised on hardware; the new pacing
is checked by simulator timing assertions for both generations and failure paths.
Validation after the pacing update: strict C++11 build/install succeeded and
all 38 tests passed in 68.252 seconds. The installed local binaries include
the pacing update. No additional physical clear was performed at that stage; later XG comparison used the updated clear implementation.

## MPC/MPCX bulk default and explicit chunks

At the user's request, the programming transfer defaults to one complete
prepared image: normal MPC 64 bytes at FC10, preservation MPCX 24 bytes at
FC00, and initialization MPCX 80 bytes at FC00. The preservation MPCX image
is not enlarged merely to reach 80 bytes; unmodified configuration remains
outside its programming range. Existing generation/initialization validation
and byte construction are unchanged.

`--write-chunk-size official` explicitly selects the default whole-image lengths.
Otherwise `--write-chunk-size N` accepts a positive decimal integer, 1..255 bytes;
16 reproduces the previous MPCX split mode (24 bytes becomes 16+8, 80 bytes
becomes five 16-byte writes). Normal MPC 64 bytes becomes four 16-byte writes.
Both modes verify every programmed byte and restore protection after failures
without retrying data writes. The high-level wrapper forwards the value before
its IP overrides. Invalid options and clear/size combinations fail before
network reads. IP transfer sizes and fixed clear blocks/pacing are unaffected.

Tests compare bulk and split addresses, lengths, concatenated payloads and
final EEPROM images for all three image sizes. Bulk and split failures test
protection cleanup and no retries. The official erased-MPCX synthetic fixture
also checks the default FC00/80 transfer. No new physical-device writes are
performed as part of this option validation.
Strict C++11 build/install and all 41 tests passed (64.481 seconds). Both
bulk and split failures preserve the protection/no-retry guarantees.

## Consolidated verification and optional-extension update (2026-10-14)

The following records the 2026-10-14 evidence stage, separating actual hardware
evidence from synthetic failure tests. Its limits are historical; the current
2026-10-15 summary above includes later boot/ACK-loss tests and wire timing.
Official versions: MPC Writer XG 0.4.1-2-gc782 and Utility 0.9.0.

| Operation | Evidence and result | Remaining limits |
| --- | --- | --- |
| Normal MPC | Actual device: full prepared FC10/64 record verified; native synthetic record agrees; control FF became 01, license preserved; ping 100/100 | No power-cycle test; previous ping instability was not reproduced |
| Normal clear | Both actual tools erase FC00..7F as eight 16-byte FF blocks; native gaps 502.6–516.2 ms; full restoration and ping 100/100 | Native failure/lost-ACK behavior untested |
| Clear pacing update | Monotonic minimum 500 ms between write starts; subsequently used during actual XG comparison | Simulated failures test cleanup pacing; no deliberate hardware communication failure |
| Utility EEPROM IP | Actual native and custom same-value write: enable, FC00/17, FC18/40, protect agree byte-for-byte; all 128 bytes preserved; ping 100/100 | Native/custom comparison did not migrate the active runtime IP |
| Custom EEPROM IP | Actual default IP changed to the runtime IP; only two differing IP bytes changed; RAM unchanged; ping 100/100 | No power-cycle verification |
| XG preservation | Actual native/custom FC00/24 data and final 128 EEPROM bytes agree | Native ~500-ms pacing differs from custom ACK-paced programming |
| XG initialization | Both begin erased; actual FC00/80 data and all final 128 bytes agree; RAM unchanged, configured rate 10000 retained; ping 100/100 and complete restoration | Additional FF50/60/70 reads all returned C9 with zero length; readable extension hardware untested |
| Reader | Synthetic register maps, decimal/hex, raw bytes, partial bus-error recovery and native label comparison | Reported transmission rate is a setting, not measured throughput |

Earlier EEPROM FF counts are recorded in the sanitized old-ff-summary.json.
Normal EEPROM had 54 FF bytes in its 80-byte view, including unset settings;
this is an observation, not proof of the cause of historical ping loss.
TCP Keep Alive affects TCP sessions and does not explain ICMP ping directly.
Actual licensed captures and backups are private and excluded from Git and
public archives. Sanitized summaries are delivered alongside this repository.
XG capture was repeated on the actual Wi-Fi route after an initial incorrect
interface selection; only the corrected capture supports packet comparisons.

### Current optional extension policy

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
This new behavior is validated synthetically for readable, partially readable,
bus-error, short-reply and timeout sources, and ambiguous extension-write ACK.
At that update stage it had not been rerun on real hardware; prior real tests
used the version which skipped optional reads. The subsequent updated-AMANEQ
XG suite at the top verifies current inaccessible-extension behavior on hardware;
readable sources still have only synthetic coverage.

### Unreachable targets and ambiguous operations

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


Validation at the optional-extension/transport update stage: strict C++11 build/install passed; all 43 integration tests passed in 66.123 seconds. The subsequent port update passed 45 tests, as noted below. This records validation before the subsequent port update.

## Closer standalone IP Utility behavior (2026-10-14)

The verified native IP-write sequence was already implemented for normal SiTCP
and XG. This update adds the native General tab's configurable TCP data port
and UDP port to the standalone CLI via --tcp-port / --udp-port. Existing calls
remain IP-only; omitted options preserve the read settings. --port still means
the current RBCP transport destination. MPC wrapper IP operations and advanced
ip-write continue to preserve ports through default helper arguments.

Only explicitly selected bytes are changed: IP 18..1B, TCP data port 1C..1D,
UDP/RBCP port 22..23. Shared helpers keep EEPROM FC00/64 read, enable, FC00/17,
FC18/40, protect, complete 64-byte verification; runtime retains two FF00/64
reads, FF00/17 and FF18/40 writes. There is no added 500-ms delay: actual native
Utility captures progress with ACKs, unlike native MPC Writer pacing.
Runtime UDP changes reconnect and verify at the new IP/UDP endpoint, then use
that endpoint for the compact after snapshot. EEPROM port changes verify via
the unchanged runtime endpoint. Invalid IP/port options fail before network
access. No default-address fallback is introduced: no such Utility failure
behavior has been established. Protection cleanup, no destructive retries and
mandatory verification remain intentional safeguards. Compact output unchanged.

Synthetic tests cover both generations and destinations, full preservation of
all nonselected bytes, native write lengths, and a mock device actually moving
its UDP socket after the runtime parameters write. Invalid port values and
malformed IPs are rejected without any device request. At this implementation stage, new port-changing paths were simulator-only.
The following physical comparison subsequently verified EEPROM port changes
on both generations. At that stage runtime endpoint migration remained
simulator-only; normal-SiTCP migration was subsequently verified on 2026-10-15.
Validation: strict C++11 build/install passed and all 45 integration tests passed in 73.342 seconds. Local installed binaries include the validated changes; publication revision is recorded in Git history.

## Documentation organization (2026-10-14)

README.md and README.ja.md contain installation, usage, CLI defaults, option
semantics, visible output and warnings needed to operate the tools. TCP/UDP port
examples are in the README appendix; transfer-size examples remain in CLI help
and this developer guide, not README. Protocol block sizes,
register maps, clear timing, optional extension rules, native comparisons,
failure semantics and test evidence remain in the technical sections above.
MPC/MPCX selection is based on the 22-byte payload, never its filename suffix.
The source and verified hardware behavior were not changed by this cleanup.
