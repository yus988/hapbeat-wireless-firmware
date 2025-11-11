// Parameters for TASK_NECK_GEN_ESPNOW (Necklace)

// 注意: この配列の要素数を変更した場合、同じディレクトリの
// audioManagerSettings.hpp 内の CATEGORY_NUM も同じ値に更新してください。
const char *CATEGORY_ID_TXT[] = {"cat_0", "cat_1", "cat_2"};
const char *CHANNEL_ID_TXT[] = {"ch_0", "ch_1", "ch_2", "ch_3"};
const int FIX_GAIN_STEP[] = {14, 14, 14};
const char *GAIN_STEP_TXT[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
const int CATEGORY_TEXT_POS[2] = {0, 8};
const int CHANNEL_TEXT_POS[2] = {54, 8};
const int GAIN_STEP_TEXT_POS[2] = {100, 8};
const int DISP_ROT = 2;  // 上下反転
const int FONT_SIZE = 1;
const CRGB COLOR_FIX_MODE = CRGB(10, 10, 10);
const CRGB COLOR_VOL_MODE = CRGB(10, 10, 10);
const int DEVICE_POS = 0;  // neck

// Sizes
const int CATEGORY_ID_TXT_SIZE = sizeof(CATEGORY_ID_TXT) / sizeof(CATEGORY_ID_TXT[0]);
const int CHANNEL_ID_TXT_SIZE = sizeof(CHANNEL_ID_TXT) / sizeof(CHANNEL_ID_TXT[0]);
const int GAIN_STEP_TXT_SIZE = sizeof(GAIN_STEP_TXT) / sizeof(GAIN_STEP_TXT[0]);


// 音声設定マクロは audioManagerSettings.hpp に分離
// UI/操作専用設定の値は本ファイルで設定し、宣言は ui_defs.hpp に置く

// =========================
// UI/操作用パラメータの値
// =========================
// ボタンインデックス（上下反転に伴うロジカルマッピング）
extern const int LOGICAL_BTN_IDX_TOP_LEFT = DUOWL_BTN_IDX_BOTTOM_RIGHT;
extern const int LOGICAL_BTN_IDX_TOP_RIGHT = DUOWL_BTN_IDX_BOTTOM_LEFT;
extern const int LOGICAL_BTN_IDX_CENTER = DUOWL_BTN_IDX_CENTER;
extern const int LOGICAL_BTN_IDX_BOTTOM_LEFT = DUOWL_BTN_IDX_TOP_RIGHT;
extern const int LOGICAL_BTN_IDX_BOTTOM_RIGHT = DUOWL_BTN_IDX_TOP_LEFT;

// モードごとのIDセット（端末側フィルタに使用）
extern const int MODE_PLAY_ALL_IDS[] = {0, 1, 2, 3, 4, 5};
extern const int MODE_PLAY_ALL_IDS_SIZE = 6;
extern const int MODE_NO_SCRAPE_IDS[] = {1, 2, 3, 4, 5};
extern const int MODE_NO_SCRAPE_IDS_SIZE = 5;
extern const int MODE_IMPACT_ONLY_IDS[] = {1, 2};
extern const int MODE_IMPACT_ONLY_IDS_SIZE = 2;
extern const int MODE_VENUE_ONLY_IDS[] = {3, 4, 5};
extern const int MODE_VENUE_ONLY_IDS_SIZE = 3;
extern const int MODE_ARENA_ONLY_IDS[] = {0, 1, 2};
extern const int MODE_ARENA_ONLY_IDS_SIZE = 3;

// テストモードの無操作戻り時間（ミリ秒）
extern const unsigned long MODE_TEST_IDLE_TIMEOUT_MS = 60000UL;

// 表示ラベル（メニュー用：上段=JA, 下段=EN）
extern const char *MENU_PLAYALL_JA = "全て再生";
extern const char *MENU_PLAYALL_EN = "Play all";
extern const char *MENU_NOSCRAPE_JA = "すり足 無し";
extern const char *MENU_NOSCRAPE_EN = "No Slide";
extern const char *MENU_IMPACTONLY_JA = "軽打・重打のみ";
extern const char *MENU_IMPACTONLY_EN = "Tap & Hit only";
extern const char *MENU_VENUE_JA = "会場の音のみ";
extern const char *MENU_VENUE_EN = "Venue sound only";
extern const char *MENU_ARENA_JA = "競技所の音のみ";
extern const char *MENU_ARENA_EN = "Arena sound only";

// テストモード用ラベル（id=0..5）
extern const char *TEST_ID_LABEL_JA[6] = {"すり足", "軽打", "重打", "拍手", "歓声", "ブザー"};
extern const char *TEST_ID_LABEL_EN[6] = {"Slide", "Tap", "Hit", "Clap", "Cheer", "Beep"};

// ボリュームUI設定（@taskJUDO0806 と同一）
extern const int VOL_UI_BAR_X = 10;
extern const int VOL_UI_BAR_Y = 10;  // @taskJUDO0806 と同じ位置
extern const int VOL_UI_BAR_W = 80;  // @taskJUDO0806 と同じ幅
extern const int VOL_UI_BAR_H = 12;
extern const int VOL_UI_TEXT_SIZE = 2;

// ちらつき防止：この値以上の変化があった時のみ画面更新
// 例: 1なら毎回更新、2なら2段階変化で更新、3以上推奨
extern const int VOL_UI_CHANGE_THRESHOLD = 2;  // 2段階変化で更新（0～10の11段階に適応）

// 無操作でUIを消してメニューへ戻る時間
extern const unsigned long VOL_UI_TIMEOUT_MS = 1500UL;

// テストモード専用設定
// 各IDの振動の大きさ（0-255）
extern const uint8_t TEST_MODE_VOLUME[6] = {100, 100, 100, 100, 100, 100};  // すり足, 軽打, 重打, 拍手, 歓声, ブザー

// 拍手（ID=3）のランダム再生設定
extern const int TEST_CLAP_MIN_COUNT = 6;  // 最小再生回数
extern const int TEST_CLAP_MAX_COUNT = 6;  // 最大再生回数
extern const int TEST_CLAP_MIN_INTERVAL_MS = 50;  // 最小間隔（ミリ秒）- 短いインターバル対応
extern const int TEST_CLAP_MAX_INTERVAL_MS = 200;  // 最大間隔（ミリ秒）- 短いインターバル対応

// 低バッテリー警告設定
extern const CRGB COLOR_LOW_BATTERY = CRGB(10, 0, 0);  // 赤色LED
extern const char *LOW_BATTERY_MSG_JA = "充電してください";
extern const char *LOW_BATTERY_MSG_EN = "Please charge";




