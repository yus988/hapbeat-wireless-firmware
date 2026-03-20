#include "audioStreamReceiver.h"
#include "driver/i2s.h"
#include <atomic>

namespace audioStreamReceiver {

// --- Ring buffer (lock-free SPSC, power-of-2 size) ---
// 低遅延優先のため、保持量を必要最小限に抑える。
static constexpr uint32_t RING_BUF_FRAMES = 256;
static constexpr uint32_t RING_BUF_MASK = RING_BUF_FRAMES - 1;
static StereoFrame ringBuf[RING_BUF_FRAMES];
// SPSC 契約: writeIdx はライター(コールバック)のみ変更、readIdx はリーダー(I2Sタスク)のみ変更
static std::atomic<uint32_t> writeIdx{0};
static std::atomic<uint32_t> readIdx{0};

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
static void ringWrite(const StereoFrame &frame) {
  if (ringFull()) {
    stats.bufferOverruns++;
    return;  // 新フレームを破棄（readIdx は操作しない）
  }
  uint32_t w = writeIdx.load(std::memory_order_relaxed);
  ringBuf[w & RING_BUF_MASK] = frame;
  writeIdx.store(w + 1, std::memory_order_release);
}

// リーダー専用: readIdx のみ変更。
static bool ringRead(StereoFrame &frame) {
  uint32_t r = readIdx.load(std::memory_order_relaxed);
  uint32_t w = writeIdx.load(std::memory_order_acquire);
  if (w == r) return false;
  frame = ringBuf[r & RING_BUF_MASK];
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

  uint32_t expectedLen = 4 + numFrames * sizeof(StereoFrame);
  if ((uint32_t)data_len < expectedLen) return;

  stats.packetsReceived++;

  // Packet loss detection: fill zeros for missing packets
  if (seqInitialized) {
    uint8_t gap = seqNum - expectedSeqNum;
    if (gap > 0 && gap < 128) {
      stats.packetsLost += gap;
      // 無線が乱れたときに古い無音を大量に積むと遅延が増えるため、補間量を制限。
      uint32_t lostFrames = gap * numFrames;
      if (lostFrames > PRE_BUFFER_FRAMES) lostFrames = PRE_BUFFER_FRAMES;
      StereoFrame silence = {0, 0};
      for (uint32_t i = 0; i < lostFrames && !ringFull(); i++) {
        ringWrite(silence);
      }
    }
  }
  seqInitialized = true;
  expectedSeqNum = seqNum + 1;

  const StereoFrame *frames =
      reinterpret_cast<const StereoFrame *>(data + 4);
  for (uint16_t i = 0; i < numFrames; i++) {
    ringWrite(frames[i]);
  }

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

  while (true) {
    // バッファが MAX を超えていたら古いフレームをスキップして遅延固定化
    ringDrainExcess();

    uint32_t outIdx = 0;

    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
      StereoFrame f;
      if (!ringRead(f)) {
        f = silence;
        stats.bufferUnderruns++;
      }
      for (uint32_t r = 0; r < upsampleRatio; r++) {
        outBuf[outIdx++] = f;
      }
    }

    i2s_write(I2S_PORT, outBuf, outIdx * sizeof(StereoFrame),
              &bytesWritten, portMAX_DELAY);
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
      "[StreamRx] state=%s pkts=%u lost=%u overrun=%u underrun=%u buf=%u/%u "
      "est=%ums\n",
      stateStr, s.packetsReceived, s.packetsLost, s.bufferOverruns,
      s.bufferUnderruns, ringAvailable(), RING_BUF_FRAMES, delayMs);
}

}  // namespace audioStreamReceiver
