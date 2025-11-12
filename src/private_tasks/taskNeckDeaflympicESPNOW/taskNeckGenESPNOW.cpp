#include "globals.h"
#include "adjustParams.h"  // グローバル adjustParams（共通定義）
#include "ui_defs.hpp"  // このタスク専用UI定義

enum class DeafMode : uint8_t { PlayAll = 0, NoScrape, ImpactOnly, VenueOnly, ArenaOnly, Test };

static DeafMode _mode = DeafMode::PlayAll;
[[maybe_unused]] static DeafMode _prevModeBeforeTest = DeafMode::PlayAll;  // テスト前のモードを記憶
static uint32_t _lastUserOpMs = 0;
static uint8_t _testSelId = 0;  // 0..5（電源ON中は保持）
static unsigned long _buzzerStopTime = 0;  // ブザー自動停止用タイマー
static bool _buzzerPlaying = false;  // ブザー再生中フラグ

// バッテリー監視用変数
static bool _isLowBattery = false;  // 低バッテリー状態フラグ
static bool _wasLowBattery = false;  // 前回の低バッテリー状態
static unsigned long _lastBatteryCheck = 0;  // 最後のバッテリーチェック時刻
static bool _batteryWarmupDone = false;  // 起動直後の誤判定防止フラグ
static uint8_t _batteryWarmupRetry = 0;  // SoC再取得のリトライ回数管理

// 拍手（ID=3）のランダム再生用
static bool _clapPlaying = false;  // 拍手再生中フラグ
static int _clapRemainingCount = 0;  // 残り再生回数
static unsigned long _clapNextTime = 0;  // 次の再生時刻

// 低バッテリー警告表示
static void showLowBatteryWarning() {
  _display.clearDisplay();
  
  // 日本語: "充電してください" = 7文字 × 16px = 112px
  // 英語: "Please charge" = 13文字 × 8px = 104px
  // 日本語の方が長いので、日本語を基準に中央寄せ
  const int centerX = (128 - 112) / 2;  // 8px
  
  displayManager::printEfont(&_display, LOW_BATTERY_MSG_JA, centerX, 0);
  displayManager::printEfont(&_display, LOW_BATTERY_MSG_EN, 12, 16);  // 英語は少し左寄せ
  _display.display();
}

// UI表示のヘルパ
static void showModeTitle() {
  // 画面全体をクリアしてから描画（前画面の残像を防止）
  _display.clearDisplay();
  
  // 日本語を中央寄せにするためのX座標
  // 最長の "競技所の音のみ" (7文字 × 16px = 112px) を基準
  // 中央: (128 - 112) / 2 = 8px
  const int centerX = 8;
  
  switch (_mode) {
    case DeafMode::PlayAll:
      // "全て再生" = 4文字 × 16px = 64px
      displayManager::printEfont(&_display, MENU_PLAYALL_JA, centerX + 24, 0);  // 8 + (112-64)/2 = 32
      displayManager::printEfont(&_display, MENU_PLAYALL_EN, centerX + 24, 16);
      break;
    case DeafMode::NoScrape:
      // "すり足 無し" = 6文字 × 16px = 96px
      displayManager::printEfont(&_display, MENU_NOSCRAPE_JA, centerX + 8, 0);  // 8 + (112-96)/2 = 16
      displayManager::printEfont(&_display, MENU_NOSCRAPE_EN, centerX + 8, 16);
      break;
    case DeafMode::ImpactOnly:
      // "軽打・重打のみ" = 7文字 × 16px = 112px
      displayManager::printEfont(&_display, MENU_IMPACTONLY_JA, centerX, 0);
      displayManager::printEfont(&_display, MENU_IMPACTONLY_EN, centerX, 16);
      break;
    case DeafMode::VenueOnly:
      displayManager::printEfont(&_display, MENU_VENUE_JA, 0, 0);
      displayManager::printEfont(&_display, MENU_VENUE_EN, 0, 16);
      break;
    case DeafMode::ArenaOnly:
      displayManager::printEfont(&_display, MENU_ARENA_JA, 0, 0);
      displayManager::printEfont(&_display, MENU_ARENA_EN, 0, 16);
      break;
    case DeafMode::Test: {
      const char *ja = TEST_ID_LABEL_JA[_testSelId % 6];
      const char *en = TEST_ID_LABEL_EN[_testSelId % 6];

      const int ytop=0;
      const int ybottom = 16;
      const int xtitle = 24;
      
      // 上段: 日本語ラベル（左寄せ） + play->（右寄せ）
      displayManager::printEfont(&_display, ja, xtitle, ytop);
      // "play->" は6文字半角 x 8px = 48px、右端揃え: 128 - 48 = 80
      displayManager::printEfont(&_display, "play->", 80, ytop);
      
      // 下段: ↙（左端） + 英語ラベル + ↘（右端）
      displayManager::printEfont(&_display, "\xE2\x86\x99", 0, ybottom);     // ↙ (U+2199)
      displayManager::printEfont(&_display, en, xtitle, ybottom);
      // "↘" は全角1文字 x 16px = 16px、右端揃え: 128 - 16 = 112
      displayManager::printEfont(&_display, "\xE2\x86\x98", 112, ybottom);   // ↘ (U+2198)
      break;
    }
  }
  _display.display();  // 更新を反映
}

// モードに応じたID制限の適用
static void applyLimitIds() {
  switch (_mode) {
    case DeafMode::PlayAll:
      audioManager::setLimitIds(MODE_PLAY_ALL_IDS, MODE_PLAY_ALL_IDS_SIZE);
      audioManager::setIsLimitEnable(false);
      break;
    case DeafMode::NoScrape:
      audioManager::setLimitIds(MODE_NO_SCRAPE_IDS, MODE_NO_SCRAPE_IDS_SIZE);
      audioManager::setIsLimitEnable(true);
      break;
    case DeafMode::ImpactOnly:
      audioManager::setLimitIds(MODE_IMPACT_ONLY_IDS, MODE_IMPACT_ONLY_IDS_SIZE);
      audioManager::setIsLimitEnable(true);
      break;
    case DeafMode::VenueOnly:
      audioManager::setLimitIds(MODE_VENUE_ONLY_IDS, MODE_VENUE_ONLY_IDS_SIZE);
      audioManager::setIsLimitEnable(true);
      break;
    case DeafMode::ArenaOnly:
      audioManager::setLimitIds(MODE_ARENA_ONLY_IDS, MODE_ARENA_ONLY_IDS_SIZE);
      audioManager::setIsLimitEnable(true);
      break;
    case DeafMode::Test:
      audioManager::setIsLimitEnable(false);
      break; // テストはローカル再生のみ
  }
}

void TaskNeckESPNOW() {
  // 本タスクでは FixMode は使用しない
  _isFixMode = false;
  audioManager::setIsFixMode(false);
  
  // ランダム関数のシードを初期化（拍手のランダム再生用）
  randomSeed(analogRead(AIN_VIBVOL_PIN));
  
  _currAIN = analogRead(AIN_VIBVOL_PIN);
  _ampVolStep = map(_currAIN, 0, 4095, 10, 0);  // 逆向き（高電圧=低ボリューム→低ステップ）
  setAmpStepGain(_ampVolStep, true);
  
  // ボリューム表示制御用
  static int lastDisplayedVolStep = _ampVolStep;  // 最後に表示した値
  static int volModeEntryStep = _ampVolStep;      // モードに入った時の値
  static unsigned long volUiLastShown = 0;
  static bool volUiShown = false;
  
  _lastUserOpMs = millis();
  applyLimitIds();
  showModeTitle();

  while (1) {
    _currAIN = analogRead(AIN_VIBVOL_PIN);
    int newVolStep = map(_currAIN, 0, 4095, 10, 0);  // 逆向き（高電圧=低ボリューム→低ステップ）
    
    // モード判定：表示モードでない場合、窓枠を超えたらモードに入る
    if (!volUiShown) {
      int delta = abs(newVolStep - lastDisplayedVolStep);
      if (delta >= VOL_UI_CHANGE_THRESHOLD) {
        // ボリューム変更モードに入る
        volUiShown = true;
        volModeEntryStep = lastDisplayedVolStep;  // モード開始時の値を記録
      }
    }
    
    // ボリューム変更モード中は1ステップずつ滑らかに更新
    if (volUiShown && newVolStep != lastDisplayedVolStep) {
      _ampVolStep = newVolStep;
      lastDisplayedVolStep = newVolStep;
      setAmpStepGain(_ampVolStep, false);
      
      // JUDO風ボリュームUI表示（@taskJUDO0806 と同一）
      _display.clearDisplay();
      _display.setFont();
      _display.setTextSize(2);
      _display.setTextColor(SSD1306_WHITE);
      char volTxt[8]; 
      snprintf(volTxt, sizeof(volTxt), "%d", _ampVolStep);
      _display.setCursor(VOL_UI_BAR_X + VOL_UI_BAR_W + 8, VOL_UI_BAR_Y - 1);
      _display.print(volTxt);
      _display.drawRect(VOL_UI_BAR_X, VOL_UI_BAR_Y, VOL_UI_BAR_W, VOL_UI_BAR_H, SSD1306_WHITE);
      int fillWidth = map(_ampVolStep, 0, 10, 0, VOL_UI_BAR_W - 2);
      if (fillWidth > 0) { 
        _display.fillRect(VOL_UI_BAR_X + 1, VOL_UI_BAR_Y + 1, fillWidth, VOL_UI_BAR_H - 2, SSD1306_WHITE); 
      }
      _display.display();
      volUiLastShown = millis();
    } else if (!volUiShown) {
      // 表示モードでない時もゲインは更新（音量は常に追従）
      if (newVolStep != _ampVolStep) {
        _ampVolStep = newVolStep;
        setAmpStepGain(_ampVolStep, false);
      }
    }
    
    // 一定時間操作が無ければメニュー表示へ戻す
    if (volUiShown && millis() - volUiLastShown > VOL_UI_TIMEOUT_MS) {
      volUiShown = false;
      showModeTitle();
    }

    for (int i = 0; i < sizeof(_SW_PIN) / sizeof(_SW_PIN[0]); i++) {
      if (!digitalRead(_SW_PIN[i]) && !_isBtnPressed[i]) {
        // 低バッテリー状態の時はボタン操作を無効化
        if (_isLowBattery) {
          _isBtnPressed[i] = true;
          continue;
        }
        
        audioManager::stopAudio();

        _lastUserOpMs = millis();
        uint8_t category_ID = audioManager::getCategoryID();
        uint8_t channel_ID = audioManager::getChannelID();
        // ボタン機能: 1ボタン=1アクション（モード選択）。テスト中は機能切替
        if (_mode != DeafMode::Test) {
          if (i == LOGICAL_BTN_IDX_TOP_LEFT) { 
            _mode = DeafMode::PlayAll; 
            applyLimitIds(); 
            showModeTitle(); 
          }
          else if (i == LOGICAL_BTN_IDX_TOP_RIGHT) { 
            _mode = DeafMode::NoScrape; 
            applyLimitIds(); 
            showModeTitle(); 
          }
          else if (i == LOGICAL_BTN_IDX_CENTER) { 
            _mode = DeafMode::ImpactOnly; 
            applyLimitIds(); 
            showModeTitle(); 
          }
          else if (i == LOGICAL_BTN_IDX_BOTTOM_LEFT) { 
            _mode = DeafMode::ArenaOnly; 
            applyLimitIds(); 
            showModeTitle(); 
          }
          else if (i == LOGICAL_BTN_IDX_BOTTOM_RIGHT) { 
            _mode = DeafMode::VenueOnly; 
            applyLimitIds(); 
            showModeTitle(); 
          }
        } else {
          // テストモード中の操作
          if (i == LOGICAL_BTN_IDX_TOP_LEFT) {
            // 左上: 全て再生モードへ戻る
            _mode = DeafMode::PlayAll; 
            applyLimitIds(); 
            showModeTitle();
          }
          else if (i == LOGICAL_BTN_IDX_TOP_RIGHT) {
            // 右上: すり足無しモードへ戻る
            _mode = DeafMode::NoScrape; 
            applyLimitIds(); 
            showModeTitle();
          }
          else if (i == LOGICAL_BTN_IDX_BOTTOM_LEFT) { 
            // 左下: prev（ID減）
            if (_testSelId > 0) _testSelId--; 
            showModeTitle(); 
          }
          else if (i == LOGICAL_BTN_IDX_BOTTOM_RIGHT) { 
            // 右下: next（ID増）
            if (_testSelId < 5) _testSelId++; 
            showModeTitle(); 
          }
          else if (i == LOGICAL_BTN_IDX_CENTER) {
            // 中央: テスト再生（受信経路と同ロジックに乗せる）
            uint8_t dummy_mac_addr[6] = {0};
            uint8_t packet[8];
            packet[0] = category_ID;
            packet[1] = channel_ID;
            packet[2] = audioManager::getDevicePos();
            packet[3] = _testSelId; // dataID
            packet[4] = 0; // sub
            
            // 各IDの振動の大きさを設定から取得
            uint8_t vol = TEST_MODE_VOLUME[_testSelId];
            packet[5] = vol; // L
            packet[6] = vol; // R
            
            // 拍手（ID=3）: ランダム回数再生
            if (_testSelId == 3) {
              packet[7] = 0; // oneshot
              audioManager::PlaySndOnDataRecv(dummy_mac_addr, packet, 8);
              
              // ランダム回数を決定（MIN～MAX）
              _clapRemainingCount = random(TEST_CLAP_MIN_COUNT, TEST_CLAP_MAX_COUNT + 1) - 1; // 既に1回再生したので-1
              _clapPlaying = true;
              // 次の再生時刻をランダムに設定
              int interval = random(TEST_CLAP_MIN_INTERVAL_MS, TEST_CLAP_MAX_INTERVAL_MS + 1);
              _clapNextTime = millis() + interval;
            }
            // ブザー(ID=5): 1秒ループ再生
            else if (_testSelId == 5) {
              packet[7] = 1; // loopStart
              _buzzerPlaying = true;
              _buzzerStopTime = millis() + 1000; // 1秒後に停止
              audioManager::PlaySndOnDataRecv(dummy_mac_addr, packet, 8);
            }
            // その他: 通常のoneshot
            else {
              packet[7] = 0; // oneshot
              audioManager::PlaySndOnDataRecv(dummy_mac_addr, packet, 8);
            }
          }
        }
        _isBtnPressed[i] = true;
      }
      if (digitalRead(_SW_PIN[i]) && _isBtnPressed[i]) _isBtnPressed[i] = false;
    };

    // 拍手の連続再生処理
    if (_clapPlaying && millis() >= _clapNextTime) {
      if (_clapRemainingCount > 0) {
        // 次の拍手を再生
        uint8_t dummy_mac_addr[6] = {0};
        uint8_t packet[8];
        packet[0] = audioManager::getCategoryID();
        packet[1] = audioManager::getChannelID();
        packet[2] = audioManager::getDevicePos();
        packet[3] = 3; // 拍手のID
        packet[4] = 0;
        uint8_t vol = TEST_MODE_VOLUME[3];
        packet[5] = vol;
        packet[6] = vol;
        packet[7] = 0; // oneshot
        audioManager::PlaySndOnDataRecv(dummy_mac_addr, packet, 8);
        
        _clapRemainingCount--;
        // 次の再生時刻をランダムに設定
        int interval = random(TEST_CLAP_MIN_INTERVAL_MS, TEST_CLAP_MAX_INTERVAL_MS + 1);
        _clapNextTime = millis() + interval;
      } else {
        // 全ての再生が完了
        _clapPlaying = false;
      }
    }

    // ブザー自動停止（1秒経過したら停止）
    if (_buzzerPlaying && millis() >= _buzzerStopTime) {
      uint8_t dummy_mac_addr[6] = {0};
      uint8_t packet[8];
      packet[0] = audioManager::getCategoryID();
      packet[1] = audioManager::getChannelID();
      packet[2] = audioManager::getDevicePos();
      packet[3] = 5; // ブザーのID
      packet[4] = 0;
      uint8_t vol = TEST_MODE_VOLUME[5];
      packet[5] = vol;
      packet[6] = vol;
      packet[7] = 2; // loopStop
      audioManager::PlaySndOnDataRecv(dummy_mac_addr, packet, 8);
      _buzzerPlaying = false;
    }

    // テストモードの自動復帰（60秒無操作でPlayAll）
    if (_mode == DeafMode::Test && millis() - _lastUserOpMs > MODE_TEST_IDLE_TIMEOUT_MS) {
      _mode = DeafMode::PlayAll;
      applyLimitIds();
      showModeTitle();
      _lastUserOpMs = millis();
    }

    // バッテリー監視（1秒間隔）
    unsigned long now = millis();
    if (now - _lastBatteryCheck >= 1000) {
      _lastBatteryCheck = now;
      
      uint16_t socPct;
      bool newBlinkOne;
#ifdef BAT_DEBUG
      static bool dbgLowBattery = false;
      static unsigned long dbgLast = 0;
      if (now - dbgLast >= 5000) { 
        dbgLast = now; 
        dbgLowBattery = !dbgLowBattery;  // 5秒ごとに通常/低バッテリーを切り替え
      }
      socPct = dbgLowBattery ? 0 : 50;  // 低バッテリー: 0%, 通常: 50%
      newBlinkOne = dbgLowBattery;
#else
      socPct = lipo.soc();
      if (!_batteryWarmupDone) {
        if (socPct == 0 && _batteryWarmupRetry < 5) {
          _batteryWarmupRetry++;
          delay(20);
          socPct = lipo.soc();
        } else {
          _batteryWarmupDone = (socPct > 0) || (_batteryWarmupRetry >= 5);
        }
        if (!_batteryWarmupDone) {
          continue;  // SoCが安定するまで低バッテリー処理を行わない
        }
      }
      newBlinkOne = (socPct < 3);
#endif
      
      _isLowBattery = (socPct < 1);  // 1%未満を低バッテリーとする
      
      // 低バッテリー状態が変化した場合の処理
      if (_isLowBattery && !_wasLowBattery) {
        // 低バッテリー状態になった
        _leds[0] = COLOR_LOW_BATTERY;
        FastLED.show();
        showLowBatteryWarning();
      } else if (!_isLowBattery && _wasLowBattery) {
        // 低バッテリー状態から復帰した
        _leds[0] = COLOR_VOL_MODE;  // 通常のボリュームモード色に戻す
        FastLED.show();
        showModeTitle();  // 通常のメニュー表示に戻す
      }
      
      _wasLowBattery = _isLowBattery;
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void TaskUI_ESPNOW(void *args) {
#if defined(NECKLACE_V2) || defined(NECKLACE_V3)
  TaskNeckESPNOW();
#endif
}

// 統一インターフェース
void TaskUI_Run(void *args) { TaskUI_ESPNOW(args); }






