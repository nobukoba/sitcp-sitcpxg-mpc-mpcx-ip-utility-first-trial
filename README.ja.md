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

ランタイムとEEPROMの内容を分けて表示します。MAC/IPアドレスとMPC/MPCX情報を確認できます。表示の詳細は[付録](#付録)を参照してください。

## IP設定専用コマンドの使い方

MPC/MPCXファイルを使用せず、SiTCP / SiTCP-XGのIP設定だけを扱う場合に使用します。

```bash
./bin/sitcp-sitcpxg-ip-reader 192.168.2.161
./bin/sitcp-sitcpxg-ip-writer 192.168.2.161 192.168.2.170
```

低レベルのIPレジスタ処理は共有していますが、MPC/MPCXペイロードの読み取りや書き換えは行いません。

## 詳細コマンドの使い方

```bash
./bin/mpc-mpcx-ip-command --help
```

診断や低レベル操作については[詳細コマンド一覧](#詳細コマンド一覧)を参照してください。

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

両方のIPオプションを指定すると、EEPROMのIPを先に書き込み、ランタイムIPを最後に変更します。元のアドレスを必要とする処理が終わるまで通信を維持するためです。ランタイムIP変更後は新しいIPへ再接続し、読み戻して検証します。応答を受信する前にIPが変更された可能性があるため、タイムアウトした破壊的な現在IPの書き込みを無条件に再試行しません。

writerは変更前後のランタイム/EEPROMのMAC・IPを表示し、すべての操作と検証が完了すると成功を表示します。全レジスタのダンプはreaderで確認してください。MPC/MPCXの種類は拡張子ではなく22バイトの内容から判定します。

### EEPROM消去の詳細

EEPROMの`0xFFFFFC00..0xFFFFFC7F`（128バイト）を`FF`で消去し、書き込み保護を戻して全バイトを検証します。前後のMAC/IPと成功結果を表示します。

MPC/MPCXファイルの書き込み、RAMからの初期化、ランタイム/IPレジスタの変更は行いません。`--clear`をファイル、`--set-eeprom-ip`、`--set-current-ip`と組み合わせないでください。`--port`と`--timeout`は指定できます。消去は既定で無効です。通常の起動モードに戻す前に適切なライセンスを再書き込みしてください。

### EEPROM消去後のMPCX書き込み

SiTCP-XGでは、`mpc-mpcx-ip-writer`がEEPROMの`0xFFFFFC10` bit7を確認します。このビットが1の場合（消去後の`FF`を含む）、ランタイムの`0xFFFFFF00..0xFFFFFF4F`全体を読み取り、80バイトのEEPROMイメージを作成します。

| EEPROM範囲 | 初期化が必要な場合のコピー元 |
| --- | --- |
| `FC00..FC0F` | MPCXファイルの先頭16バイト |
| `FC10..FC11` | ランタイムの`FF10..FF11` |
| `FC12..FC17` | MPCXファイルの末尾6バイト（MAC） |
| `FC18..FC4F` | 送信レートを含むランタイムの`FF18..FF4F` |

80バイトすべてを書き込み、検証します。bit7が0の場合は既存の24バイトMPCX書き込み処理を使い、`FC40..FC4F`を含むEEPROM設定を維持します。送信レートの自動修復ではなく、ランタイム値を読み取ったままコピーします。`--set-eeprom-ip`、`--set-current-ip`を指定した場合は、ファイル書き込み後にその順で適用します。EEPROM IPの上書きを指定しない場合、初期化では現在のランタイムIPを保存します。ForceDefault時のアドレスが保存される場合もあります。

書き込まずに計画を確認できます。

```bash
./bin/mpc-mpcx-ip-command mpcx-plan DEVICE_IP FILE.mpcx
```

確認後は通常のCLIで書き込みます。

```bash
./bin/mpc-mpcx-ip-writer DEVICE_IP FILE.mpcx
```

診断表示の`??`は書き込みデータに使用できません。必要なランタイムイメージ全体を読み取れない場合や、識別子/リセットビットが整合しない場合は、EEPROMの書き込み保護を解除する前に停止します。不完全なイメージや推測した既定値を書き込まず、自動消去も行いません。

公式ガイドにはRAMからの初期化と既定値へのフォールバックが記載されています。バージョン`0.4.1-2-gc782`の解析では通常SiTCPの固定既定値へのフォールバックが確認されていますが、MPCXのフォールバックイメージは確立されていません。そのため、この実装では通常SiTCPの既定値をSiTCP-XGに流用しません。通常のMPC書き込みは変更していません。`FC4F`より後の公式オプション拡張コピーは未実装で、`FC50..FC7F`は変更しません。

### readerの表示詳細

readerは常に次の情報を表示します。

```text
current MAC
current IP
EEPROM MAC
EEPROM IP
```

RuntimeとEEPROMを分けて表示し、その後にEEPROMから復元したMPC/MPCX情報を表示します。ランタイムの読み取り範囲は装置の世代に従います。

- 通常SiTCP: `0xFFFFFF00..0xFFFFFF3F`（64バイト）
- SiTCP-XG: `0xFFFFFF00..0xFFFFFF4F`（80バイト）
- EEPROM（両世代共通）: `0xFFFFFC00..0xFFFFFC4F`（80バイト）

生データは1行16バイトの16進ダンプで表示します。通常SiTCPのランタイムは4行、XGは5行、EEPROMはどちらも5行です。SiTCP-XGのパラメータは各領域のデータから別々にデコードします。数値は`10000 (0x2710) Mbps`や`4660 (0x1234)`のように10進・16進を併記します。タイムアウトの換算値には単位と生の10進/16進値を残します。

現在、EEPROM、サーバー、IP専用簡易表示のIPアドレスには、`192.168.10.10 (0xC0A80A0A)`のようにネットワークバイト順の16進値を併記します。MACは通常のコロン区切り16進表記です。通常SiTCPのライセンスバイトをXGの送信レートとして解釈しません。

同じ詳細表示を次のコマンドでも確認できます。

```bash
./bin/mpc-mpcx-ip-command read 192.168.2.161
```

通常SiTCPでは、レジスタマニュアルでアクセス禁止とされるランタイム`0xFFFFFF40..0xFFFFFF4F`を読み取りません。EEPROMの`FC40..FC4F`は読み取り可能で、MPCペイロードの復元に必要です。XGではランタイム`FF40..FF4F`も読み取ります。世代判定がタイムアウトした場合、読み取り範囲を選ぶ前に失敗として終了します。

ブロックのバスエラー時は、そのブロックを1バイトずつ読み直します。読めた値を表示し、拒否されたバイトを`??`で示して、該当アドレスを警告します。バイトが欠けたフィールドは`unavailable`とし、仮のゼロでデコードしません。ランタイムとEEPROMを分け、読み取れたEEPROM情報は表示します。

読み取りコマンドの終了コードは、完全な表示が`0`、部分的な表示が`3`、タイムアウトや短い応答などの致命的エラーが`1`です。writerは簡潔なMAC/IP表示を使い、書き込み後の読み戻し検証を必ず行います。

装置の世代は、公開仕様のSiTCP-XG Identifierレジスタ`0xFFFFFF08..0xFFFFFF0B`から先に判定します。値が正確に`0x58544350`の場合にSiTCP-XGと判定します。MPC/MPCXペイロードの分類とは独立しており、ペイロードから装置の世代を判定しません。

### 詳細コマンド一覧

主なサブコマンドは次のとおりです。`MPC_OR_MPCX_FILE`は`.mpc`または`.mpcx`のライセンス/設定ファイルを意味します。

```text
inspect MPC_OR_MPCX_FILE
mac MPC_OR_MPCX_FILE
read IP [--port N] [--timeout SEC]
verify IP FILE [--port N] [--timeout SEC]
mpcx-plan IP FILE [--port N] [--timeout SEC]
probe IP ADDRESS [LENGTH] [--port N] [--timeout SEC]
rbcp-read IP ADDRESS LENGTH [--port N] [--timeout SEC]
rbcp-write IP ADDRESS HEX-BYTES [--port N] [--timeout SEC]
clear IP --yes-really-clear [--port N] [--timeout SEC]
ip-read IP [--port N] [--timeout SEC]
ip-write CURRENT_IP NEW_IP [--eeprom|--current] [--port N] [--timeout SEC]
```

`read`と`ip-read`は同じランタイム/EEPROMの詳細表示を行います。`ip-write`とIP専用writerは、変更前後のMAC/IPと結果を表示します。IP専用readerは簡潔なMAC/IP表示を維持します。`ip-write`の既定の書き込み先はEEPROMです。現在のランタイムIPを変更する場合は`--current`を指定します。

### ビルド要件

- C++11対応コンパイラ（`g++`または`clang++`）
- POSIXソケット
- `make`

既定のビルドは`-std=c++11`を使います。対象環境はLinux、macOS、WSLです。

### インストール先と更新

リポジトリのルートでビルド/インストールを行ってください。`PREFIX`の既定値は`$(CURDIR)`、`BINDIR`の既定値は`$(PREFIX)/bin`です。既定のインストールに管理者権限は不要です。

| コマンド | 出力先 | 実行例 |
| --- | --- | --- |
| `make` | `./src/`（ビルド出力） | `./src/mpc-mpcx-ip-reader DEVICE_IP` |
| `make install` | `./bin/`（インストール済みコピー） | `./bin/mpc-mpcx-ip-reader DEVICE_IP` |

**使い方の例はすべて`./bin/`のインストール済みコピーを使います。**
インストールせず開発する場合は`./src/`に置き換えてください。インストール成功後は両方のディレクトリに同じ5コマンドが揃います。`make`だけでは既存のインストール済みコピーは更新されません。`make clean`は`src/`内の生成された実行ファイルだけを削除し、ソースとインストール済みバイナリは残します。

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

開発者向けガイドには、SiTCP-XGマニュアルとMPC Writer XGガイドなど、関連するBee Beans Technologiesの資料への参照があります。
