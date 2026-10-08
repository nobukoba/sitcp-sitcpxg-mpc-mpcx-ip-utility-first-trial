# SiTCP / SiTCP-XG MPC / MPCX / IP Utility (first trial)

**Language: [English](README.md) | 日本語**

RBCPを使ってSiTCP / SiTCP-XGを設定する、試験的なC++11ユーティリティです。

## コマンド

インストールされるコマンドは次の5つです。

- `mpc-mpcx-ip-writer` — MPC/MPCXのEEPROMデータを書き込み、必要に応じてEEPROMと現在のIPアドレスを変更します。
- `mpc-mpcx-ip-reader` — MPC/MPCX情報を読み取り、現在とEEPROMのMAC/IPアドレスを表示します。
- `mpc-mpcx-ip-command` — MPC/MPCX、IP設定、低レベルRBCP操作の詳細コマンドです。
- `sitcp-sitcpxg-ip-writer` — SiTCP / SiTCP-XGのIP設定専用writerです。
- `sitcp-sitcpxg-ip-reader` — SiTCP / SiTCP-XGのIP設定専用readerです。

## インストール

```bash
git clone https://github.com/nobukoba/sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial.git
cd sitcp-sitcpxg-mpc-mpcx-ip-utility-first-trial
make
make install
```

`make install`で実行ファイルが`./bin/`にコピーされます。

## writerの使い方

MPC/MPCXファイルを装置に書き込みます。

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx
```

ファイルを書き込み、EEPROMに保存するIPアドレスも設定します。

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-eeprom-ip 192.168.2.170
```

ファイルを書き込まず、EEPROMの消去だけを行います。

```bash
./bin/mpc-mpcx-ip-writer 192.168.10.10 --clear
```

`--clear`は保存されたライセンスと設定を消去します。ファイルやIP変更オプションと組み合わせず、単独で使用してください。通常の使用に戻す前に、適切なライセンスを再書き込みしてください。

## readerの使い方

```bash
./bin/mpc-mpcx-ip-reader 192.168.2.161
```

ランタイムとEEPROMの内容を分けて表示します。MAC/IPアドレスとMPC/MPCX情報を確認できます。表示の詳細は[表示と終了コード](#readerの表示と終了コード)を参照してください。

## IP設定専用コマンドの使い方

MPC/MPCXファイルを使用せず、SiTCP / SiTCP-XGのIP設定だけを扱う場合に使用します。

```bash
./bin/sitcp-sitcpxg-ip-reader 192.168.2.161
./bin/sitcp-sitcpxg-ip-writer 192.168.2.161 192.168.2.170
```

MPC/MPCXのライセンスデータは読み取ったり書き換えたりしません。

## 詳細コマンドの使い方

```bash
./bin/mpc-mpcx-ip-command --help
```

診断や低レベル操作については[開発者ガイドのコマンド一覧](FOR_DEVELOPERS.md#advanced-command-reference)を参照してください。

## 付録

### writerのオプションと動作

```text
mpc-mpcx-ip-writer CURRENT_IP MPC_OR_MPCX_FILE [options]
```

MPC/MPCX情報を書き込み、現在のランタイムIPも設定します。

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-current-ip 192.168.2.170
```

EEPROMの既定IPと現在のランタイムIPの両方を設定します。

```bash
./bin/mpc-mpcx-ip-writer 192.168.2.161 FILE.mpcx \
  --set-eeprom-ip 192.168.2.170 \
  --set-current-ip 192.168.2.170
```

RBCPの既定UDPポートは`4660`、タイムアウトは`3`秒です。`--help`にも既定値が表示されます。

| オプション | 内容 |
| --- | --- |
| `--clear` | EEPROM消去のみ。ファイル書き込みやIP変更なし（既定: 無効） |
| `--set-eeprom-ip IP` | EEPROMの既定IPを設定 |
| `--set-current-ip IP` | 現在のランタイムIPを設定 |
| `--port N` | RBCP UDPポート（既定: 4660） |
| `--timeout SEC` | RBCPタイムアウト秒数（既定: 3） |
| `-h, --help` | ヘルプを表示 |

両方を指定すると、EEPROMのIPを先に設定し、現在のIPを最後に変更します。変更後のIPへ再接続して検証します。

writerは変更前後のランタイム/EEPROMのMAC・IPを表示し、すべての操作と検証が完了すると成功を表示します。全レジスタのダンプはreaderで確認してください。MPC/MPCXの種類は拡張子ではなく22バイトの内容から判定します。

### EEPROM消去後のMPCX書き込み

書き込み前に、変更予定の内容を確認できます。

```bash
./bin/mpc-mpcx-ip-command mpcx-plan DEVICE_IP FILE.mpcx
```

確認後に書き込みます。

```bash
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx
```

初期化で保存されるEEPROM IPは現在のランタイムIPです。別のIPを保存する場合は`--set-eeprom-ip`を指定してください。必要な設定を完全に読み取れない場合は、書き込み前に停止します。初期化条件、コピー範囲、公式ツールとの差異は[開発者ガイド](FOR_DEVELOPERS.md#mpcx-initialization-source-and-limits-2026-09-26)を参照してください。

### readerの表示と終了コード

ランタイムとEEPROMのMAC/IP、MPC/MPCX情報を分けて表示します。`??`は読み取れなかったバイト、`unavailable`は必要なデータが揃わない項目です。

| 終了コード | 意味 |
| --- | --- |
| `0` | 完全な読み取り |
| `3` | 一部だけ読み取れた状態 |
| `1` | タイムアウトや短い応答などの致命的エラー |

同じ詳細表示は次のコマンドでも確認できます。

```bash
./bin/mpc-mpcx-ip-command read 192.168.2.161
```

レジスタ範囲、表示書式、世代判定、バスエラー時の処理は[開発者ガイド](FOR_DEVELOPERS.md#runtime--eeprom-diagnostic-reports)を参照してください。

### ビルド要件

- C++11対応コンパイラ（`g++`または`clang++`）
- POSIXソケット
- `make`

既定のビルドは`-std=c++11`を使います。対象環境はLinux、macOS、WSLです。

### インストール先と更新

既定のインストール先はリポジトリ内の`./bin/`で、管理者権限は不要です。`make`だけではインストール済みコピーは更新されません。

ソースを更新したら、再ビルドしてインストール済みコピーも更新します。

```bash
git pull --ff-only
make install
```

`make install`は古くなったバイナリをビルドしてからコピーします。

別のインストール先を選ぶ場合:

```bash
make install PREFIX="$HOME/.local"
"$HOME/.local/bin/mpc-mpcx-ip-reader" DEVICE_IP
```

`$HOME/.local/bin`にPATHを通せば、コマンド名だけで実行できます。独自のprefixを指定した場合、例の`./bin/`を選択した`PREFIX/bin/`に置き換えてください。システム全体へのインストールは`sudo make install PREFIX=/usr/local`で行えます（管理者権限が必要です）。

### 公式リンク・参考資料

- [Bee Beans Technologies](https://www.bbtech.co.jp/)
- [SiTCP / SiTCP-XG ソフトウェア・マニュアル](https://www.bbtech.co.jp/download-files/sitcp/index_en.html)
- [SiTCP MPC Writer XG ユーザーガイド（英語PDF）](https://www.bbtech.co.jp/download-files/sitcp/SiTCP-MPC-Writer-XG-en.0.1.1.pdf)
- [SiTCP Forum](https://sitcp.bbtech.co.jp/)

### 関連ドキュメント

- [開発者向けガイド（英語）](FOR_DEVELOPERS.md) — 構成、ビルド/開発、技術的根拠、MPC/MPCXのEEPROM配置、装置の世代判定、参考資料、テスト。
- [AIエージェント向け指示（英語）](AGENTS.md) — 自動開発で守る制約。
