# Reverse engineering notes

This document records the evidence behind the SiTCP / SiTCP-XG behavior implemented by this project. Keep three evidence classes distinct:

1. public official Bee Beans Technologies documentation;
2. static analysis of the official Windows MPC Writer;
3. read-only or controlled verification against real MPC/MPCX files and hardware.

## Public references

- Bee Beans Technologies SiTCP downloads: https://www.bbtech.co.jp/download-files/sitcp/index_en.html
- SiTCP MPC Writer XG guide: https://www.bbtech.co.jp/download-files/sitcp/SiTCP-MPC-Writer-XG-en.0.1.1.pdf
- Bee Beans Technologies `sitcpy`: https://github.com/BeeBeansTechnologies/sitcpy
- SiTCP Forum: https://sitcp.bbtech.co.jp/

Public documentation establishes EEPROM/register access and the high-level writer workflow, but a public byte-level specification for the complete 22-byte MPC/MPCX payload/classifier was not found in the material reviewed on 2026-09-03.

## RBCP recovered/confirmed behavior

The writer uses an 8-byte RBCP header:

```text
FF CMD ID LEN ADDR[31:0]
```

with `0xC0` for read and `0x80` for write.

Static analysis of `SiTcpMpcWriteXG.exe 0.4.1-2-gc782` identified the RBCP write routine around `0x409960` and read routine around `0x409e50`.

## EEPROM

The relevant EEPROM window begins at:

```text
0xFFFFFC00
```

EEPROM write protection control is:

```text
0xFFFFFCFF = 0x00  enable writes
0xFFFFFCFF = 0xFF  disable writes
```

This active-low behavior agrees with public SiTCP-XG documentation.

The official writer clear path was observed to fill `FC00..FC7F` with `FF` in 16-byte writes while EEPROM writing is enabled, then restore protection.

## MPC/MPCX payload classifier

The selected payload must be exactly 22 (`0x16`) bytes.

Recovered classifier behavior:

```text
normal-SiTCP candidate:
  payload[6:13]
  subtract 0x34 from every non-zero byte
  if the resulting seven-byte tag is valid -> internal type 2

SiTCP-XG candidate:
  payload[0:7]
  subtract 0x2C from every non-zero byte
  if the resulting seven-byte tag is valid -> internal type 1
```

The normal candidate is tested first. Valid tag characters include NUL, space, hyphen, digits, and ASCII letters. Known matching files decode to `Other  `.

Therefore the filename extension is not authoritative. Detection must use the file contents.

## Verified SiTCP-XG mapping

Static analysis plus matching real file/device verification supports:

```text
payload[0:16]  -> EEPROM FC00..FC0F
FC10..FC11     -> preserve current device bytes
payload[16:22] -> EEPROM FC12..FC17
```

The final six payload bytes correspond to the target MAC in the verified pair. FC10..FC11 are not fixed constants and must be preserved.

## Verified normal SiTCP mapping

Matching file/device verification supports:

```text
payload[0:6]  -> EEPROM FC12..FC17
payload[6:22] -> EEPROM FC40..FC4F
```

This is a separate layout from SiTCP-XG and must remain a distinct implementation path.

## Device generation detection

The current implementation reconstructs both possible 22-byte payloads from EEPROM and runs the recovered classifier. If only one mapping is valid, that identifies the target generation.

Some EEPROM contents can make both mappings appear valid. The current read-only disambiguation probe is:

```text
0xFFFFFF50
```

Observed behavior used by this project:

```text
readable       -> SiTCP-XG
RBCP bus error -> normal SiTCP
timeout        -> unresolved
```

This should continue to be treated as reconstructed/observed behavior rather than a general public specification unless stronger documentation is found.

## Write sequence used by this project

For MPC/MPCX programming:

1. read the current EEPROM image needed for the target generation;
2. preserve bytes that must not be replaced;
3. write `00` to FCFF to release EEPROM write protection;
4. write the image in 16-byte blocks;
5. write `FF` to FCFF to restore protection, including error paths where possible;
6. read the programmed image again;
7. compare byte-for-byte and fail on any mismatch.

Reads use small chunks and may retry timeouts. Writes are not blindly retried after timeout because UDP acknowledgement loss does not prove the EEPROM operation did not occur.

## IP configuration status

The repository now contains IP reader/writer command names, but IP write behavior is not yet considered verified.

Before enabling destructive IP writing, determine independently for both normal SiTCP and SiTCP-XG:

- exact EEPROM bytes representing the configured/default IP;
- exact current/runtime register bytes representing the active IP;
- whether runtime IP changes take effect immediately;
- what happens to the existing RBCP path after changing the active address;
- whether and how the utility should reconnect to the new IP for read-back verification;
- any checksum, reset, reload, or write-protection requirements;
- differences between generations.

Until then, `sitcp-sitcpxg-ip-writer` must refuse destructive writes.

## Evidence summary

| Item | Public docs | Writer analysis | Hardware/file verification |
| --- | --- | --- | --- |
| RBCP access | yes | yes | yes |
| EEPROM FC00..FCFF | yes | yes | yes |
| FCFF=00 releases protection | yes | yes | consistent |
| payload exactly 22 bytes | not found | yes | yes |
| content classifier | not found | yes | yes |
| XG 16 + preserve 2 + 6 mapping | not found | yes | yes |
| normal 6 + 16 mapping | not found in reviewed material | investigated | yes |
| filename extension controls type | no | no | no |
| runtime IP write mapping | incomplete | not yet complete | not yet verified |

## Remaining work

- verify exact semantic meaning of all license bytes;
- determine the meaning of preserved XG FC10..FC11 bytes;
- strengthen target-generation detection;
- reconstruct and verify current/runtime IP registers for both generations;
- reconstruct and verify EEPROM IP write behavior;
- test reconnect/read-back after a runtime IP change;
- compare the C++ readers against known hardware and the previous Python implementation.
