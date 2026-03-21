#include "audioStreamReceiver.h"
#include "driver/i2s.h"
#include <atomic>

#ifdef STREAM_ADPCM
#include "ima_adpcm.h"
#endif

// ---- Latency measurement GPIO ----
// LATENCY_MEASURE_PIN をビルドフラグで定義するとパケット受信/I2S 出力タイミングを計測可能。
// NECKLACE_V3: GPIO 12 が空き
#ifdef LATENCY_MEASURE_PIN
static bool latencyGpioInited = false;
static inline void latencyGpioInit() {
  if (!latencyGpioInited) {
    pinMode(LATENCY_MEASURE_PIN, OUTPUT);
    digitalWrite(LATENCY_MEASURE_PIN, LOW);
    latencyGpioInited = true;
  }
}
static inline void latencyGpioToggle() {
  static bool state = false;
  state = !state;
  digitalWrite(LATENCY_MEASURE_PIN, state ? HIGH : LOW);
}
#else
static inline void latencyGpioInit() {}
static inline void latencyGpioToggle() {}
#endif

namespace audioStreamReceiver {

// --- Ring buffer (lock-free SPSC, power-of-2 size) ---
// 低遅延優先のため、保持量を必要最小限に抑える。
static constexpr uint32_t RING_BUF_FRAMES = 256;
static constexpr uint32_t RING_BUF_MASK = RING_BUF_FRAMES - 1;

struct TimestampedFrame {
  StereoFrame frame;
  uint32_t recvTimestamp;  // micros() at packet reception
};
static TimestampedFrame ringBuf[RING_BUF_FRAMES];

// SPSC 契約: writeIdx はライター(コールバック)のみ変更、readIdx はリーダー(I2Sタスク)のみ変更
static std::atomic<uint32_t> writeIdx{0};
static std::atomic<uint32_t> readIdx{0};

// --- Serial latency measurement ---
static uint32_t latencySum = 0;
static uint32_t latencyCount = 0;
static uint32_t latencyMin = UINT32_MAX;
static uint32_t latencyMax = 0;
static constexpr uint32_t LATENCY_REPORT_INTERVAL = 500;  // report every N batches

// --- Stream state ---
enum class StreamState : uint8_t { IDLE, BUFFERING, STREAMING };
static volatile StreamState state = StreamState::IDLE;

// Pre-buffer before starting I2S playback (absorbs initial jitter)
static constexpr uint32_t PRE_BUFFER_FRAMES = 16;   // 2ms at 8kHz
static constexpr uint32_t TARGET_BUFFER_FRAMES = 24; // 3ms at 8kHz
static constexpr uint32_t MAX_BUFFER_FRAMES = 48;   // 6ms at 8kHz

// --- Sequence tracking ---
static uint8_t expectedSeqNum = 0;
static bool seqInitialized = false;

// --- Statistics ---
static StreamStats stats = {0, 0, 0, 0};
static uint32_t piggybackRecovered = 0;  // piggyback で復元成功した回数

// --- Upsample ---
static uint32_t upsampleRatio = 1;

static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

// ---- Ring buffer helpers ----

static inline uint32_t ringAvailable() {
  return writeIdx.load(std::memory_order_acquire) -
         readIdx.load(std::memory_order_acquire);
}

static inline bool ringFull() {
  return ringAvailable() >= RING_BUF_FRAMES;
}

// ライター専用: writeIdx のみ変更。満杯時は新フレームを破棄。
static void ringWrite(const StereoFrame &frame, uint32_t timestamp) {
  if (ringFull()) {
    stats.bufferOverruns++;
    return;  // 新フレームを破棄（readIdx は操作しない）
  }
  uint32_t w = writeIdx.load(std::memory_order_relaxed);
  ringBuf[w & RING_BUF_MASK] = {frame, timestamp};
  writeIdx.store(w + 1, std::memory_order_release);
}

// リーダー専用: readIdx のみ変更。timestamp にパケット受信時刻を返す。
static bool ringRead(StereoFrame &frame, uint32_t &timestamp) {
  uint32_t r = readIdx.load(std::memory_order_relaxed);
  uint32_t w = writeIdx.load(std::memory_order_acquire);
  if (w == r) return false;
  const TimestampedFrame &tf = ringBuf[r & RING_BUF_MASK];
  frame = tf.frame;
  timestamp = tf.recvTimestamp;
  readIdx.store(r + 1, std::memory_order_release);
  return true;
}

// リーダー専用: バッファ超過分をスキップして遅延を固定化。
static void ringDrainExcess() {
  uint32_t r = readIdx.load(std::memory_order_relaxed);
  uint32_t w = writeIdx.load(std::memory_order_acquire);
  uint32_t available = w - r;
  if (available > MAX_BUFFER_FRAMES) {
    uint32_t drop = available - TARGET_BUFFER_FRAMES;
    readIdx.store(r + drop, std::memory_order_release);
    stats.bufferOverruns += drop;
  }
}

// ---- Public API ----

void initI2SStream(int bclkPin, int lrckPin, int doutPin) {
  latencyGpioInit();
  upsampleRatio = I2S_OUTPUT_RATE / STREAM_SAMPLE_RATE;
  if (upsampleRatio < 1) upsampleRatio = 1;

  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = (uint32_t)I2S_OUTPUT_RATE;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = 2;   // ダブルバッファ最小構成で低遅延化
  i2s_config.dma_buf_len = 16;  // 16 frames per DMA buf = 2ms at 8kHz
  i2s_config.use_apll = true;  // APLL で低ジッタクロック生成
  i2s_config.tx_desc_auto_clear = true;

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num = bclkPin;
  pin_config.ws_io_num = lrckPin;
  pin_config.data_out_num = doutPin;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);

  USBSerial.printf("[StreamRx] I2S init: %d Hz stereo, upsample=%dx\n",
                   I2S_OUTPUT_RATE, upsampleRatio);
}

void onStreamDataRecv(const uint8_t *mac_addr, const uint8_t *data,
                      int data_len) {
  (void)mac_addr;
  if (data_len < 4) return;
  if (data[0] != STREAM_PKT_TYPE) return;

  uint8_t seqNum = data[1];
  uint16_t numFrames = (uint16_t)data[2] | ((uint16_t)data[3] << 8);

#ifdef STREAM_ADPCM
  uint32_t currentLen = ima_adpcm::ADPCM_HEADER_SIZE + numFrames;
#else
  uint32_t currentLen = 4 + numFrames * sizeof(StereoFrame);
#endif
  if ((uint32_t)data_len < currentLen) return;

  stats.packetsReceived++;

  uint32_t recvTs = micros();

#ifdef STREAM_ADPCM
  // --- Piggyback recovery check ---
  // パケットが currentLen より大きければ piggyback セクションが存在
  uint32_t piggybackOffset = currentLen;
  uint32_t piggybackTotalLen = ima_adpcm::PIGGYBACK_HEADER_SIZE + numFrames;
  bool hasPiggyback = ((uint32_t)data_len >= piggybackOffset + piggybackTotalLen);

  if (seqInitialized) {
    uint8_t gap = seqNum - expectedSeqNum;
    if (gap > 0 && gap < 128) {
      stats.packetsLost += gap;

      // gap==1 かつ piggyback あり → 前パケットを復元
      if (gap == 1 && hasPiggyback) {
        uint8_t prevSeq;
        ima_adpcm::State pbL, pbR;
        ima_adpcm::readPiggybackHeader(&data[piggybackOffset], prevSeq, pbL, pbR);

        // prevSeq が期待値と一致する場合のみ復元
        if (prevSeq == expectedSeqNum) {
          static StereoFrame pbDecodedBuf[240];
          uint16_t pbN = numFrames > 240 ? 240 : numFrames;
          const uint8_t *pbAdpcm = &data[piggybackOffset + ima_adpcm::PIGGYBACK_HEADER_SIZE];
          ima_adpcm::decodeStereo(pbL, pbR, pbAdpcm, pbN,
                                  reinterpret_cast<int16_t *>(pbDecodedBuf));
          for (uint16_t i = 0; i < pbN; i++) {
            ringWrite(pbDecodedBuf[i], recvTs);
          }
          piggybackRecovered++;
          // ロス1パケット分を復元したのでギャップ消費済み（無音挿入不要）
        } else {
          // prevSeq 不一致 → 復元不可、無音で埋める
          StereoFrame silence = {0, 0};
          uint32_t lostFrames = numFrames;
          if (lostFrames > PRE_BUFFER_FRAMES) lostFrames = PRE_BUFFER_FRAMES;
          for (uint32_t i = 0; i < lostFrames && !ringFull(); i++) {
            ringWrite(silence, 0);
          }
        }
      } else {
        // gap>=2 or no piggyback → 無音で埋める（piggyback は直前1パケのみ復元可能）
        uint32_t lostFrames = gap * numFrames;
        if (lostFrames > PRE_BUFFER_FRAMES) lostFrames = PRE_BUFFER_FRAMES;
        StereoFrame silence = {0, 0};
        for (uint32_t i = 0; i < lostFrames && !ringFull(); i++) {
          ringWrite(silence, 0);
        }
      }
    }
  }
  seqInitialized = true;
  expectedSeqNum = seqNum + 1;

  // 現在パケットの ADPCM デコード
  ima_adpcm::State stL, stR;
  ima_adpcm::readStateFromPacket(data, stL, stR);
  static StereoFrame decodedBuf[240];
  uint16_t n = numFrames > 240 ? 240 : numFrames;
  ima_adpcm::decodeStereo(stL, stR,
                          &data[ima_adpcm::ADPCM_HEADER_SIZE], n,
                          reinterpret_cast<int16_t *>(decodedBuf));
  for (uint16_t i = 0; i < n; i++) {
    ringWrite(decodedBuf[i], recvTs);
  }
#else
  // Non-ADPCM path (no piggyback support)
  if (seqInitialized) {
    uint8_t gap = seqNum - expectedSeqNum;
    if (gap > 0 && gap < 128) {
      stats.packetsLost += gap;
      uint32_t lostFrames = gap * numFrames;
      if (lostFrames > PRE_BUFFER_FRAMES) lostFrames = PRE_BUFFER_FRAMES;
      StereoFrame silence = {0, 0};
      for (uint32_t i = 0; i < lostFrames && !ringFull(); i++) {
        ringWrite(silence, 0);
      }
    }
  }
  seqInitialized = true;
  expectedSeqNum = seqNum + 1;

  const StereoFrame *frames =
      reinterpret_cast<const StereoFrame *>(data + 4);
  for (uint16_t i = 0; i < numFrames; i++) {
    ringWrite(frames[i], recvTs);
  }
#endif

  // バッファ超過制御はリーダー側 (i2sOutputTask) で実施 → SPSC 契約遵守

  if (state == StreamState::IDLE) {
    state = StreamState::BUFFERING;
  }
  if (state == StreamState::BUFFERING &&
      ringAvailable() >= PRE_BUFFER_FRAMES) {
    state = StreamState::STREAMING;
  }
}

void i2sOutputTask(void *args) {
  (void)args;
  static constexpr uint32_t BATCH_SIZE = 8;
  static constexpr uint32_t MAX_UPSAMPLE = 8;
  static StereoFrame outBuf[BATCH_SIZE * MAX_UPSAMPLE];
  const StereoFrame silence = {0, 0};
  size_t bytesWritten;

  USBSerial.println("[StreamRx] I2S output task: waiting for data...");

  while (state != StreamState::STREAMING) {
    vTaskDelay(1);
  }

  USBSerial.println("[StreamRx] Streaming started");

  uint32_t batchCounter = 0;

  while (true) {
    // バッファが MAX を超えていたら古いフレームをスキップして遅延固定化
    ringDrainExcess();

    uint32_t outIdx = 0;
    uint32_t preWriteTs = micros();

    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
      StereoFrame f;
      uint32_t recvTs = 0;
      if (!ringRead(f, recvTs)) {
        f = silence;
        stats.bufferUnderruns++;
      } else if (recvTs != 0) {
        // 受信→I2S出力までの遅延を計測
        uint32_t delta = preWriteTs - recvTs;
        latencySum += delta;
        latencyCount++;
        if (delta < latencyMin) latencyMin = delta;
        if (delta > latencyMax) latencyMax = delta;
      }
      for (uint32_t r = 0; r < upsampleRatio; r++) {
        outBuf[outIdx++] = f;
      }
    }

    latencyGpioToggle();  // I2S 出力直前（オシロスコープ計測用）
    i2s_write(I2S_PORT, outBuf, outIdx * sizeof(StereoFrame),
              &bytesWritten, portMAX_DELAY);

    // 定期的にシリアルへ遅延統計を出力
    if (++batchCounter >= LATENCY_REPORT_INTERVAL && latencyCount > 0) {
      uint32_t avgUs = latencySum / latencyCount;
      USBSerial.printf(
          "[Latency] avg=%uus min=%uus max=%uus (n=%u) buf=%u\n",
          avgUs, latencyMin, latencyMax, latencyCount, ringAvailable());
      latencySum = 0;
      latencyCount = 0;
      latencyMin = UINT32_MAX;
      latencyMax = 0;
      batchCounter = 0;
    }
  }
}

StreamStats getStreamStats() { return stats; }

StreamState getState() { return state; }

uint32_t getBufferLevel() { return ringAvailable(); }

uint32_t getEstimatedDelayMs() {
  return (ringAvailable() * 1000U) / STREAM_SAMPLE_RATE;
}

void printStats() {
  StreamStats s = stats;
  const char *stateStr = "IDLE";
  if (state == StreamState::BUFFERING) stateStr = "BUFFERING";
  else if (state == StreamState::STREAMING) stateStr = "STREAMING";
  uint32_t delayMs = getEstimatedDelayMs();
  USBSerial.printf(
      "[StreamRx] state=%s pkts=%u lost=%u recovered=%u overrun=%u underrun=%u "
      "buf=%u/%u est=%ums\n",
      stateStr, s.packetsReceived, s.packetsLost, piggybackRecovered,
      s.bufferOverruns, s.bufferUnderruns, ringAvailable(), RING_BUF_FRAMES,
      delayMs);
}

LatencyStats getLatencyStats() {
  LatencyStats ls;
  if (latencyCount > 0) {
    ls.avgUs = latencySum / latencyCount;
    ls.minUs = latencyMin;
    ls.maxUs = latencyMax;
    ls.sampleCount = latencyCount;
  } else {
    ls.avgUs = 0;
    ls.minUs = 0;
    ls.maxUs = 0;
    ls.sampleCount = 0;
  }
  return ls;
}

}  // namespace audioStreamReceiver
