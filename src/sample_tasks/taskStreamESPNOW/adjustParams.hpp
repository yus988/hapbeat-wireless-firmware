// Parameters for TASK_NECK_STREAM_ESPNOW (Necklace - Audio Streaming)

const char *CATEGORY_ID_TXT[] = {"stream"};
const char *CHANNEL_ID_TXT[] = {"ch_0"};
const int FIX_GAIN_STEP[] = {14};
const char *GAIN_STEP_TXT[] = {"0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
                                "8",  "9",  "10", "11", "12", "13", "14", "15",
                                "16", "17", "18", "19", "20", "21", "22", "23"};
const int CATEGORY_TEXT_POS[2] = {0, 8};
const int CHANNEL_TEXT_POS[2] = {54, 8};
const int GAIN_STEP_TEXT_POS[2] = {100, 8};
const int DISP_ROT = 0;
const int FONT_SIZE = 1;
const CRGB COLOR_FIX_MODE = CRGB(10, 10, 10);
const CRGB COLOR_VOL_MODE = CRGB(0, 0, 10);
const int DEVICE_POS = 0;  // neck

const int CATEGORY_ID_TXT_SIZE =
    sizeof(CATEGORY_ID_TXT) / sizeof(CATEGORY_ID_TXT[0]);
const int CHANNEL_ID_TXT_SIZE =
    sizeof(CHANNEL_ID_TXT) / sizeof(CHANNEL_ID_TXT[0]);
const int GAIN_STEP_TXT_SIZE =
    sizeof(GAIN_STEP_TXT) / sizeof(GAIN_STEP_TXT[0]);
