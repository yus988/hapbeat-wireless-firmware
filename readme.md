# Hapbeat Wireless Firmware

このリポジトリは、無線版 Hapbeat（Duo/Band 共通）のファームウェアです。本プロジェクトは、ESP32-S3 をベースにしており、ESP-NOW や MQTT などの通信プロトコルを活用した、デバイス間通信やインターネット接続機能を提供します。また、LittleFS を使用したファイルシステム管理や複数のセンサーデバイスへの対応も可能です。

## Quick Start (Duo Wireless & ESP-NOW の場合)

※ 事前に [git](https://git-scm.com/downloads), [VSCode](https://azure.microsoft.com/ja-jp/products/visual-studio-code) をインストールし、VSCode の拡張機能から [PlatformIO](https://platformio.org/install/ide?install=vscode) をインストールしてください。

1. リポジトリのクローン or zip ファイルダウンロード
2. Hapbeat を PC に接続し、**BOOT ボタンを押下したまま電源を ON**にし、認識されているか確認  
   （デバイスマネージャーで不明なデバイスと表示されなければ OK。ドライバインストールは不要）
3. VSCode の下部（画像赤枠）env:DuoWL_V3-ESPNOW を選択しビルド&書き込み
   ![alt text](assets/desc_env.png)
4. PlatformIO -> upload file image で data フォルダ内にある wav データを Hapbeat にアップロード  
   ![alt text](desc_fileupload.png)
5. 電源を OFF->BOOT ボタンを押下しないで電源を ON

- UI が表示されているか確認
- （M5Stack basic の場合）ボタン押下で振動するか確認

[詳細はこちらをご覧ください](https://yus988.notion.site/ESP-NOW-162063f7898080878a9be85b6082d63e?pvs=74)

## 必要な環境

**対応ハードウェア**:

- [Hapbeat Duo Wireless (DuoWL)](https://hapbeat.com/products/duo-wireless/)
- [Hapbeat Band Wireless (BandWL)](https://hapbeat.com/products/band-wireless/)

**ソフトウェア**:

- Windows OS（Mac でも動作するとは思いますがサポート対象外です）
- [PlatformIO](https://platformio.org/) IDE

# 各種パラメータ調整

## パラメータ・UI の変更範囲（各 task ディレクトリ内で完結）

調整すべきファイルはすべて `src/sample_tasks/<task>/` または `src/private_tasks/<task>/` ディレクトリ配下に限定されます。共通部（`main.cpp` / `globals.h` / `lib/` 配下）を編集する必要はありません。

- UI 表示・色・文言・レイアウトを変える: `src/public_tasks/<task>/adjustParams.hpp`

  - 例: テキスト配列（`CATEGORY_ID_TXT` / `CHANNEL_ID_TXT` / `GAIN_STEP_TXT`）、表示座標（`*_TEXT_POS`）、LED の色（`COLOR_*`）、ディスプレイ設定（`DISP_ROT` / `FONT_SIZE`）など

  調整項目一覧（例: `taskBandGenESPNOW` の既定値。タスクにより異なる場合があります）

  | 変数名               | 内容説明                                  | デフォルト例                         |
  | -------------------- | ----------------------------------------- | ------------------------------------ |
  | `CATEGORY_ID_TXT`    | カテゴリ名の配列                          | `{ "Cat_0", "Cat_1", "Cat_2" }`      |
  | `CHANNEL_ID_TXT`     | チャンネル/装着者識別名の配列             | `{ "CH_0", "CH_1", "CH_2", "CH_3" }` |
  | `FIX_GAIN_STEP`      | 固定モード時のゲイン（カテゴリ別、0..23） | `{ 16, 0, 23 }`                      |
  | `GAIN_STEP_TXT`      | ゲイン段階表示の文字列（0..23）           | `{ "v 0", "v 1", …, "v23" }`         |
  | `CATEGORY_TEXT_POS`  | カテゴリ表示位置 [x,y]                    | `{ 0, 0 }`                           |
  | `CHANNEL_TEXT_POS`   | チャンネル表示位置 [x,y]                  | `{ 40, 0 }`                          |
  | `GAIN_STEP_TEXT_POS` | ゲイン表示位置 [x,y]                      | `{ 84, 0 }`                          |
  | `BATTERY_TEXT_POS`   | バッテリー表示位置 [x,y]                  | `{ 0, 16 }`                          |
  | `DISP_ROT`           | 画面回転（0 or 90）                       | `90`                                 |
  | `FONT_SIZE`          | フォントサイズ（Adafruit_GFX）            | `1`                                  |
  | `COLOR_FIX_MODE`     | 固定モード時の LED 色                     | `CRGB(5, 5, 5)`                      |
  | `COLOR_VOL_MODE`     | 可変モード時の LED 色                     | `CRGB(0, 0, 5)`                      |
  | `COLOR_DANGER_MODE`  | 警告時の LED 色（使用タスクのみ）         | 例: `CRGB(10, 0, 0)`                 |
  | `ADJ_VOL_STEP`       | 可変モードのボリューム調整ステップ        | `4`                                  |
  | `DEVICE_POS`         | 装着位置番号（アプリ運用値）              | `5`（例: Band 左腕）                 |

  注意:

  - `CATEGORY_ID_TXT` の要素数を変更した場合、同じタスク配下の `audioManagerSettings.hpp` にある `CATEGORY_NUM` も同じ値に更新してください（未更新だとメモリ確保や配列境界の前提がズレます）。

- 音声処理の上限・動作パラメータを変える: `src/public_tasks/<task>/audioManagerSettings.hpp`
  - 例: `CATEGORY_NUM` / `SOUND_FILE_NUM` / `DATA_NUM` / `SUB_DATA_NUM` / `VOLUME_MAX` / `STUB_NUM` / `POSITION_NUM` / `IS_EVENT_MODE`
  - 個別タスクにファイルが無い場合でも、共通デフォルト（`lib/audioManager/audioManagerSettings_default.hpp`）でビルド可能です
  - 注意: `CATEGORY_ID_TXT` の要素数を変更した場合は、ここにある `CATEGORY_NUM` を必ず同じ値に更新してください。

## 新しい task（UI 操作・GUI 関連）の追加方法

### スクリプトによる半自動追加（推奨）

1. 実行（ルートディレクトリ）

```powershell
pwsh -NoProfile -File new_private_task.ps1
```

2. 対話手順

- 表示された `src/sample_tasks/` の一覧からコピー元を選択
- 「新しい private タスク名の末尾のみ」を入力（先頭の `task` は自動付与。例: `NeckNewESPNOW` → `taskNeckNewESPNOW`）
- 環境名は自動生成（例: `DuoWL_V3_NECKNEWESPNOW`/`BandWL_V3_...`）。必要ならスクリプト実行後に `src/private_tasks/platformio.private.ini` を手動調整

3. 生成内容（自動）

- `src/private_tasks/<新タスク名>/` へ丸ごとコピー
- `src/private_tasks/platformio.private.ini` に [env:...] を追記（コピー元の build*flags を継承、`-I` を private に置換と `-D TASK*<新タスク>` を補完）
- `src/private_tasks/adjustParams.hpp` に TASK\_\* 分岐を 1 行追加

4. 反映

- PlatformIO の環境一覧は即時には更新されません。VSCode の「Reload Window」または「PlatformIO: Rebuild IntelliSense Index」を実行してください。

---

### 手動で追加する方法

共通部（main.cpp / globals.h）は編集不要です。以下の手順のみで追加できます。

1. ディレクトリ作成

- `src/private_tasks/task<Device><Gen|Feature><PROTOCOL>/` を作成（例: `taskNeckNewESPNOW/`）
- 中に最低限、以下のファイルを用意:
  - `task_entry.cpp`（必須: Init/Start/Loop の共通エントリ）
  - `<任意の名前>.cpp` に UI 本体（最後に `TaskUI_Run(void*)` を実装）
  - `audioManagerSettings.hpp`（音声設定マクロ）
  - `adjustParams.hpp`（UI 実体定義）

2. task_entry.cpp を実装

- 必須 3 関数: `TaskAppInit()`, `TaskAppStart()`, `TaskAppLoop()`
- `TaskAppStart()` では共通の UI エントリ `TaskUI_Run` を起動します

```cpp
#include "globals.h"
#include "task_entry.h"
#include <espnow_manager.h>  // ESPNOW の場合。MQTT の場合は <MQTT_manager.h>

void TaskAppInit() {
  // 例: ESPNOW の初期化
  displayManager::setTitle(CATEGORY_ID_TXT, CATEGORY_ID_TXT_SIZE,
                           CHANNEL_ID_TXT, CHANNEL_ID_TXT_SIZE,
                           GAIN_STEP_TXT, GAIN_STEP_TXT_SIZE);
  setFixGain(true);
  _display.display();
  espnowManager::init_esp_now(audioManager::PlaySndOnDataRecv);
}

void TaskAppStart() {
  xTaskCreatePinnedToCore(TaskUI_Run, "TaskUI", 4096, NULL, 23, &thp[1], 1);
}

void TaskAppLoop() {
  // MQTT の場合はここで MQTT_manager::loopMQTTclient(); を回す
  // ESPNOW の場合は空のままでOK
}
```

3. UI 本体に共通エントリ `TaskUI_Run` を用意

- 既存のループ関数がある場合はラップするだけで OK

```cpp
// 任意ファイル（例: MyTask.cpp）
// 既存の UI 関数
void MyTaskMain(void *args) { /* ... */ }

// 共通エントリ
void TaskUI_Run(void *args) { MyTaskMain(args); }
```

4. src/private_tasks/platformio.private.ini へ env を追加（private_tasks のタスクをビルド対象にする）

- 対象タスクだけがビルドされるよう `build_src_filter` を指定
- `adjustParams.cpp` の切替に使う `-D TASK_*` マクロも合わせて定義

```ini
[env:NeckWL_V3_NEW_ESPNOW]
build_flags =
    -D NECKLACE_V3
    -D ESPNOW
    -D TASK_NECK_NEW_ESPNOW
lib_ignore = MQTT_manager
build_src_filter =
    +<*> -<sample_tasks/*> -<private_tasks/*> +<private_tasks/taskNeckNewESPNOW/>

; audioManagerSettings.hpp を解決するためのヘッダ検索パス（タスク配下を追加）
build_flags =
    ${env.build_flags}
    -I src/private_tasks/taskNeckNewESPNOW
```

## 画面 UI の変更（テンプレート）

新規構成の雛形は [`src/adjustParams.cpp.tpl`](src/adjustParams.cpp.tpl) にあります。内容を参考に、各タスクの `adjustParams.hpp` で必要な実体（配列/色/座標など）を調整してください。

## 複数台への一括書き込み（upload_all.py）

複数の ESP32 を自動検出して、各デバイスに対し「フラッシュ消去 → ファームウェア書き込み → ファイルシステム書き込み（LittleFS/SPIFFS）」を順番に実行します。

- 前提

  - PlatformIO CLI（`pio`）がインストールされ、PATH に通っていること
  - Python 3 が使えること（`pyserial` が必要。エラー時は `pip install pyserial`）
  - Windows では PowerShell ではなく、cmd.exe または Git Bash での実行を推奨

- 環境の指定（どちらか）

  1. 実行時に環境変数 `PIOENV` を指定する

  ```bat
  rem 例: BandWL_V3_GEN_ESPNOW を指定して実行（cmd.exe）
  set PIOENV=BandWL_V3_GEN_ESPNOW
  python upload_all.py
  ```

  2. もしくは `upload_all.py` 内の `TARGET_ENV` を編集して固定する

- 実行コマンド
  （PowerShell 等のターミナルではなく PlatformIO CLI から実行する必要があります）

```bat
python upload_all.py
```

- 動作概要

  - ポート自動検出: デバイス記述に「USB / CP210 / CH910」を含むシリアルポートを対象
  - 各デバイスごとに以下を実行
    - フラッシュ消去: `pio run -e <ENV> -t erase --upload-port <PORT>`
    - ファーム書き込み: `pio run -e <ENV> -t upload --upload-port <PORT>`
    - FS 書き込み: `pio run -e <ENV> -t uploadfs --upload-port <PORT>`

- トラブルシューティング
  - デバイスが見つからない: ドライバ（CP210/CH910）導入、Device Manager で COM ポート確認、BOOT モード確認
  - `serial` モジュールが無い: `pip install pyserial`
  - `pio` が見つからない: PlatformIO CLI をインストールし PATH を設定

## 振動用音声データの差し替え

[`data/`]フォルダ内の wav データを差し替えてください。ファイル名規則は次項を確認してください。

- 差し替えたら Quick Start にあるよう、platformIO -> upload file image で逐一アップロードし直してください。
- 拡張子は wav のみの対応となります。
- wav 形式は 符号付き 16bit（必須） / サンプルレート 16 kHz がおすすめです。
  - 8 kHz だと明確に遅延が大きくなるので注意。恐らく esp 側で最適化されていない。
- 極力ファイルサイズを小さくするようにしてください。
  - 可能であればモノラル・1 ファイルにつき数百ミリ秒が目安です。

### ファイル名規則

ファイル名はハイフン区切りで以下の様な構成になっています。  
**category - soundID_subID - audioChannel - saveDistination - name**

- category: 再生するカテゴリを指定
- soundID: 音声データの種類
  - subID: 同じ soundID の差分データ。歩行音などの繰り返し再生する音声データにランダム性を付与したい場合に活用推奨
- audioChannel: C / L / R から指定。基本 C で、DuoWL で左右の振動強度を別々にしたい場合のみ L/R を指定。
  - C の場合はモノラル音源、L / R の場合は片方のチャンネルをミュートにしたステレオ音源を用意すること。
- saveDistination: RAM / FS からデータの保存先を指定。
  - RAM: 内部 RAM に保存。応答性を重視するファイルは RAM を選択すること。ただしサイズ制限が厳しいため１ファイル 100KB 以下、RAM 全体で 500KB 以内が目安。
  - FS: ファイルストレージに保存。RAM に比べサイズに余裕があるが、呼び出しに時間が掛かる（数百ミリ秒程度）ため、ループ音源などリアルタイ性が求められないもの推奨。FS 全体で 2MB 以内が目安。
- name: 人間が読む用の名前。プログラム上では無視される。

例：0-0_1-C-RAM-gunshot.wav というファイルに対して、読み込まれるパラメータ

- category = 0
- sound_id = 0
- sub_id = 1
- audio_channel = C
- data_storage = RAM

### 最大データ数・音声処理の設定

各タスクの [`audioManagerSettings.hpp`](src/public_tasks) を編集してください（タスクごとに上書き可、未定義時は `lib/audioManager/audioManagerSettings_default.hpp` が使用されます）。

## 送受信データ形式説明

コンマ区切りのテキストデータ（例 "0,0,99,0,0,100,100,0"）を受信することで、以下の内容にパースしデータの可否や再生する wav データ、音量の選択を行います。

### **{category, channel, position, sound_id, sub_id, volume_L, volume_R, playtype}**

- category: **0--設定最大値**の整数値。 受信カテゴリ（ゲーム、動画など別々のコンテンツを 1 台で担いたい場合に分別）
- channel: **0--255**の整数値。受信チャンネル（player A / B / C など同一コンテンツで別々のチャンネルを設けたいときに分別）
- position: **0--255**の整数値。受信する装着位置（DuoWL:首＋ BandWL:手首など、1 人が複数の Hapbeat を装着する場合に分別）
  - それぞれ `99` を指定することで受信側の設定に関わらず常に再生させることができます。
    - 例：category = 99 とすることで、受信側の category 設定に関わらず再生
  - 上記 3 つは機能的には同等です。
- sound_id: **0--設定最大値**の整数値。音声ファイルの種類、カテゴリごとに独立
- sub_id: **0--設定最大値** 同じ soundID の差分データ。歩行音などの繰り返し再生する音声データにランダム性を付与したい場合に活用推奨。
- volume_L, R: **0--255** の整数値。左右の再生音量を指定。L=R の場合、モノラル、L≠R の場合ステレオと認識。ステレオの場合、左右それぞれの音声ファイルが必要であることに注意。
- playtype: **0--3** の整数値。各トラックで同時に再生できる音声は 1 種類のみ。
  - 0 = oneshot: 対象を 1 回のみ再生（トラック０）
  - 1 = loopStart: 対象をループ再生（トラック１）
  - 2 = loopStop: ループ再生を停止（トラック１）
  - 3 = oneshot(2ndline): 対象を 1 回のみ再生（トラック２）
    - oneshot を同時に複数出したい場合にご利用ください。

## 装着位置早見表

実際は開発者側が自由に選べますが、Hapbeat 社では以下を基準としています。
![alt text](assets/device_position_num.png)

---

---

### 使用ライブラリ

このプロジェクトは以下のライブラリを使用しています。各ライブラリはそれぞれのライセンスに基づいて提供されています。

| ライブラリ名                         | ライセンス         | リンク                                                                   |
| ------------------------------------ | ------------------ | ------------------------------------------------------------------------ |
| **Adafruit SSD1306**                 | BSD ライセンス     | [GitHub](https://github.com/adafruit/Adafruit_SSD1306)                   |
| **efont Unicode Font Data**          | Apache License 2.0 | [GitHub](https://github.com/tanakamasayuki/efont-unicode-font-data)      |
| **ESP8266Audio**                     | LGPL v2.1          | [GitHub](https://github.com/earlephilhower/ESP8266Audio)                 |
| **FastLED**                          | MIT ライセンス     | [GitHub](https://github.com/FastLED/FastLED)                             |
| **MQTT (256dpi)**                    | MIT ライセンス     | [GitHub](https://github.com/256dpi/arduino-mqtt)                         |
| **Soldered MCP4018 Arduino Library** | MIT ライセンス     | [GitHub](https://github.com/SolderedElectronics/MCP4018-Arduino-Library) |

### ライセンス情報

このプロジェクトで使用されるライブラリのライセンス情報は `LICENSES` フォルダに含まれています。詳細は各ライセンスファイルを参照してください。
