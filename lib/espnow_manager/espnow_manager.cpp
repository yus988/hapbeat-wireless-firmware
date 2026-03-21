#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

namespace espnowManager {

esp_now_peer_info_t slave;

// --- NVS チャンネル管理 ---
static constexpr uint8_t CHANNEL_LIST[] = {1, 6, 11};
static constexpr uint8_t CHANNEL_COUNT = sizeof(CHANNEL_LIST) / sizeof(CHANNEL_LIST[0]);
static uint8_t currentChannel = 1;
static Preferences prefs;

static uint8_t loadChannelFromNVS() {
  prefs.begin("espnow", true);  // read-only
  uint8_t ch = prefs.getUChar("wifi_ch", 1);
  prefs.end();
  // 有効なチャンネルか確認
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    if (CHANNEL_LIST[i] == ch) return ch;
  }
  return 1;  // デフォルト
}

static void saveChannelToNVS(uint8_t ch) {
  prefs.begin("espnow", false);  // read-write
  prefs.putUChar("wifi_ch", ch);
  prefs.end();
}

uint8_t getChannel() { return currentChannel; }

void setChannel(uint8_t ch) {
  currentChannel = ch;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  // ピア情報も更新
  slave.channel = ch;
  esp_now_del_peer(slave.peer_addr);
  esp_now_add_peer(&slave);
  saveChannelToNVS(ch);
  USBSerial.printf("[ESPNOW] Channel changed to %d\n", ch);
}

void cycleChannel() {
  uint8_t nextIdx = 0;
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    if (CHANNEL_LIST[i] == currentChannel) {
      nextIdx = (i + 1) % CHANNEL_COUNT;
      break;
    }
  }
  setChannel(CHANNEL_LIST[nextIdx]);
}

// --- 初期化 ---

void init_esp_now(esp_now_recv_cb_t callback) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect();

  // 送信側と同じ無線パラメータを設定
  esp_wifi_set_max_tx_power(84);  // 21 dBm (最大出力)

#if defined(AUDIO_STREAM_MODE)
  esp_wifi_set_protocol(WIFI_IF_STA,
                        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                            WIFI_PROTOCOL_11N);
  // 受信感度重視: 6Mbps (OFDM) は 54Mbps より +14dB 感度が高い
  // 49B パケットの空中時間は ~150µs で帯域的に全く問題ない
  esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_6M);
#else
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
#endif

  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  esp_wifi_set_ps(WIFI_PS_NONE);

  // NVS からチャンネルを復元
  currentChannel = loadChannelFromNVS();
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() == ESP_OK) {
    USBSerial.println("ESPNow Init Success");
  } else {
    USBSerial.println("ESPNow Init Failed");
    ESP.restart();
  }

  // ブロードキャストピア登録
  memset(&slave, 0, sizeof(slave));
  for (int i = 0; i < 6; ++i) {
    slave.peer_addr[i] = (uint8_t)0xff;
  }
  slave.channel = currentChannel;
  slave.encrypt = false;
  esp_err_t addStatus = esp_now_add_peer(&slave);
  if (addStatus == ESP_OK) {
    USBSerial.println("Pair success");
  }

  esp_now_register_recv_cb(callback);

  USBSerial.printf("[ESPNOW] CH=%d TX=21dBm PHY=6Mbps BW=HT20\n", currentChannel);
}

}  // namespace espnowManager
