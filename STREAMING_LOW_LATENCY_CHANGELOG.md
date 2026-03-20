# ESPNOW 音声ストリーミング低遅延化メモ

更新日: 2026-03-18  
対象:
- 送信側: `c:/GitHub/Hapbeat/wireless-sender-firmware`
- 受信側: `c:/GitHub/Hapbeat/hapbeat-wireless-firmware`

## 背景

- 症状: `M5Stack_basic_AudioStream_ModuleAudio` で音声入力から再生まで体感 500ms〜1000ms の遅延。
- 要望:
  - 低遅延化（実用レベルへ）
  - 送信側で起動確認できる最低限の表示
  - 安定性も重視（ただし低遅延優先）

---

## 原因として特定したポイント

1. **無線設定が距離優先 (LR / 低レート)**
   - `WIFI_PROTOCOL_LR` や `1M_L` は到達距離には有利だが、低遅延ストリーミングには不利。

2. **受信バッファが遅延を溜めやすい**
   - リングバッファが大きいと、ジッタ時にバッファが伸びて遅延が増加しやすい。

3. **受信側タスク優先度**
   - UI タスク優先度が I2S 出力より高く、再生のリアルタイム性を阻害する可能性。

4. **送信側キュー滞留**
   - `esp_now_send` の完了待ちが積み上がると、古い音声が遅れて再生される。

5. **表示負荷**
   - 波形描画は重く、低遅延最適化時には不要。

---

## 実施した修正（送信側）

### 1) 低遅延向け無線プロファイル

ファイル: `wireless-sender-firmware/lib/espnow_manager/espnow_manager.cpp`

- `ENABLE_AUDIO_STREAM` 時は LR を使わず、`11b/g/n` を使用。
- 非音声用途は従来の距離重視設定を維持。

### 2) I2S 取り込み遅延の削減

ファイル: `wireless-sender-firmware/lib/audioStreamSender/audioStreamSender.cpp`

- `i2s_read` チャンクを小型化:
  - `kStereoSamples = FRAMES_PER_PACKET * 2`
- I2S DMA を小型化:
  - `dma_buf_count = 4`
  - `dma_buf_len = 64`

### 3) 送信 in-flight 制御（新規）

ファイル:
- `wireless-sender-firmware/lib/audioStreamSender/audioStreamSender.cpp`
- `wireless-sender-firmware/lib/espnow_manager/espnow_manager.cpp`
- `wireless-sender-firmware/platformio.ini`

- `STREAM_MAX_INFLIGHT` を導入（env で `2`）。
- in-flight が閾値以上なら古いパケットを送らずに破棄（遅延固定化）。
- `OnDataSent` コールバックで in-flight カウントをデクリメント。
- 統計カウンタを追加:
  - `txInFlight`
  - `txDroppedBusy`
  - `txSendFail`

### 4) 最小表示モード（低負荷）

ファイル:
- `wireless-sender-firmware/lib/audioStreamSender/audioStreamSender.h`
- `wireless-sender-firmware/lib/audioStreamSender/audioStreamSender.cpp`
- `wireless-sender-firmware/platformio.ini`

- 新フラグ: `STREAM_DISPLAY_MINIMAL`
- 表示内容:
  - 入力レベル（単一バー）
  - `pkt`（送信パケット数）
  - `q`（in-flight）
  - `drop/fail`
- 更新頻度は 4fps 相当（低負荷）。
- 波形描画は無効（遅延優先）。

### 5) 対象 env の更新

ファイル: `wireless-sender-firmware/platformio.ini`

`[env:M5Stack_basic_AudioStream_ModuleAudio]`:
- `ENABLE_DISPLAY` を有効化（最小表示用）
- `FRAMES_PER_PACKET=16`
- `STREAM_MAX_INFLIGHT=2`
- `STREAM_DISPLAY_MINIMAL`

---

## 実施した修正（受信側）

### 1) 低遅延向け無線設定

ファイル: `hapbeat-wireless-firmware/lib/espnow_manager/espnow_manager.cpp`

- `AUDIO_STREAM_MODE` 時は `WIFI_PROTOCOL_LR` を使わず `11b/g/n` を使用。
- `WiFi.setSleep(false)` を追加。
- channel 設定順を修正:
  - channel / peer 設定を行ってから `esp_now_add_peer`。

### 2) バッファ戦略の低遅延化

ファイル: `hapbeat-wireless-firmware/lib/audioStreamReceiver/audioStreamReceiver.cpp`

- リングバッファ小型化:
  - `RING_BUF_FRAMES: 2048 -> 512 -> 256`
- プリバッファ小型化:
  - `PRE_BUFFER_FRAMES: 80 -> 32 -> 24`
- 遅延上限の導入:
  - `TARGET_BUFFER_FRAMES=40`
  - `MAX_BUFFER_FRAMES=96`
  - 超過時は古いフレームを破棄して遅延固定化。
- 欠落時の無音補間量を上限化（過剰補間での遅延増大防止）。
- I2S 出力側を小型化:
  - `dma_buf_count=3`
  - `dma_buf_len=16`
  - `BATCH_SIZE=8`

### 3) タスク優先度の見直し

ファイル: `hapbeat-wireless-firmware/src/sample_tasks/taskStreamESPNOW/task_entry.cpp`

- `I2SStream` を高優先度化: `20 -> 23`
- `TaskUI` を低優先度化: `23 -> 5`
- `StreamStats` をさらに低優先度化: `5 -> 2`

### 4) 遅延可視化の追加

ファイル:
- `hapbeat-wireless-firmware/lib/audioStreamReceiver/audioStreamReceiver.h`
- `hapbeat-wireless-firmware/lib/audioStreamReceiver/audioStreamReceiver.cpp`
- `hapbeat-wireless-firmware/src/sample_tasks/taskStreamESPNOW/task_entry.cpp`

- `getEstimatedDelayMs()` を追加（`ringAvailable / sample_rate` から推定）。
- ログ出力と OLED 表示に `dly:xxms` を追加。

---

## 期待される効果

- 500ms〜1000ms 級の遅延を生む主要因（LR設定・バッファ滞留）を排除。
- バッファ膨張時に古い音声を捨てることで、遅延が積み上がりにくい設計へ変更。
- 送信側表示は維持しつつ、低負荷表示のみ残してパフォーマンス影響を最小化。

---

## 現在のチューニング値（要点）

- 送信:
  - `FRAMES_PER_PACKET=16`
  - `STREAM_MAX_INFLIGHT=2`
  - `STREAM_DISPLAY_MINIMAL`
- 受信:
  - `RING_BUF_FRAMES=256`
  - `PRE_BUFFER_FRAMES=24`
  - `TARGET_BUFFER_FRAMES=40`
  - `MAX_BUFFER_FRAMES=96`
  - `BATCH_SIZE=8`

---

## 次の調整候補

1. `STREAM_MAX_INFLIGHT=1`（さらに低遅延、ただし欠落増加リスク）
2. `FRAMES_PER_PACKET=12`（パケット化待ち短縮、無線負荷は増える）
3. `TARGET_BUFFER_FRAMES` を 32 へ（安定性と引き換えに更なる短遅延）
4. `drop/fail/lost/underrun` を見ながら段階調整

---

## 備考

- IDE の linter は PlatformIO 依存ヘッダ未解決によりノイズが多く、本変更では無視方針。
- 本ドキュメントは、今回の会話内で行った低遅延化作業の記録。
