#include "audioManager.h"
#include <LittleFS.h>
#include <string>
#include <vector>

#include "FS.h"
// オーディオ再生関連 ESP8266
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMP3.h"
// EEPROM
#include <EEPROM.h>
// audio libraries
#include <AudioGeneratorMP3.h>

#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "AudioOutputMixer.h"

namespace audioManager {
// ログを一括で出力し、分割出力の取りこぼしを防ぐ
static inline void logNoAudioMapped(uint8_t currentCategory,
                                    uint8_t dataId,
                                    uint8_t subId,
                                    int isRight,
                                    uint8_t stub,
                                    const uint8_t *lastPacket,
                                    int lastLen) {
  char buf[256];
  int pos = snprintf(buf, sizeof(buf),
                     "No audio mapped. cat=%u, dataID=%u, subID=%u, isR=%d, stub=%u, packet=(",
                     currentCategory, dataId, subId, isRight, stub);
  for (int i = 0; i < lastLen && i < 8; ++i) {
    if (pos < (int)sizeof(buf)) {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%u%s",
                      lastPacket[i], (i == 7 || i == lastLen - 1) ? ")" : ",");
    }
  }
  if (pos >= (int)sizeof(buf)) {
    // 末尾が切れた場合でも行として成立させる
    buf[sizeof(buf) - 2] = ')';
    buf[sizeof(buf) - 1] = '\0';
  }
  USBSerial.println(buf);
}

#ifdef DEBUG_WL
  #define DEBUG_PRINT(x) USBSerial.print(x)
  #define DEBUG_PRINTLN(x) USBSerial.println(x)
  #define DEBUG_PRINTF(fmt, ...) USBSerial.printf(fmt, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)
#endif

struct MessageData {
  uint8_t id;
  const char *message;
  MessageData(uint8_t i, const char *m) : id(i), message(m) {}
};
std::vector<MessageData> messages;  // 実際のインスタンス化

std::vector<bool> isPlayAudio(STUB_NUM, false);
// ディスプレイに表示するコールバック関数
void (*statusCallback)(const char *);

struct DataPacket {
  uint8_t category;
  uint8_t channelId;
  uint8_t devicePos;
  uint8_t dataID;
  uint8_t subID;
  uint8_t lVol;
  uint8_t rVol;
  uint8_t playCmd;
};

int maxVol = 255;
// position identifier
// 0:neck, 1:chest, 2:abdomen
// 3:upperArm_L, 4:upperArm_R, 5:wrist_L, 6:wrist_R,
// 7:thigh_L, 8:thigh_R, 9:calf_L, 10:calf_R
uint8_t _devicePos;  // 装着場所の指定
uint8_t _gainNum;    //
// EEPROM内に保存するデバイスコンフィグを格納
struct ConfigData {
  uint8_t categoryNum;
  uint8_t channelId;  // 装着者のID。99でブロードキャスト
  bool isFixMode;
  bool isLimitEnable;
  uint8_t volumeLevels[CATEGORY_NUM];  // カテゴリごとのボリュームレベル
                                       // (最大CATEGORY_NUMカテゴリ)
};
ConfigData _settings;

// MAX98357 gain select
uint8_t sel_A_pin;
uint8_t sel_B_pin;

AudioGeneratorWAV *_wav_gen[STUB_NUM];
AudioFileSourceLittleFS *_audioFileSrc[SOUND_FILE_NUM];
String _audioFileNames[SOUND_FILE_NUM];
int16_t *_audioRAM[SOUND_FILE_NUM];         // RAMに格納するデータのポインタ
uint8_t _audioStorageType[SOUND_FILE_NUM];  // データの格納場所を示すフラグ
AudioOutputI2S *_i2s_out;
AudioOutputMixer *_mixer;
AudioOutputMixerStub *_stub[STUB_NUM];

AudioFileSource *_audioFileSources[SOUND_FILE_NUM] = {nullptr};

// グローバル変数またはクラスメンバとして以前のオーディオソースを保持する配列を宣言
AudioFileSource *_previousSources[STUB_NUM] = {nullptr};

// [cat][pos][dataID][_subID][isRight]
uint8_t _audioDataIndex[CATEGORY_NUM][POSITION_NUM][DATA_NUM][SUB_DATA_NUM][2];
size_t _audioDataSize[SOUND_FILE_NUM];
uint8_t _currentCategory = 0;  // 受信コマンドで指定されたカテゴリ（stub 非依存）
uint8_t _dataID[STUB_NUM];
uint8_t _subID[STUB_NUM];
uint8_t _volume[STUB_NUM];
uint8_t _playingIdx;  // 再生中のidxを表示

// CATEGORY_ID_TXT_SIZEを格納する変数
static uint8_t _categorySize = 0;

// 再送無視処理（ループの時だけ）
unsigned long _lastReceiveTime = 0;
uint8_t _lastData[8];  // データの長さが8バイトの場合（適宜変更）
const unsigned long _ignoreDuration = 300;  // 一定時間以内の重複データを無視
bool _ignoreLoopData = false;  // ループ再生データを無視するためのフラグ

// 送信機・中継機からの重複メッセージ無視処理
unsigned long _lastDuplicateCheckTime = 0;
uint8_t _lastDuplicateData[8];  // 重複チェック用の前回データ
const unsigned long _duplicateIgnoreDuration =
    50;  // 50ms以内の重複データを無視

// ループ継続のkeep-alive管理
static bool _loopKeepAliveActive = false;
static unsigned long _loopKeepAliveDeadlineMs = 0;

bool isSameData(const uint8_t *data, int len) {
  for (int i = 0; i < len; i++) {
    if (data[i] != _lastData[i]) {
      return false;  // 一つでも異なるバイトがあれば別データ
    }
  }
  return true;  // 全て同じであれば重複
}

// data[2]以外が同じかどうかをチェックする関数
bool isSameDataExceptPos2(const uint8_t *data, const uint8_t *lastData,
                          int len) {
  for (int i = 0; i < len; i++) {
    if (i == 2) continue;  // data[2]はスキップ
    if (data[i] != lastData[i]) {
      return false;  // data[2]以外で異なるバイトがあれば別データ
    }
  }
  return true;  // data[2]以外が全て同じであれば重複
}

// モード選択の対象にするIDを格納
int _limitIDs[10];        // 最大10個のIDを格納する配列
size_t _numLimitIDs = 0;  // 現在格納されているIDの数

void initParamsEEPROM() {
  size_t eepromSize = sizeof(ConfigData);
  EEPROM.begin(eepromSize);
  EEPROM.get(0, _settings);
  if (_settings.channelId == 0xFF || _settings.categoryNum == 0xFF ||
      _settings.isFixMode == 0xFF) {
    _settings.categoryNum = 0;
    _settings.channelId = 0;
    _settings.isFixMode = true;
    _settings.isLimitEnable = false;

    // デフォルトのボリュームレベルを設定
    for (uint8_t i = 0; i < CATEGORY_NUM; i++) {
      _settings.volumeLevels[i] = 15;
    }
    // 設定をEEPROMに保存
    EEPROM.put(0, _settings);
    EEPROM.commit();
  }

  // EEPROMから読み込んだボリュームレベルの範囲チェック
  for (uint8_t i = 0; i < CATEGORY_NUM; i++) {
    if (_settings.volumeLevels[i] > VOLUME_MAX) {
      _settings.volumeLevels[i] = 0;  // 範囲外の値をリセット
    }
  }
}

// ファイルの読み込み
void readAllSoundFiles() {
  // LittleFSマウント確認
  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS Mount Failed");
    return;
  }
  // インデックス類を未設定値で初期化
  memset(_audioDataIndex, 0xFF, sizeof(_audioDataIndex));
  for (int i = 0; i < SOUND_FILE_NUM; ++i) {
    _audioStorageType[i] = 0xFF;
    _audioDataSize[i] = 0;
    _audioRAM[i] = nullptr;
    _audioFileNames[i] = String();
  }
  uint8_t isRight;
  uint8_t cat;
  uint8_t dataID;
  uint8_t subID;
  File root = LittleFS.open("/");
  File _file = root.openNextFile();
  uint8_t fileIdx = 0;
  String storageType;

  while (_file) {
    // ファイル名から種別を取得
    String fileName = _file.name();
    // 区切り文字の位置を取得
    int hyphenPos1 = fileName.indexOf('-');
    int underscorePos1 = fileName.indexOf('_', hyphenPos1 + 1);
    int hyphenPos2 = fileName.indexOf('-', underscorePos1 + 1);
    int hyphenPos3 = fileName.indexOf('-', hyphenPos2 + 1);
    int hyphenPos4 = fileName.indexOf('-', hyphenPos3 + 1);

    // 形式チェック（不足・順序不正はスキップ）
    if (hyphenPos1 < 0 || underscorePos1 < 0 || hyphenPos2 < 0 ||
        hyphenPos3 < 0 || hyphenPos4 < 0 ||
        !(hyphenPos1 < underscorePos1 && underscorePos1 < hyphenPos2 &&
          hyphenPos2 < hyphenPos3 && hyphenPos3 < hyphenPos4)) {
      DEBUG_PRINTF("Skip (filename format): %s\n", fileName.c_str());
      _file = root.openNextFile();
      continue;
    }

    // 各要素の抽出
    cat = fileName.substring(0, hyphenPos1).toInt();
    dataID = fileName.substring(hyphenPos1 + 1, underscorePos1).toInt();
    subID = fileName.substring(underscorePos1 + 1, hyphenPos2).toInt();
    // subID = fileName.substring(4, 5).toInt();
    isRight = (fileName[hyphenPos2 + 1] == 'R') ? 1 : 0;
    storageType = fileName.substring(hyphenPos3 + 1, hyphenPos4);  // "FS"
    DEBUG_PRINTF(
        "FILE : %s, cat = %d, id = %d, subId = %d, isRight: %d, "
        "Storage: %s\n",
        fileName.c_str(), cat, dataID, subID, isRight, storageType.c_str());

    // 範囲チェック
    bool inRange = true;
    if (cat >= CATEGORY_NUM) { inRange = false; DEBUG_PRINTF("Skip (cat): %s\n", fileName.c_str()); }
    if (dataID >= DATA_NUM) { inRange = false; DEBUG_PRINTF("Skip (dataID): %s\n", fileName.c_str()); }
    if (subID >= SUB_DATA_NUM) { inRange = false; DEBUG_PRINTF("Skip (subID): %s\n", fileName.c_str()); }
    if (fileIdx >= SOUND_FILE_NUM) { inRange = false; DEBUG_PRINTF("Skip (fileIdx overflow): %s\n", fileName.c_str()); }

    if (inRange) {
      _audioDataSize[fileIdx] = _file.size();
      // データの格納場所を決定
      if (storageType == "RAM") {
        // 16ビット（2バイト）ごとのデータ
        _audioRAM[fileIdx] = new int16_t[_audioDataSize[fileIdx] / 2];
        _file.read(reinterpret_cast<uint8_t *>(_audioRAM[fileIdx]),
                   _audioDataSize[fileIdx]);
        _audioStorageType[fileIdx] = RAM_STORAGE;

        DEBUG_PRINTLN("Stored in RAM_STORAGE");
      } else {
        // ファイル名のみ格納
        String fullPath = "/" + fileName;
        _audioFileNames[fileIdx] = fullPath;
        _audioStorageType[fileIdx] = FS_STORAGE;
        DEBUG_PRINTLN("Stored in FS_STORAGE");
      }

      _audioDataIndex[cat][0][dataID][subID][isRight] = fileIdx;
      fileIdx++;
    }
    _file = root.openNextFile();
  }
}

// 引数無しの場合は全てのstubを停止
void stopAudio(uint8_t stub) {
  // デフォルト=99はヘッダーファイルに記載。taskUIからの呼び出し時に必須
  if (stub == 99) {
    for (int iStub = 0; iStub < STUB_NUM; iStub++) {
      _wav_gen[iStub]->stop();
      _stub[iStub]->stop();
      isPlayAudio[iStub] = false;
    }
  } else {
    _wav_gen[stub]->stop();
    _stub[stub]->stop();
    isPlayAudio[stub] = false;
  }
}

void playAudio(uint8_t tStubNum, uint8_t tVol, bool isLoop) {
  int isLR = (tStubNum % 2 == 0) ? 0 : 1;
  uint8_t pos = 0;  // pos = 0 は仮置き
  if (_currentCategory >= CATEGORY_NUM) {
    DEBUG_PRINTLN("Invalid current category");
    return;
  }
  uint8_t idx = _audioDataIndex[_currentCategory][pos][_dataID[tStubNum]]
                               [_subID[tStubNum]][isLR];
  if (idx == 0xFF || idx >= SOUND_FILE_NUM || _audioStorageType[idx] == 0xFF) {
    logNoAudioMapped(_currentCategory, _dataID[tStubNum], _subID[tStubNum], isLR,
                     tStubNum, _lastDuplicateData, 8);
    return;
  }

  _volume[tStubNum] = tVol;
  _stub[tStubNum]->SetGain((float)tVol / maxVol);

  if (_wav_gen[tStubNum]->isRunning()) {
    _wav_gen[tStubNum]->stop();
  }
  // ★ 初回または非ループ時はAudioFileSourceを新規作成
  if (_previousSources[tStubNum] != nullptr) {
    delete _previousSources[tStubNum];
    _previousSources[tStubNum] = nullptr;
  }

  AudioFileSource *src = nullptr;
  if (_audioStorageType[idx] == RAM_STORAGE) {
    src = new AudioFileSourcePROGMEM(_audioRAM[idx], _audioDataSize[idx]);
    if (!_wav_gen[tStubNum]->begin(src, _stub[tStubNum])) {
      USBSerial.println("Failed to start WAV generator with RAM data");
      delete src;
      return;
    }
  } else {
    src = new AudioFileSourceLittleFS(_audioFileNames[idx].c_str());
    if (!src->isOpen()) {
      USBSerial.printf("Failed to open file: %s\n",
                       _audioFileNames[idx].c_str());
      delete src;
      return;
    }
    if (!_wav_gen[tStubNum]->begin(src, _stub[tStubNum])) {
      USBSerial.println("Failed to start WAV generator with FS data");
      delete src;
      return;
    }
  }
  _previousSources[tStubNum] = src;
  isPlayAudio[tStubNum] = true;
  DEBUG_PRINTF("Succeed to play with stub: %d\n", tStubNum);
}

void PlaySndOnDataRecv(const uint8_t *mac_addr, const uint8_t *data,
                       int data_len) {
  unsigned long recvStartTime = micros();
  unsigned long currentTime = millis();

#if defined(ESPNOW_REPEATER_ONLY)
  if (data_len <= 2 || data[2] != 50) {
    DEBUG_PRINTLN("Skipped packet: not from repeater (devicePos != 50)");
    return;
  }
  uint8_t repeaterDevicePos = data[2];
#endif

  // 送信機・中継機からの重複メッセージチェック（data[2]以外が同じ場合）
  if (currentTime - _lastDuplicateCheckTime < _duplicateIgnoreDuration &&
      isSameDataExceptPos2(data, _lastDuplicateData, data_len)) {
    DEBUG_PRINTLN("Ignoring duplicate message from transmitter/relay");
    return;
  }

  // 前回受信データを更新
  _lastDuplicateCheckTime = currentTime;
  for (int i = 0; i < data_len; i++) {
    _lastDuplicateData[i] = data[i];
  }

  // ループ再生（data[7] == 1）のデータで、無視期間中の場合
  if (_ignoreLoopData && currentTime - _lastReceiveTime < _ignoreDuration) {
    DEBUG_PRINTLN("Ignoring loop data during _ignoreDuration");
    return;
  }

  DEBUG_PRINTF(
      "received: _category %d, _channel_Id %d, _devPos %d, _dataID %d, "
      "_subID "
      "%d, Vol %d:%d, playtype %d\n",
      data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

  uint8_t playCmd = data[7];
  
  // 受信IDの制限（UIモードで指定された場合）
  // 新しい再生開始コマンド(0,1,3)のみを制限。制御コマンド(2=stop, 9=continue)は制限しない
  if (_settings.isLimitEnable && (playCmd == 0 || playCmd == 1 || playCmd == 3)) {
    bool allowed = false;
    for (size_t i = 0; i < _numLimitIDs; ++i) {
      if ((int)data[3] == _limitIDs[i]) { allowed = true; break; }
    }
    if (!allowed) {
      DEBUG_PRINTF("Blocked by limit IDs (dataID=%d, playCmd=%d)\n", data[3], playCmd);
      return;
    }
  }

  // ループ再生のデータ（data[7] ==
  // 1）の場合、無視フラグを立てて無視期間を設定
  // 中継モード（data[2] != 50）の場合は干渉させない。あくまで暫定処理
  if (data[7] == 1) {
    _ignoreLoopData = true;
    _lastReceiveTime = currentTime;
  } else {
    _ignoreLoopData = false;
  }

  // カテゴリーを99で通すのはNG。ファイル名と紐づいているので、
  // data[0]とファイルのカテゴリは必ず合わないといけない。
  // カテゴリに限らず通すなら isEventModeで通す（audioManager.h）
  // 後日 adjustParams.h に移すこと（set isEventModeを実装する）
  bool devicePosMatches = (data[2] == _devicePos || data[2] == 99 || data[2] == 50);

  if ((data[0] == _settings.categoryNum || isEventMode) &&
      (data[1] == _settings.channelId || data[1] == 99) &&
      devicePosMatches) {
    if (playCmd == 2) {
      for (int iStub = 2; iStub <= 3; ++iStub) {
        while (_wav_gen[iStub]->isRunning()) {
          stopAudio(iStub);
        }
      }
      // 停止時はkeep-aliveを無効化
      _loopKeepAliveActive = false;
    } else {
      int startIdx;
      switch (playCmd) {
        case 0:
          startIdx = 0;
          break;
        case 1:
          startIdx = 2;
          // ループ開始時にkeep-aliveを有効化し期限を更新
          _loopKeepAliveActive = true;
          _loopKeepAliveDeadlineMs = currentTime + LOOP_CONTINUE_TIMEOUT_MS;
          break;
        case 9:
          // continue: 期限を延長して以降の処理は行わない
          if (_loopKeepAliveActive) {
            _loopKeepAliveDeadlineMs = currentTime + LOOP_CONTINUE_TIMEOUT_MS;
          }
          return;
        case 3:
          startIdx = 4;
          break;
      }

      // カテゴリは stub に依存させず単一で保持
      _currentCategory = data[0];
      for (int i = startIdx; i <= startIdx + 1; ++i) {
        _dataID[i] = data[3];
        _subID[i] = data[4];
        _volume[i] = data[5 + i % 2];
      }

      // L-とR-のファイルが両方存在するかチェック
      uint8_t pos = 0;
      uint8_t idxL = _audioDataIndex[_currentCategory][pos][data[3]][data[4]][0];
      uint8_t idxR = _audioDataIndex[_currentCategory][pos][data[3]][data[4]][1];
      bool hasStereoFiles = (idxL != 0xFF && idxL < SOUND_FILE_NUM && _audioStorageType[idxL] != 0xFF) &&
                            (idxR != 0xFF && idxR < SOUND_FILE_NUM && _audioStorageType[idxR] != 0xFF);

      uint8_t stub = startIdx;
      stopAudio(stub);
      playAudio(stub, _volume[stub]);

      // ステレオファイル（L-とR-のペア）が存在する場合のみ、両方再生
      // モノラル（C-ファイルのみ）の場合は、左右のボリュームに関わらず左のみ再生
      if (hasStereoFiles) {
        stopAudio(stub + 1);
        playAudio(stub + 1, _volume[stub + 1]);
      }
    }
  }

  // ★ 処理終了時刻の記録
  unsigned long recvEndTime = micros();

  // ★ 処理時間の計算（μs → ms）
  float processingTimeMs = (recvEndTime - recvStartTime) / 1000.0;

  // ★ 処理時間の出力
  // DEBUG_PRINTF("★ 受信から音声再生開始までの遅延: %.3f ms\n", processingTimeMs);
}

// MQTT接続にESPNOWのコールバックを実行するためのリレー関数
void PlaySndFromMQTTcallback(char *topic, byte *payload, unsigned int length) {
  // デバッグ出力（USBSerialで確実に出力）
  USBSerial.println("=== PlaySndFromMQTTcallback START ===");
  USBSerial.print("Topic: ");
  USBSerial.println(topic);
  USBSerial.print("Payload length: ");
  USBSerial.println(length);
  USBSerial.print("Payload: ");
  for (unsigned int i = 0; i < length; i++) {
    USBSerial.print((char)payload[i]);
  }
  USBSerial.println();
  
  DEBUG_PRINTLN();
  DEBUG_PRINT("Message arrived in topic: ");
  DEBUG_PRINTLN(topic);

  DEBUG_PRINT("Message: ");
  for (int i = 0; i < length; i++) {
    DEBUG_PRINT((char)payload[i]);
  }
  DEBUG_PRINTLN();

  // 文字列を整数に変換する処理
  String payloadStr((char *)payload, length);
  int values[8];
  int i = 0;
  int startIndex = 0;

  for (int j = 0; j < payloadStr.length(); j++) {
    if (payloadStr[j] == ',' || j == payloadStr.length() - 1) {
      if (j == payloadStr.length() - 1) j++;
      String valueStr = payloadStr.substring(startIndex, j);
      values[i++] = valueStr.toInt();
      startIndex = j + 1;
    }
  }

  // データをESP-NOW形式に変換
  DataPacket dataPacket;
  dataPacket.category = values[0];
  dataPacket.channelId = values[1];
  dataPacket.devicePos = values[2];
  dataPacket.dataID = values[3];
  dataPacket.subID = values[4];
  dataPacket.lVol = values[5];
  dataPacket.rVol = values[6];
  dataPacket.playCmd = values[7];
  
  // 変換後のデータを出力
  USBSerial.println("=== Converted DataPacket ===");
  USBSerial.print("category: "); USBSerial.println(dataPacket.category);
  USBSerial.print("channelId: "); USBSerial.println(dataPacket.channelId);
  USBSerial.print("devicePos: "); USBSerial.println(dataPacket.devicePos);
  USBSerial.print("dataID: "); USBSerial.println(dataPacket.dataID);
  USBSerial.print("subID: "); USBSerial.println(dataPacket.subID);
  USBSerial.print("lVol: "); USBSerial.println(dataPacket.lVol);
  USBSerial.print("rVol: "); USBSerial.println(dataPacket.rVol);
  USBSerial.print("playCmd: "); USBSerial.println(dataPacket.playCmd);

  // 制限されたIDがあるか確認
  for (size_t i = 0; i < _numLimitIDs; ++i) {
    if (_settings.isLimitEnable == true && _limitIDs[i] == dataPacket.dataID) {
      DEBUG_PRINTLN("ID is restricted, aborting action.");
      return;  // 制限されたIDが見つかった場合、ここで処理を中断
    }
  }

  // dataPacket.dataIDに対応するMessageDataオブジェクトを検索
  for (int i = 0; i < messages.size(); ++i) {
    if (messages[i].id == dataPacket.dataID) {
      statusCallback(messages[i].message);
      break;  // 対応するメッセージを見つけたらループを抜ける
    }
  }

  // ダミーのMACアドレス（適宜設定）
  uint8_t dummy_mac_addr[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00};

  USBSerial.println("=== Calling PlaySndOnDataRecv ===");
  
  // ESP-NOWのコールバック関数を呼び出す
  PlaySndOnDataRecv(dummy_mac_addr, (uint8_t *)&dataPacket, sizeof(dataPacket));
  
  USBSerial.println("=== PlaySndFromMQTTcallback END ===");
}

void playAudioInLoop() {
  // keep-alive期限切れチェック（ループ用stubのみ対象）
  if (_loopKeepAliveActive && millis() > _loopKeepAliveDeadlineMs) {
    stopAudio();
    _loopKeepAliveActive = false;
  }

  for (int iStub = 0; iStub < STUB_NUM; iStub++) {
    if (isPlayAudio[iStub]) {
      // DEBUG_PRINTF("playing stub: ");
      // DEBUG_PRINTLN(iStub);
      if ((iStub == 2 || iStub == 3) &&
          _wav_gen[iStub]->isRunning()) {  // loop _stub case
        if (!_wav_gen[iStub]->loop()) {
          // --- ギャップレスループ再生 ---
          // 新しい AudioGeneratorWAV を生成し、準備が整ってから置き換える。
          bool restarted = false;
          if (_previousSources[iStub] != nullptr &&
              _previousSources[iStub]->seek(0, SEEK_SET)) {
            AudioGeneratorWAV *newGen = new AudioGeneratorWAV();
            if (newGen->begin(_previousSources[iStub], _stub[iStub])) {
              _stub[iStub]->SetGain((float)_volume[iStub] / maxVol);

              // 旧インスタンスを止めて破棄
              _wav_gen[iStub]->stop();
              delete _wav_gen[iStub];
              _wav_gen[iStub] = newGen;

              isPlayAudio[iStub] = true;
              restarted = true;
            } else {
              delete newGen;
            }
          }

          if (!restarted) {
            // 失敗時のみ従来処理（AudioFileSource再生成）
            playAudio(iStub, _volume[iStub], true);
          }
        }
      } else {
        if (_wav_gen[iStub]->isRunning()) {
          if (!_wav_gen[iStub]->loop()) {
            stopAudio(iStub);
          }
        }
      }
    }
  }
}

// _stub 0 = 常用、stub 1 = 左右分ける場合の右、stub 2 = ループ用
void initAudioOut(int I2S_BCLK_PIN, int I2S_LRCK_PIN, int I2S_DOUT_PIN) {
  // init audio
  _i2s_out = new AudioOutputI2S();
  _i2s_out->SetPinout(I2S_BCLK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN);
  _i2s_out->SetGain(1.0);  // 出力ゲインを設定
  for (int i = 0; i < STUB_NUM; i++) {
    _wav_gen[i] = new AudioGeneratorWAV();
  }
  // _mixer に1回渡したら、あとは _stub => _mixer の順で色々渡す
  _mixer = new AudioOutputMixer(32, _i2s_out);

  for (int i = 0; i < STUB_NUM; i++) {
    _stub[i] = _mixer->NewInput();
    _stub[i]->SetRate(SAMPLING_RATE);
  }
}

// get
uint8_t getGain() { return _gainNum; }
uint8_t getCategoryID() { return _settings.categoryNum; }
uint8_t getChannelID() { return _settings.channelId; }
uint8_t getDevicePos() { return _devicePos; }
bool getIsFixMode() { return _settings.isFixMode; }
bool getIsLimitEnable() { return _settings.isLimitEnable; }
bool getIsPlaying() {
  for (int i = 0; i < STUB_NUM; i++) {
    if (isPlayAudio[i]) {
      return true;  // 1つでもtrueがあればtrueを返す
    }
  }
  return false;  // 全てfalseの場合はfalseを返す
}

//  set
// stubNum: 0 = oneshot, 2 = loop
void setDataID(uint8_t stubNum, uint8_t dataID, uint8_t subID) {
  _dataID[stubNum] = dataID;
  _subID[stubNum] = subID;
}

void setCategoryID(uint8_t value) {
  _settings.categoryNum = value;
  EEPROM.put(offsetof(ConfigData, categoryNum), _settings.categoryNum);
  EEPROM.commit();
}
void setChannelID(uint8_t value) {
  _settings.channelId = value;
  EEPROM.put(offsetof(ConfigData, channelId), _settings.channelId);
  EEPROM.commit();
};
void setGain(uint8_t val, uint8_t G_SEL_A = 99, uint8_t G_SEL_B = 99) {
  _gainNum = val;
  DEBUG_PRINTF("_gainNum: ");
  DEBUG_PRINTLN(val);
}
void setDevicePos(uint8_t value) { _devicePos = value; };
void setIsFixMode(bool value) {
  _settings.isFixMode = value;
  EEPROM.put(offsetof(ConfigData, isFixMode), _settings.isFixMode);
  EEPROM.commit();
};
// 有効/無効に切り替えるIDを設定
void setLimitIds(const int limitIDs[], size_t size) {
  _numLimitIDs = (size > 10) ? 10 : size;  // 最大数を超えないように制限
  for (size_t i = 0; i < _numLimitIDs; ++i) {
    _limitIDs[i] = limitIDs[i];
  }
}
void setIsLimitEnable(bool value) {
  _settings.isLimitEnable = value;
  EEPROM.put(offsetof(ConfigData, isLimitEnable), _settings.isLimitEnable);
  EEPROM.commit();
};

void setMessageData(const char *msg, uint8_t id) {
  messages.push_back(
      MessageData(id, msg));  // MessageData のコンストラクタを明示的に呼び出す
}
void setStatusCallback(void (*statusCb)(const char *)) {
  statusCallback = statusCb;
}

// CATEGORY_ID_TXT_SIZEをセット
void setCategorySize(uint8_t size) { _categorySize = size; }

// ボリュームレベルをEEPROMに保存
void saveVolumeLevels(uint8_t *newVolumeLevels, uint8_t size) {
  for (uint8_t i = 0; i < size && i < CATEGORY_NUM; i++) {
    _settings.volumeLevels[i] = newVolumeLevels[i];
  }
  EEPROM.put(0, _settings);  // ConfigData全体を保存
  EEPROM.commit();
}

// ボリュームレベルをEEPROMから読み込み
void loadVolumeLevels(uint8_t *destVolumeLevels, uint8_t size) {
  for (uint8_t i = 0; i < size && i < CATEGORY_NUM; i++) {
    destVolumeLevels[i] = _settings.volumeLevels[i];
  }
}

}  // namespace audioManager
