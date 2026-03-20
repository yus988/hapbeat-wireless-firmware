#include "globals.h"
#include "task_entry.h"

#include <audioStreamReceiver.h>
#include <espnow_manager.h>

#ifdef EN_MCP4018
  #include "MCP4018-SOLDERED.h"
  extern MCP4018_SOLDERED _digipot;
#endif

// ボリュームUI設定（Deaflympic タスクと同一スタイル）
static constexpr int VOL_UI_BAR_X = 10;
static constexpr int VOL_UI_BAR_Y = 10;
static constexpr int VOL_UI_BAR_W = 80;
static constexpr int VOL_UI_BAR_H = 12;
static constexpr int VOL_UI_CHANGE_THRESHOLD = 2;
static constexpr unsigned long VOL_UI_TIMEOUT_MS = 1500UL;

static void showStreamStats() {
  audioStreamReceiver::StreamStats s = audioStreamReceiver::getStreamStats();
  _display.clearDisplay();
  if (s.packetsReceived == 0) {
    displayManager::printEfont(&_display, "Stream: Ready", 0, 0);
    displayManager::printEfont(&_display, "Waiting sender...", 0, 16);
  } else {
    char line[32];
    snprintf(line, sizeof(line), "RX: %lu pkts", s.packetsReceived);
    displayManager::printEfont(&_display, line, 0, 0);
    snprintf(line, sizeof(line), "buf:%lu dly:%lums",
             audioStreamReceiver::getBufferLevel(),
             audioStreamReceiver::getEstimatedDelayMs());
    displayManager::printEfont(&_display, line, 0, 16);
    if (s.packetsLost > 0 || s.bufferUnderruns > 0) {
      snprintf(line, sizeof(line), "L:%lu U:%lu", s.packetsLost, s.bufferUnderruns);
      displayManager::printEfont(&_display, line, 0, 32);
    }
  }
  _display.display();
}

static void showVolumeUI(int step) {
  _display.clearDisplay();
  _display.setFont();
  _display.setTextSize(2);
  _display.setTextColor(SSD1306_WHITE);
  char volTxt[8];
  snprintf(volTxt, sizeof(volTxt), "%d", step);
  _display.setCursor(VOL_UI_BAR_X + VOL_UI_BAR_W + 8, VOL_UI_BAR_Y - 1);
  _display.print(volTxt);
  _display.drawRect(VOL_UI_BAR_X, VOL_UI_BAR_Y, VOL_UI_BAR_W, VOL_UI_BAR_H, SSD1306_WHITE);
  int fillWidth = map(step, 0, VOLUME_MAX, 0, VOL_UI_BAR_W - 2);
  if (fillWidth > 0) {
    _display.fillRect(VOL_UI_BAR_X + 1, VOL_UI_BAR_Y + 1, fillWidth, VOL_UI_BAR_H - 2, SSD1306_WHITE);
  }
  _display.display();
}

static void TaskStreamStats(void *args) {
  while (true) {
    audioStreamReceiver::printStats();
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void TaskAppInit() {
  _display.clearDisplay();
  displayManager::printEfont(&_display, "Stream: Ready", 0, 0);
  displayManager::printEfont(&_display, "Waiting sender...", 0, 16);
  _display.display();

  // ADCからボリューム初期値を読み取ってアンプゲインを設定
#if defined(NECKLACE_V2) || defined(NECKLACE_V3)
  _currAIN = analogRead(AIN_VIBVOL_PIN);
  _ampVolStep = map(_currAIN, 0, 4095, VOLUME_MAX, 0);
  setAmpStepGain(_ampVolStep, false);
  USBSerial.printf("[Stream] Initial volume step: %d/%d\n", _ampVolStep, VOLUME_MAX);
#endif

  audioStreamReceiver::initI2SStream(BCLK_PIN, LRCK_PIN, DOUT_PIN);
  espnowManager::init_esp_now(audioStreamReceiver::onStreamDataRecv);
}

void TaskAppStart() {
  xTaskCreatePinnedToCore(audioStreamReceiver::i2sOutputTask, "I2SStream",
                          4096, NULL, 23, &thp[0], 1);  // Core 1: I2S 出力専用
  xTaskCreatePinnedToCore(TaskUI_Run, "TaskUI", 4096, NULL, 5, &thp[1], 0);  // Core 0: UI は I2S と競合させない
  xTaskCreatePinnedToCore(TaskStreamStats, "StreamStats", 2048, NULL, 2, NULL,
                          0);  // Core 0
}

void TaskAppLoop() {}

void TaskUI_Run(void *args) {
#if defined(NECKLACE_V2) || defined(NECKLACE_V3)
  int lastDisplayedVolStep = _ampVolStep;
  bool volUiShown = false;
  unsigned long volUiLastShown = 0;
  unsigned long lastStatsUpdate = 0;

  while (true) {
    _currAIN = analogRead(AIN_VIBVOL_PIN);
    int newVolStep = map(_currAIN, 0, 4095, VOLUME_MAX, 0);

    // しきい値を超えたらボリュームUI表示モードに入る
    if (!volUiShown) {
      int delta = abs(newVolStep - lastDisplayedVolStep);
      if (delta >= VOL_UI_CHANGE_THRESHOLD) {
        volUiShown = true;
      }
    }

    // ボリュームUI表示中
    if (volUiShown && newVolStep != lastDisplayedVolStep) {
      _ampVolStep = newVolStep;
      lastDisplayedVolStep = newVolStep;
      setAmpStepGain(_ampVolStep, false);
      showVolumeUI(_ampVolStep);
      volUiLastShown = millis();
    } else if (!volUiShown) {
      if (newVolStep != _ampVolStep) {
        _ampVolStep = newVolStep;
        setAmpStepGain(_ampVolStep, false);
      }
    }

    // 一定時間操作なしでストリーム情報表示に戻る
    if (volUiShown && millis() - volUiLastShown > VOL_UI_TIMEOUT_MS) {
      volUiShown = false;
      showStreamStats();
      lastStatsUpdate = millis();
    }

    // ボリュームUI非表示時は定期的にストリーム情報を更新
    if (!volUiShown && millis() - lastStatsUpdate > 1000) {
      showStreamStats();
      lastStatsUpdate = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
#else
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
#endif
}
