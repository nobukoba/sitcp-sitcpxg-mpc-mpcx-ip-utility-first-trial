# Reverse engineering notes

This document records the evidence behind behavior implemented by this project. Two independent functional domains are covered here:

1. MPC/MPCX license/configuration handling.
2. SiTCP Utility compatible IP-address handling.

Do not infer IP behavior from MPC/MPCX EEPROM layout merely because both concern SiTCP/SiTCP-XG.

For each domain, keep three evidence classes distinct:

1. public official Bee Beans Technologies documentation;
2. static analysis of the relevant official Windows utility;
3. read-only or controlled verification against real files/hardware.

## Public references

- Bee Beans Technologies SiTCP downloads: https://www.bbtech.co.jp/download-files/sitcp/index_en.html
- SiTCP MPC Writer XG guide: https://www.bbtech.co.jp/download-files/sitcp/SiTCP-MPC-Writer-XG-en.0.1.1.pdf
- Bee Beans Technologies `sitcpy`: https://github.com/BeeBeansTechnologies/sitcpy
- SiTCP Forum: https://sitcp.bbtech.co.jp/

## MPC/MPCX: RBCP recovered/confirmed behavior

The MPC Writer uses an 8-byte RBCP header:

```text
FF CMD ID LEN ADDR[31:0]
```

with `0xC0` for read and `0x80` for write.

Static analysis of `SiTcpMpcWriteXG.exe 0.4.1-2-gc782` identified the RBCP write routine around `0x409960` and read routine around `0x409e50`.

## MPC/MPCX: EEPROM

The relevant EEPROM window begins at:

```text
0xFFFFFC00
```

EEPROM write protection control observed for MPC/MPCX programming is:

```text
0xFFFFFCFF = 0x00  enable writes
0xFFFFFCFF = 0xFF  disable writes
```

The official MPC Writer clear path was observed to fill `FC00..FC7F` with `FF` in 16-byte writes while EEPROM writing is enabled, then restore protection.

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

The filename extension is therefore not authoritative; detection must use file contents.

## MPC/MPCX verified SiTCP-XG mapping

Static analysis plus matching real file/device verification supports:

```text
payload[0:16]  -> EEPROM FC00..FC0F
FC10..FC11     -> preserve current device bytes
payload[16:22] -> EEPROM FC12..FC17
```

The final six payload bytes correspond to the target MAC in the verified pair. FC10..FC11 are not fixed constants and must be preserved.

## MPC/MPCX verified normal SiTCP mapping

Matching file/device verification supports:

```text
payload[0:6]  -> EEPROM FC12..FC17
payload[6:22] -> EEPROM FC40..FC4F
```

This is a separate layout from SiTCP-XG and must remain a distinct MPC/MPCX implementation path.

## MPC/MPCX device generation detection

The current MPC/MPCX implementation reconstructs both possible 22-byte payloads from EEPROM and runs the recovered classifier. If only one mapping is valid, that identifies the target generation.

If both appear valid, the current read-only disambiguation probe is:

```text
0xFFFFFF50
```

Observed behavior used by this project:

```text
readable       -> SiTCP-XG
RBCP bus error -> normal SiTCP
timeout        -> unresolved
```

This is MPC/MPCX-side detection logic. It must not automatically be reused as the design basis for the IP-only utility unless independently verified there.

## MPC/MPCX write sequence used by this project

1. read the current EEPROM image needed for the target generation;
2. preserve bytes that must not be replaced;
3. release EEPROM write protection;
4. write the image in 16-byte blocks;
5. restore protection, including error paths where possible;
6. read the programmed image again;
7. compare byte-for-byte and fail on mismatch.

Reads may retry timeouts. Destructive writes are not blindly retried after timeout because UDP acknowledgement loss does not prove that the write did not occur.

# SiTCP Utility IP-address domain

The IP reader/writer in this repository are intended to reproduce only the IP-address functionality of the SiTCP Utility. This is a separate reverse-engineering task from MPC/MPCX.

The IP tools must not classify, reconstruct, validate, or modify MPC/MPCX license payloads merely to read or change an IP address.

The previously attempted implementation that reused the MPC/MPCX reader was incorrect and has been removed.

## IP behavior to verify

Determine independently for normal SiTCP and SiTCP-XG:

- how the SiTCP Utility discovers/contacts the device;
- how it reads the current/runtime IP;
- how it reads the EEPROM/default IP;
- how it writes the EEPROM/default IP;
- how it changes the current/runtime IP without changing EEPROM when requested;
- whether a current/runtime change takes effect immediately;
- whether the existing connection becomes invalid after the change;
- how the official utility reconnects or verifies the new address;
- whether checksums, resets, reload commands, write-enable sequences, or generation-specific commands are involved;
- whether normal SiTCP and SiTCP-XG use different mechanisms.

Until these are verified, both IP source files remain non-destructive placeholders.

## Intended IP CLI behavior after verification

```text
sitcp-sitcpxg-ip-reader CURRENT_IP [options]
sitcp-sitcpxg-ip-writer CURRENT_IP NEW_IP [options]
```

Design requirements:

- reader reports current/runtime and EEPROM/default IP values;
- writer changes only IP-related configuration;
- EEPROM is the default write target;
- changing current/runtime IP requires an explicit option;
- writer performs read-back verification;
- after changing current/runtime IP, reconnect to `NEW_IP` and verify when possible.

## Evidence summary

| Item | Public docs | Utility/static analysis | Hardware verification |
| --- | --- | --- | --- |
| MPC/MPCX RBCP access | yes | yes | yes |
| MPC/MPCX EEPROM FC00..FCFF | yes | yes | yes |
| MPC/MPCX payload exactly 22 bytes | not found | yes | yes |
| MPC/MPCX content classifier | not found | yes | yes |
| XG 16 + preserve 2 + 6 mapping | not found | yes | yes |
| normal 6 + 16 mapping | not found in reviewed material | investigated | yes |
| IP-only current/runtime read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM/default read mechanism | to investigate | to investigate | not yet verified |
| IP-only EEPROM write mechanism | to investigate | to investigate | not yet verified |
| IP-only runtime write/reconnect | to investigate | to investigate | not yet verified |

## Remaining work

### MPC/MPCX

- verify exact semantic meaning of all license bytes;
- determine the meaning of preserved XG FC10..FC11 bytes;
- strengthen target-generation detection;
- compare the C++ readers/command against known hardware and the previous Python implementation.

### IP utility

- identify and analyze the official SiTCP Utility behavior specific to IP changes;
- implement an IP-only reader after read mechanisms are verified;
- implement EEPROM IP write with read-back;
- implement optional current/runtime IP change;
- verify reconnect/read-back at the new IP;
- test normal SiTCP and SiTCP-XG independently.
