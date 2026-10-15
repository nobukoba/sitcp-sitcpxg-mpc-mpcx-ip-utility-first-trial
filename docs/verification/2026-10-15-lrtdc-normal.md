# Verification record: LRTDC normal SiTCP comprehensive comparison (2026-10-15)

[Verification index](README.md) | [Current technical guide](../../FOR_DEVELOPERS.md)

These are dated observations for the stated builds and targets, not a current
specification or a new test run. Original conditions, results, recovery and
limitations are retained below. Source-relative references have been updated;
historical prose such as "above/below" refers to the original report order.
Capture/backup paths name private session storage, not files in this checkout.

## LRTDC normal SiTCP comprehensive comparison (2026-10-15)

This later pass tested LRTDC normal SiTCP at 192.168.10.16:4660 after the
user changed the connected board. It does not replace the earlier AMANEQ/XG
observations. No access was made to 192.168.2.187. The native Writer and Utility
were the same installed tools used in the earlier comparisons. Firmware
revision was not supplied.
The starting runtime was IP 192.168.10.16, MAC 02:00:C0:A8:00:10, TCP 24,
UDP 4660; EEPROM held IP 192.168.10.17, MAC 7C:F0:98:01:20:17, the same ports,
FC10=01 and FC11=FF. The selected 22-byte SN_2F2C54AF22.mpc exactly matched
the installed MAC/license. Private snapshots covered all 64 runtime bytes and
128 EEPROM bytes before and after each case, including bytes outside the write.

The 40 recorded cases include expected failures and the initial mismatch
display before correction. Fourteen native/CLI pairs had identical starting
and final full images and identical physical write addresses, lengths and
payloads. The comparison excludes request IDs, host source ports, read order,
GUI delays and supplementary verification reads. Check pairs had no writes;
their requests were separately verified as exactly FF08/4 then FC10/1.

| Paired operation | Observed result |
| --- | --- |
| Check without file, matched file, mismatched file, erased EEPROM with file | Same two reads, no writes, same report text after the punctuation fix below |
| MPC preservation, Initialize unchecked and checked | Enable FCFF/1, FC10/64, protect FCFF/1; saved IP 17 and FC11=FF retained |
| MPC after Clear, Initialize unchecked and checked | Same writes; FF18/40 settings copied from runtime, saved IP becomes 16, FC11 remains FF |
| Valid saved IP with FC10=80, Initialize checked | No FF18/40 settings copy; saved IP 17 and FC11=FF retained, FC10 becomes 01 |
| Alternate MPC file SN_2F2C54AF18.mpc | Same replacement MAC/license and preserved settings; original file restored afterward |
| Clear | Enable, eight FC00..FC7F 16-byte FF writes, protect; all EEPROM bytes FF and runtime unchanged |
| EEPROM IP/TCP/UDP | Enable, FC00/17, FC18/40, protect; saved IP 11, TCP 25, UDP 4661; runtime unchanged |
| Runtime IP/TCP/UDP | FF00/17 then FF18/40; new endpoint 192.168.10.11:4661 readable, EEPROM unchanged |
| RAM-read fallback after Clear | Identical fixed 64-byte FC10 image with selected MAC/license; FC11=00 |

IP suffixes 16/17/11 in this section mean 192.168.10.16/17/11. Forced normal
programming follows the saved-IP endpoint rule, even when Check reports the
FC10 status as uninitialized. Thus the checkbox does not force a normal MPC
RAM copy. This now has real-LRTDC evidence for initialized, erased and FC10=80
states, in addition to the earlier synthetic normal reference.

### Check display and intentional interface differences

Real normal SiTCP with an MPCX file displayed `SiTCP 100/1000M - Mismatch.`.
The CLI previously omitted that final period. It now includes it only for
normal targets; the observed XG mismatch remains `Mismatch` without a period.
The current binary was rechecked on the board, with exit 7 and zero writes.
Native and CLI matched-file, fileless, mismatched and erased-state report text
was compared, preserving `EEOROM needs to be inbitialized.` verbatim.

The native checked Initialize box adds a blank line and
`EEPROM is forcibly initialized` to Check's dialog. The CLI intentionally
rejects `--check --initialize-eeprom` before access because Check and programming
are separate modes; its programming option reproduces the normal write behavior
above. Native fileless Check uses an error icon; successful CLI fileless reads
exit 0. Modal framing, icons and confirmation dialogs are not CLI features.

### Additional CLI cases and injected failures

Explicit 16-byte programming produced four data writes rather than the native
single 64-byte write, with the expected full result. Combined MPC programming
and `--set-eeprom-ip` correctly changed only the requested saved IP after
programming. The detailed reader left all snapshots unchanged.

Fallback was tested through a local proxy forwarding commands to the real
board. It shortened only the FF18/40 response delivered to the client from
40 to 39 payload bytes. Native showed its unreadable-RAM fallback warning;
native and CLI then wrote identical fixed images, including FC11=00 rather
than the FF retained on a successful RAM-copy path. The physical RAM was
healthy: this is an injected transport-response failure, not a reproduced
hardware RAM fault. With `--no-fallback`, CLI stopped without any write.

Six additional CLI fault cases covered lost enable ACK, lost MPC-data ACK,
lost IP-data ACK, short Check status, no response and strict no-fallback.
For ACK loss, the proxy forwarded the operation once, received its real ACK,
then withheld delivery once. Those three ACK-loss runs failed as expected,
sent no repeated data write, and attempted protection cleanup. The other three
fault cases stopped before any write, including protection commands. Full
results were verified and restored. The MPC-data case wrote the already-installed
payload, so packet
evidence establishes one accepted command, not an independently measurable
EEPROM cell-program event. The silent endpoint did not forward to hardware.
Native ACK-loss behavior was not freshly paired on LRTDC; earlier native
failure observations remain separate evidence. Do not claim identical failure
handling: bounded CLI errors, mandatory verification and cleanup are deliberate.

### Timing, restoration and evidence limits

Current normal MPC write-start gaps were 502.251 ms from enable to data and
1237.732 ms from data to protect. File programming waits 500 ms after ACK;
ACK latency and processing add time. Mandatory read-back follows protection.
Native protection can follow dismissal of
its success dialog, so its total duration includes operator interaction.
Current CLI Clear gaps ranged 489.684..511.392 ms. This confirms that the
existing Clear implementation does not guarantee a strict 500 ms minimum on
the physical wire; its separate policy was not changed by the Writer pacing
fix. Matching write bytes therefore does not imply matching timing.

After testing, all original runtime/64 and EEPROM/128 bytes were restored and
verified exactly, including runtime IP 16 and saved IP 17. Final ping was
10/10, loss 0%, reported RTT below 1 ms. Native Utility Read was refreshed at
the restored endpoint; temporary GUI write fields were reset. Capture and
fault proxies were stopped. No power cycling, JTAG, TCP throughput or prolonged
stability test was performed in this LRTDC pass. Earlier successful AMANEQ boots
do not establish LRTDC boot reliability or resolve the reported startup issue.

The current warning-enabled build passed all 55 regression tests in 171.814 s.
The small display fix changes no write algorithm. External sanitized result:
`outputs/sitcp-writer-debug/lrtdc-normal-comparison.json`; private capture and
snapshots: `work/sitcp-debug/lrtdc-normal-20261015/`. They are outside the source
repository; licensed packet data and backups must not be published.
