#ifndef AUDIO_STREAM_RECEIVER_H
#define AUDIO_STREAM_RECEIVER_H

#include <Arduino.h>
#include <esp_now.h>

#ifndef STREAM_SAMPLE_RATE
#define STREAM_SAMPLE_RATE 8000
#endif

#ifndef I2S_OUTPUT_RATE
#define I2S_OUTPUT_RATE 8000
#endif

#define STREAM_PKT_TYPE 0xAA

namespace audioStreamReceiver {

struct StereoFrame {
  int16_t left;
  int16_t right;
};

struct StreamStats {
  uint32_t packetsReceived;
  uint32_t packetsLost;
  uint32_t bufferOverruns;
  uint32_t bufferUnderruns;
};

void initI2SStream(int bclkPin, int lrckPin, int doutPin);
void onStreamDataRecv(const uint8_t *mac_addr, const uint8_t *data,
                      int data_len);
void i2sOutputTask(void *args);
StreamStats getStreamStats();
uint32_t getBufferLevel();
uint32_t getEstimatedDelayMs();
void printStats();

}  // namespace audioStreamReceiver

#endif
