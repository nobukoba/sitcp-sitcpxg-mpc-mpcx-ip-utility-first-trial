# Verification records

[Current technical guide](../../FOR_DEVELOPERS.md) | [English README](../../README.md) | [日本語 README](../../README.ja.md)

Current behavior and known limits belong in `FOR_DEVELOPERS.md`. This directory
stores dated test and investigation records. A later correction does not make
an earlier tested revision behave differently. Synthetic, static-analysis and
physical-device evidence must remain distinguishable.

| Record | Scope |
| --- | --- |
| [2026-10-15-lrtdc-normal.md](2026-10-15-lrtdc-normal.md) | LRTDC normal SiTCP; Check, initialization, Clear, settings, injected failures; 40 cases / 14 pairs; no reboot |
| [2026-10-15-xg-modes.md](2026-10-15-xg-modes.md) | AMANEQ XG Check/forced-mode and display rechecks; separate pacing evidence |
| [2026-10-15-amaneq-xg.md](2026-10-15-amaneq-xg.md) | Updated AMANEQ XG; 19-case comparison, eight boot observations, ACK loss and restoration |
| [2026-10-15-amaneq-normal.md](2026-10-15-amaneq-normal.md) | Earlier normal AMANEQ; 17 cases, outage/recovery and six follow-up boot trials |
| [2026-10-15-fallback.md](2026-10-15-fallback.md) | Normal fallback table/FC11 correction and synthetic native XG RAM-failure investigation |
| [2026-10-15-software-validation.md](2026-10-15-software-validation.md) | Option implementation, build/test stages, document and readability audits; no new hardware run |
| [2026-10-13-native-reference.md](2026-10-13-native-reference.md) | Synthetic normal MPC / Utility captures, static register evidence and reader validation |
| [2026-10-14-implementation-validation.md](2026-10-14-implementation-validation.md) | Clear pacing, bulk transfer default, optional extensions, ports and historical validation stages |
| [2026-10-14-hardware.md](2026-10-14-hardware.md) | Earlier XG preservation/initialization, EEPROM IP and both-generation port comparisons |
| [xg-register-observation.md](xg-register-observation.md) | Historical XG FC40..FC4F observation; write-history hypothesis remains unproven |

## What to record

For new runs, record the date, board/firmware identity when known, tool versions,
source revision or hashes (including dirty-tree status), network endpoint,
starting state/backups and authorization scope. Separate procedures, expected
results, observations, failures, restoration and untested paths. State whether
evidence is physical capture, simulator traffic, static analysis or public docs.
Never turn a negative reproduction series into proof that an intermittent bug
does not exist. Link the conclusion back from the current technical guide.

## Private raw evidence

These Markdown records are sanitized repository documentation. External JSON
reports mentioned in them may be absent from a source checkout. Raw MPC/MPCX
files, license-bearing packet captures, RAM/EEPROM backups, proprietary tools,
credentials and disassembly are not stored here or committed to Git. Paths and
hashes identify evidence; they do not grant access or establish a new test run.
