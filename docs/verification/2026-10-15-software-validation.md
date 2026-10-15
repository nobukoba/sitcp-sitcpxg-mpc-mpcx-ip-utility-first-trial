# Verification record: Check and forced initialization options

[Verification index](README.md) | [Current technical guide](../../FOR_DEVELOPERS.md)

These are dated observations for the stated builds and targets, not a current
specification or a new test run. Original conditions, results, recovery and
limitations are retained below. Source-relative references have been updated;
historical prose such as "above/below" refers to the original report order.
Capture/backup paths name private session storage, not files in this checkout.

## Check and forced initialization options

Latest pacing update: MPC and MPCX file programming now wait 500 ms after
each enable, data-chunk, optional-extension and protection ACK. Failed
enable/data transactions wait 500 ms before cleanup protection; data is never
retried. This supersedes the older ACK-paced XG timing observations below.
The wait is after ACK, so the time between write starts can exceed 500 ms.
Reads, standalone IP writing and Clear retain their separate timing policies.
A fresh real-XG capture measured enable-to-data 502.312 ms and data-to-protect
1765.535 ms, with successful image verification and restoration of the original
EEPROM. This establishes the measured case, not identical native elapsed time
or a guarantee about every OS/device. Private capture is in
`work/sitcp-debug/physical-192-168-10-10-xg/pacing-recheck/modes.pcapng`;
external summary is `outputs/sitcp-writer-debug/xg10-pacing-recheck.json`.
Validation stages and the final 55-test result are centralized in
[Testing](../../FOR_DEVELOPERS.md#testing); physical mode and pacing evidence is recorded separately.

`mpc-mpcx-ip-writer IP [FILE] --check [--port N] [--timeout SEC]` is a read-only
mode (default off). Before network access an optional file is validated as a
known 22-byte MPC/MPCX payload. It reads the independent generation identifier
at FF08/4 and the EEPROM FC10/1 status flag, then prints target type, status
flag and, if supplied, decoded file tag and Matched/Mismatch. No MAC/IP snapshots,
license programming, Clear, protect command or full EEPROM verification occurs.
No file is required in either the CLI or the tested native GUI.
Exit 0 means successful reads and optional generation match, 7 means mismatch,
1 means a file/read/option error; missing required CLI arguments use exit 2.
This does not establish cryptographic license validity or byte-for-byte equality
with the installed license. A mismatch still causes zero writes.

Native Check was observed on both synthetic generations: exactly FF08/4 and
FC10/1 requests, no writes. It classified the selected file (Other), displayed
the interface generation and match, and reported initialization from FC10 bit7.
For normal SiTCP a valid saved IP with FC10=80 was reported as needing
initialization, even though the normal writer uses its separate IP endpoint
rule. The EEPROM message describes this status flag, not the automatic
normal-MPC programming decision. Native Check
also describes a checked Initialize box as forced; that wording alone does
not establish the subsequent normal-MPC write behavior.

`--initialize-eeprom` is a programming option (default off), requires a file,
and maps the native checkbox on the tested XG path. It forces the 80-byte
RAM-derived image even with FC10 bit7 clear. It does not Clear first. Shared
preparation still validates the entire RAM identifier/control image, then overlays
the file, writes/verifies 80 bytes and handles optional FF50/60/70 blocks as
before. Read failures remain fatal on XG, regardless of --no-fallback. Runtime
settings remain unchanged until an explicitly requested current-IP operation.
Existing transfer sizing and saved/current IP overrides remain available.
Mandatory protection cleanup and no destructive retry are unchanged.

The native checkbox was also exercised on normal SiTCP with saved IP 192.0.2.10
and FC10=80. Despite Check's forced wording, the actual write read no FF18/40,
preserved FC11 and saved settings, set FC10=01 and overlaid only the MPC data.
The CLI matches that observed behavior: --initialize-eeprom is accepted but has
no additional effect on normal MPC. Its IP endpoint automatic rule and normal
fallback/--no-fallback remain unchanged. Do not describe this option as forcing
normal-MPC RAM copies or factory defaults.

Independent synthetic native fixtures are
`tests/reference/official_xg_forced.json` and
`tests/reference/official_normal_forced.json`; they start with different RAM and
EEPROM settings, with EEPROM already initialized for XG. Tests compare full
128-byte results and exact base writes. The original trace includes native
Check plus the write operation; it is not a claim that all CLI read packets
or native GUI delays are identical. These fixtures are loopback-only; the
real-board recheck below separately verifies XG forced programming, and the
later LRTDC comparison above verifies the normal checked-box paths.

--check rejects Clear and all programming/IP options, including
--initialize-eeprom, --no-fallback and explicit chunk sizing. --clear rejects
--check and --initialize-eeprom. All these conflicts are rejected before access.
Standalone IP commands and advanced mpcx-plan gain neither new flag. Ordinary
file programming output remains compact; Check prints a distinct read-only report
and does not print the writer's success message. No extra CLI confirmation
prompt is introduced by the force option.

```bash
./bin/mpc-mpcx-ip-writer DEVICE_IP --check
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx --check
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx --initialize-eeprom
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx --initialize-eeprom \
  --write-chunk-size official --set-eeprom-ip 192.168.10.11
```

## Documentation consistency audit (2026-10-15)

The English/Japanese READMEs, this guide and AGENTS.md were reread against the
writer parsers, shared initialization/transport helpers and installed help for
all five commands. The stale README instruction to stop by default on normal
RAM failure was corrected in both languages. The option reference now records
defaults, units, applicability, examples, Clear conflicts, IP override order,
non-atomic failure behavior and the distinction between current and saved ports.
Detailed chunk-size examples stay here and in CLI help; README omits them.
Historical RAM-failure, migration, boot and timing claims are marked by their
validation stage, with the FC11 fallback exception and current results explicit.

The initial audit checked 31 local links and followed the 46-test fallback
validation. Later audits include the new Check/initialization links and reports.
The final pre-push audit rechecks all five installed --help invocations, option
coverage, every local Markdown link, fence balance, matching README examples,
requested appendix placement and `git diff --check`. The external
`documentation-consistency-audit.json` records exact counts, help text and
document hashes. Current code-validation results are recorded in the status
section below. No hardware operations are part of this documentation audit.

## Validation stages for the current options

After the LRTDC comparison, the source-readability review rebuilt and installed
all five commands with C++11 and `-Wall -Wextra -Wpedantic`, with no warnings.
All 55 tests passed in 157.289 seconds. All five installed help outputs matched
their pre-formatting text exactly. Document checks covered 32 local links,
identical English/Japanese README command examples and requested appendix
placement; `git diff --check` passed. No hardware access occurred in this review.

The final pre-push run passed all 55 tests in 157.795 seconds after a
warning-enabled C++11 build/install. It includes read-only Check with/without a
file, exact native-style reports, mismatches and conflicting options, native
normal/XG forced-mode reference equality, forced split writes with EEPROM IP
override, and successful/failing 500 ms programming intervals. Existing Clear,
fallback, IP/port, protection and diagnostic tests also pass.

| Earlier stage | Result |
| --- | --- |
| Initial Check/force options | 52 tests passed, 96.978 seconds |
| Native file-report update | 54 tests passed, 88.419 seconds |
| First pacing run | Existing 54 passed; new test's fault target was outside its preservation write range |
| Corrected pacing fixture: forced initialization with split writes | All 9 writer-mode tests passed, 24.641 seconds; no production fix needed |
| Fileless native-report correction | All 9 writer-mode tests passed, 26.468 seconds |

These are local UDP simulator/reference tests, not additional physical test
cases. The new-mode and pacing sections above separately identify real-device
captures and restoration checks. No proprietary license is needed for the tests.


## Source readability review (2026-10-15)

The 2026-10-15 readability review covered all five command sources and six
shared headers. Long argument lists, nested conditional expressions and help
string literals were split across source lines; writer locals were given
descriptive names. Adjacent C++ string literals preserve the exact help text,
including its output newlines. Reader section separators, decimal/hex values,
raw dumps, compact writer output and programming order are retained.
All 11 files have a final newline, no trailing whitespace or tabs, no bare CR,
consistent line endings within each file, and lines shorter than 100 columns.
The source-tree formatting limit remains 100 in `.clang-format`.
This review performs no device writes; earlier physical captures describe
their saved source revisions rather than a new hardware run of the formatting.
