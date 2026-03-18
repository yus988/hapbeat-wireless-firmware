#include "audioStreamReceiver.h"
#include "driver/i2s.h"

namespace audioStreamReceiver {

// --- Ring buffer (lock-free SPSC, power-of-2 size) ---
static constexpr uint32_t RING_BUF_FRAMES = 2048;
static constexpr uint32_t RING_BUF_MASK = RING_BUF_FRAMES - 1;
static StereoFrame ringBuf[RING_BUF_FRAMES];
static volatile uint32_t writeIdx = 0;
static volatile uint32_t readIdx = 0;

// --- Stream state ---
enum class StreamState : uint8_t { IDLE, BUFFERING, STREAMING };
static volatile StreamState state = StreamState::IDLE;

// Pre-buffer before starting I2S playback (absorbs initial jitter)
static constexpr uint32_t PRE_BUFFER_FRAMES = 80;  // 10ms at 8kHz

// --- Sequence tracking ---
static uint8_t expectedSeqNum = 0;
static bool seqInitialized = false;

// --- Statistics ---
static StreamStats stats = {0, 0, 0, 0};

// --- Upsample ---
static uint32_t upsampleRatio = 1;

static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

// ---- Ring buffer helpers ----

static inline uint32_t ringAvailable() { return writeIdx - readIdx; }

static inline bool ringFull() {
  return ringAvailable() >= RING_BUF_FRAMES;
}

static void ringWrite(const StereoFrame &frame) {
  if (ringFull()) {
    stats.bufferOverruns++;
    readIdx++;  // drop oldest to make room
  }
  ringBuf[writeIdx & RING_BUF_MASK] = frame;
  writeIdx++;
}

static bool ringRead(StereoFrame &frame) {
  if (writeIdx == readIdx) return false;
  frame = ringBuf[readIdx & RING_BUF_MASK];
  readIdx++;
  return true;
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
  i2s_config.dma_buf_count = 4;
  i2s_config.dma_buf_len = 32;  // 32 frames per DMA buf = 4ms at 8kHz
  i2s_config.use_apll = false;
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
      uint32_t lostFrames = gap * numFrames;
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

  if (state == StreamState::IDLE) {
    state = StreamState::BUFFERING;
  }
  if (state == StreamState::BUFFERING &&
      ringAvailable() >= PRE_BUFFER_FRAMES) {
    state = StreamState::STREAMING;
  }
}

void i2sOutputTask(void *args) {
  static constexpr uint32_t BATCH_SIZE = 32;
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

void printStats() {
  StreamStats s = stats;
  const char *stateStr = "IDLE";
  if (state == StreamState::BUFFERING) stateStr = "BUFFERING";
  else if (state == StreamState::STREAMING) stateStr = "STREAMING";
  USBSerial.printf(
      "[StreamRx] state=%s pkts=%u lost=%u overrun=%u underrun=%u buf=%u/%u\n",
      stateStr, s.packetsReceived, s.packetsLost, s.bufferOverruns,
      s.bufferUnderruns, ringAvailable(), RING_BUF_FRAMES);
}

}  // namespace audioStreamReceiver
