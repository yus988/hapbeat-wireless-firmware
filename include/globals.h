#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
// ディスプレイ関連
#include <Adafruit_SSD1306.h>
#include <FastLED.h>
#include <Wire.h>
// 自作ライブラリ
#if defined(BAND_V4)
#include <MAX17048.h>
#else
#include <BQ27220.h>
#endif
#include <audioManager.h>
#include <displayManager.h>

#include "task_entry.h"

#ifdef EN_MCP4018
  #include "MCP4018-SOLDERED.h"  // Include Soldered library for MCP4018 Digipot.
extern MCP4018_SOLDERED _digipot;  // Create object for Digipot library.
#endif

#include "adjustParams.h"
#include "pinAssign.h"

// 定数の宣言
extern const float SHUNT_RESISTANCE;  // シャント抵抗 (オーム)
extern const float INA_GAIN;          // INA180A2IDBVRのゲイン (V/V)
extern const int ADC_MAX;             // ADCの分解能
extern const float V_REF;             // アナログ基準電圧 (V)
extern const int BATTERY_CAPACITY;    // バッテリー容量 (mAh)

// adjustParams.cpp の変数追加
extern const int DISP_ROT;
extern const int FIX_GAIN_STEP[];

extern const char *CATEGORY_ID_TXT[];
extern const char *CHANNEL_ID_TXT[];
extern const CRGB COLOR_FIX_MODE;
extern const CRGB COLOR_VOL_MODE;
extern const CRGB COLOR_DANGER_MODE;
extern const char *GAIN_STEP_TXT[];
extern const int BAT_NOTIFY_SOC;
extern const int BAT_NOTIFY_VOL;
extern const bool ENABLE_WIRED_MUTE_FUNCTION;

#ifdef MQTT
extern const ID_definitions ID_MSG;
extern const MessageData DISP_MSG[];
extern const int LIMITED_IDS[];
extern const char *LIMIT_ENABLE_MSG[];
#endif

// タスク関連の変数
extern TaskHandle_t thp[3];  // タスクハンドル
extern bool _isFixMode;
extern unsigned long _lastDisplayUpdate;    // ディスプレイ更新時刻
extern unsigned long _lastBatStatusUpdate;  // バッテリー状態更新時刻

// LED関連の変数
extern CRGB _leds[1];
extern CRGB _currentColor;

// OLEDディスプレイ関連の変数
extern const int SCREEN_WIDTH;   // OLED _display width
extern const int SCREEN_HEIGHT;  // OLED _display height
extern Adafruit_SSD1306 _display;

// バージョンごとのスイッチとボタンの状態
#if defined(NECKLACE_V2) || defined(NECKLACE_V3)
extern int _SW_PIN[5];
extern bool _isBtnPressed[5];

// 音量関連の変数
extern int _prevAIN;
extern int _currAIN;
extern uint8_t _ampVolStep;
#endif

#if defined(BAND_V3) || defined(BAND_V4)
extern int _SW_PIN[3];
extern bool _isBtnPressed[3];
#elif defined(BAND_V2)
extern int _SW_PIN[2];
extern bool _isBtnPressed[2];
#endif

// 電流計算の関数宣言
float calculateCurrent(int adc_value);
void setFixGain(bool updateOLED = true);
void setAmpStepGain(int step, bool updateOLED = true);
void showTextWithParams(const char *text, uint8_t posX, uint8_t posY,
                        bool isClearDisplay);
#endif  // GLOBALS_H
