# Verification record: Normal SiTCP comprehensive hardware comparison (2026-10-15)

[Verification index](README.md) | [Current technical guide](../../FOR_DEVELOPERS.md)

These are dated observations for the stated builds and targets, not a current
specification or a new test run. Original conditions, results, recovery and
limitations are retained below. Source-relative references have been updated;
historical prose such as "above/below" refers to the original report order.
Capture/backup paths name private session storage, not files in this checkout.

## Normal SiTCP comprehensive hardware comparison (2026-10-15)

Tested code revision: `0297c7f5c5a078a2705177e624b282bcfb098e9e`.
Strict C++11 build/install and 45 integration tests passed in 75.397 seconds.
This session tested only the normal SiTCP 1G AMANEQ on the 192.168.10 network;
192.168.2.187 and SiTCP-XG were excluded. The board's default address is
192.168.10.16, while its starting and restored EEPROM/runtime address was
192.168.10.17, MAC 7C:F0:98:01:20:17, TCP 24 and UDP 4660.

Official MPC Writer 0.4.1-2-gc782 and SiTCP Utility 0.9.0 were exercised against
the real board with Npcap/dumpcap capture. RAM (64 bytes) and EEPROM (128 bytes)
were saved before and after each paired operation. Real license files, raw
images and raw captures remain private. The sanitized session report is
`normal-comprehensive-test.json` in the external deliverables directory.

| Paired operation | Write address/length/data sequence | Starting and final RAM/EEPROM |
| --- | --- | --- |
| MPC preserving settings | identical; enable, FC10/64, protect | identical |
| MPC initialization after Clear | identical; RAM settings copied into MPC image | identical |
| Clear | identical; enable, eight 16-byte FF blocks, protect | identical |
| EEPROM IP/TCP/UDP settings | identical; enable, FC00/17, FC18/40, protect | identical |
| Runtime IP/TCP/UDP settings | identical; FF00/17, FF18/40 | identical |

The IP settings comparisons changed IP to 192.168.10.18, TCP to 25 and UDP to
4661. Both programs' EEPROM changes were accepted after power cycling; runtime
changes were read back at the new IP/UDP endpoint and TCP 25 accepted a
connection. Runtime changes preserved EEPROM. For the official runtime test,
packets and register read-back establish the result; the GUI did not produce
a new completion log or update its target fields during observation. Do not
describe that GUI completion behavior as verified. Alternate licensed MAC
programming and explicit 16-byte MPC chunks were also exercised. The complete
session report contains 17 cases, including failure and reader tests.

### Differences and timing limits

The custom tools issue additional reads for independent generation detection,
snapshots and mandatory verification. Write-data equality does not mean the
entire UDP traffic sequence or user interface is identical. Normal MPC data
ACK latency was about 732 ms for both programs. The native success dialog
delays its protection write until dismissal; the observed roughly 24-second
gap included operator interaction and is not an EEPROM programming requirement.

Native Clear write-start gaps measured 501.569..516.942 ms; custom Clear gaps
measured 492.171..514.823 ms in this Windows/Cygwin capture. Source scheduling
uses a monotonic 500 ms interval, but this capture does not establish a strict
500 ms minimum on the wire. Clock resolution, scheduling and capture timestamp
effects have not been separated. This is an unresolved timing qualification;
the existing simulator tolerance is not proof of the stricter wire guarantee.

### Lost ACK and unreachable-device tests

A local UDP proxy forwarded each selected command once to the real board and
suppressed one actual response. No writes were blindly retried. Custom enable,
MPC-data and EEPROM-IP-data ACK loss returned failure, attempted protection
restoration, and received its protection ACK. Data already accepted by the
board was independently read back and the complete baseline was restored.
An unreachable local endpoint caused a bounded failure with zero write requests.
Protection confirmation means an acknowledged FCFF=FF write, not a read of the
write-only control register.

With the official Writer's enable ACK suppressed, one enable write and no data
write, retry, protection write or error dialog were observed over approximately
108 seconds. The window was then closed and protection explicitly restored
with an acknowledged command. This bounded observation does not prove that
the official program would wait forever or characterize other timeout settings.

### Power-cycle outage and final state

After the custom runtime migration test, a power cycle intended to restore
EEPROM settings produced no ARP/RBCP response. Default-mode relay selection and
another power cycle did not recover communication. A longer power-off was
confirmed at zero measured volts. FPGA JTAG status nevertheless showed DONE,
PLL lock and no CRC error. Reloading the existing flash configuration using
Vivado `boot_hw_device` recovered communication; no firmware file was programmed.
After recovery all 64 RAM bytes and 128 EEPROM bytes matched the starting
baseline exactly. The outage cause remains undetermined: temporal proximity to
the runtime test does not establish an IP-writer defect or EEPROM corruption.

After fault-test cleanup, the final read confirmed IP 192.168.10.17, MAC
7C:F0:98:01:20:17, TCP 24 and UDP 4660 in RAM and EEPROM. Final ping was 30/30
with no loss. Power and relay were left ON. This establishes the final observed
state, not a guarantee of stability across every subsequent power cycle.

### Follow-up power-cycle reproduction (2026-10-15)

Six additional captured trials did not reproduce the outage: three baseline
power cycles, then three cycles following custom runtime migration to
192.168.10.18/TCP 25/UDP 4661 and a TCP connection. Each cycle held output OFF
for three seconds, turned it ON, and first tested RBCP twelve seconds after
the ON command completed. All six answered the first probe; full RAM/64 and
EEPROM/128 comparisons matched the original baseline after every cycle.
No EEPROM writes or JTAG recovery were needed in this follow-up. Final state
remained 192.168.10.17, TCP 24, UDP 4660 with power/relay ON.

The sanitized external `boot-reproduction.json` records all six trials and the
private capture hash. This small negative series does not exclude an intermittent
fault. It repeats the immediate runtime-change/TCP/power sequence, not the
entire preceding clear, license, settings and repeated-boot history. These
results cannot establish whether the earlier failure originated in SiTCP,
board reset/clock logic, PHY initialization or power sequencing. Further
isolation requires a reproduced failure with evidence preserved before recovery.
