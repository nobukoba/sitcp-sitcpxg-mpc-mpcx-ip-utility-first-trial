# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

**Language: English | [日本語](README.ja.md)**

Experimental C++11 utilities for SiTCP and SiTCP-XG configuration over RBCP.

## Commands

The installed command set contains five commands:

- `mpc-mpcx-ip-writer` — write MPC/MPCX EEPROM data and optionally change EEPROM/current IP addresses.
- `mpc-mpcx-ip-reader` — read MPC/MPCX information and always display current/EEPROM MAC and IP addresses.
- `mpc-mpcx-ip-command` — advanced MPC/MPCX, IP, and low-level RBCP operations.
- `sitcp-sitcpxg-ip-writer` — IP-only writer for SiTCP / SiTCP-XG.
- `sitcp-sitcpxg-ip-reader` — IP-only reader for SiTCP / SiTCP-XG.

## Quick installation

```bash
git clone https://github.com/nobukoba/sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial.git
cd sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial
make
make install
```

`make install` copies the executables into `./bin/`.

## How to use the writer

Write an MPC/MPCX file to the device:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx
```

Write the file and set the saved EEPROM IP address:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-eeprom-ip 192.168.2.170
```

Clear the EEPROM only, without writing a file:

```bash
./bin/mpc-mpcx-ip-writer 192.168.10.10 --clear
```

`--clear` erases the saved license and settings. Use it on its own, without a
file or IP-change options. Reprogram the appropriate license before normal use.

## How to use the reader

```bash
./bin/mpc-mpcx-ip-reader 192.168.2.161
```

Displays runtime and EEPROM contents separately, including MAC/IP addresses
and MPC/MPCX information. See the [output and exit codes](#reader-output-and-exit-codes) for report details.

## How to use the IP-only commands

Use these when only SiTCP / SiTCP-XG IP configuration is needed and no MPC/MPCX file should be involved:

```bash
./bin/sitcp-sitcpxg-ip-reader 192.168.2.161
./bin/sitcp-sitcpxg-ip-writer 192.168.2.161 192.168.2.170
```

These commands do not read or rewrite MPC/MPCX license data.

## How to use the advanced command

```bash
./bin/mpc-mpcx-ip-command --help
```

For diagnostics and low-level operations, see the [developer command reference](FOR_DEVELOPERS.md#advanced-command-reference).

## Appendix

### Writer options and behavior

```text
mpc-mpcx-ip-writer CURRENT_IP MPC_OR_MPCX_FILE [options]
```

Write MPC/MPCX information and also set the current/runtime IP:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-current-ip 192.168.2.170
```

Set both EEPROM/default and current/runtime IP addresses:

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-eeprom-ip 192.168.2.170 \
  --set-current-ip 192.168.2.170
```

Default RBCP UDP port is `4660`; default timeout is `3` seconds. These defaults are also shown by `--help`.

Writer options:

```text
--clear              Clear EEPROM only; no file/IP changes (default: off)
--set-eeprom-ip IP   Set EEPROM/default IP address
--set-current-ip IP  Set current/runtime IP address
--port N             RBCP UDP port (default: 4660)
--timeout SEC        RBCP timeout in seconds (default: 3)
-h, --help           Show help
```

When both options are given, EEPROM IP is set first and current IP last. The writer reconnects to the new IP and verifies the result.

The writer displays runtime and EEPROM MAC/IP before and after the operation. Success is printed only after all requested operations and verification complete. Use the reader for full register dumps. MPC/MPCX payload type is determined from the 22-byte contents, not the filename extension.

### MPCX writing after clearing EEPROM

Preview the planned changes without writing:

```bash
./bin/mpc-mpcx-ip-command mpcx-plan DEVICE_IP FILE.mpcx
```

Then program the device:

```bash
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx
```

Initialization saves the current runtime IP in EEPROM. Use `--set-eeprom-ip` to save a different IP. If the required configuration cannot be read completely, initialization stops before writing. See the [developer guide](FOR_DEVELOPERS.md#mpcx-initialization-source-and-limits-2026-09-26) for initialization conditions, byte ranges, and differences from the official tool.

### Reader output and exit codes

Runtime and EEPROM MAC/IP values and MPC/MPCX information are displayed separately. `??` marks unreadable bytes; `unavailable` marks fields with missing data.

| Exit code | Meaning |
| --- | --- |
| `0` | Complete read |
| `3` | Partial read |
| `1` | Fatal error, such as a timeout or short reply |

The same detailed report is available with:

```bash
./bin/mpc-mpcx-ip-command read 192.168.2.161
```

See the [developer guide](FOR_DEVELOPERS.md#runtime--eeprom-diagnostic-reports) for register ranges, formatting, generation detection, and bus-error handling.

### Build requirements

- C++11 compiler (`g++` or `clang++`)
- POSIX sockets
- `make`

The default build uses `-std=c++11`. Targets are Linux, macOS, and WSL.

### Installation options and updates

The default installation is `./bin/` in the checkout and needs no administrator privileges. Running `make` alone does not refresh installed copies.

After updating the source, rebuild and refresh the installation:

```bash
git pull --ff-only
make install
```

`make install` builds any outdated binaries before copying them.

To choose another installation prefix:

```bash
make install PREFIX="$HOME/.local"
"$HOME/.local/bin/mpc-mpcx-ip-reader" DEVICE_IP
```

If `$HOME/.local/bin` is on your `PATH`, you can also run the command by name.
With a custom prefix, replace `./bin/` in the examples with your
chosen `PREFIX/bin/`. For a system installation, use
`sudo make install PREFIX=/usr/local` (administrator privileges required).

### Official links and references

- [Bee Beans Technologies](https://www.bbtech.co.jp/)
- [SiTCP / SiTCP-XG software and manuals](https://www.bbtech.co.jp/download-files/sitcp/index_en.html)
- [SiTCP MPC Writer XG User Guide (English PDF)](https://www.bbtech.co.jp/download-files/sitcp/SiTCP-MPC-Writer-XG-en.0.1.1.pdf)
- [SiTCP Forum](https://sitcp.bbtech.co.jp/)

### Documentation

- [For developers](FOR_DEVELOPERS.md) — architecture, build/development notes, technical evidence, MPC/MPCX EEPROM mappings, device-generation detection, references, and testing.
- [Agent instructions](AGENTS.md) — constraints for automated development.
