#pragma once
// Deaflympic task UI専用定義（taskNeckGenESPNOW.cpp 内でのみ使用）

// ボタンインデックスは共通マクロ（include/adjustParams.h）を参照します
// このタスクでは上下反転に伴い、ロジカルなボタン位置を再マッピングします
extern const int LOGICAL_BTN_IDX_TOP_LEFT;
extern const int LOGICAL_BTN_IDX_TOP_RIGHT;
extern const int LOGICAL_BTN_IDX_CENTER;
extern const int LOGICAL_BTN_IDX_BOTTOM_LEFT;
extern const int LOGICAL_BTN_IDX_BOTTOM_RIGHT;

// モードごとのIDセット（端末側フィルタに使用）
extern const int MODE_PLAY_ALL_IDS[];
extern const int MODE_PLAY_ALL_IDS_SIZE;
extern const int MODE_NO_SCRAPE_IDS[];
extern const int MODE_NO_SCRAPE_IDS_SIZE;
extern const int MODE_IMPACT_ONLY_IDS[];
extern const int MODE_IMPACT_ONLY_IDS_SIZE;
extern const int MODE_VENUE_ONLY_IDS[];
extern const int MODE_VENUE_ONLY_IDS_SIZE;
extern const int MODE_ARENA_ONLY_IDS[];
extern const int MODE_ARENA_ONLY_IDS_SIZE;

// テストモードの無操作戻り時間（ミリ秒）
extern const unsigned long MODE_TEST_IDLE_TIMEOUT_MS;

// 表示ラベル（メニュー用：上段=JA, 下段=EN）
extern const char *MENU_PLAYALL_JA;
extern const char *MENU_PLAYALL_EN;
extern const char *MENU_NOSCRAPE_JA;
extern const char *MENU_NOSCRAPE_EN;
extern const char *MENU_IMPACTONLY_JA;
extern const char *MENU_IMPACTONLY_EN;
extern const char *MENU_VENUE_JA;
extern const char *MENU_VENUE_EN;
extern const char *MENU_ARENA_JA;
extern const char *MENU_ARENA_EN;

// テストモード用ラベル（id=0..5）
extern const char *TEST_ID_LABEL_JA[6];
extern const char *TEST_ID_LABEL_EN[6];

// ボリュームUI設定（@taskJUDO0806 と同一）
extern const int VOL_UI_BAR_X;
extern const int VOL_UI_BAR_Y;  // @taskJUDO0806 と同じ位置
extern const int VOL_UI_BAR_W;  // @taskJUDO0806 と同じ幅
extern const int VOL_UI_BAR_H;
extern const int VOL_UI_TEXT_SIZE;

// ちらつき防止：この値以上の変化があった時のみ画面更新
// 例: 1なら毎回更新、2なら2段階変化で更新、3以上推奨
extern const int VOL_UI_CHANGE_THRESHOLD;  // ±20前後の窓枠（調整可能）

// 無操作でUIを消してメニューへ戻る時間
extern const unsigned long VOL_UI_TIMEOUT_MS;

// テストモード専用設定
// 各IDの振動の大きさ（0-255）
extern const uint8_t TEST_MODE_VOLUME[6];  // すり足, 軽打, 重打, 拍手, 歓声, ブザー

// 拍手（ID=3）のランダム再生設定
extern const int TEST_CLAP_MIN_COUNT;  // 最小再生回数
extern const int TEST_CLAP_MAX_COUNT;  // 最大再生回数
extern const int TEST_CLAP_MIN_INTERVAL_MS;  // 最小間隔（ミリ秒）
extern const int TEST_CLAP_MAX_INTERVAL_MS;  // 最大間隔（ミリ秒）

// 低バッテリー警告設定
extern const CRGB COLOR_LOW_BATTERY;  // 赤色LED
extern const char *LOW_BATTERY_MSG_JA;
extern const char *LOW_BATTERY_MSG_EN;

