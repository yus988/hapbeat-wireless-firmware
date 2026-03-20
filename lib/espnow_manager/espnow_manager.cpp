#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace espnowManager {
esp_now_peer_info_t slave;

void init_esp_now(esp_now_recv_cb_t callback) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    USBSerial.println("ESPNow Init Success");
  } else {
    USBSerial.println("ESPNow Init Failed");
    ESP.restart();
  }
  // 音声ストリーミングは低遅延優先で LR を使わない
#if defined(AUDIO_STREAM_MODE)
  esp_wifi_set_protocol(WIFI_IF_STA,
                        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                            WIFI_PROTOCOL_11N);
  // 高速レートを明示指定（自動選択だと低速に落ちる可能性）
  esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_MCS7_SGI);
#else
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
#endif

  esp_wifi_set_ps(WIFI_PS_NONE);  // ★ Wi-Fiスリープ無効化

  int wifi_ch = 1;
  esp_wifi_set_channel(wifi_ch, WIFI_SECOND_CHAN_NONE);  // ★ チャネル1に固定

  // マルチキャスト用Slave登録
  memset(&slave, 0, sizeof(slave));
  for (int i = 0; i < 6; ++i) {
    slave.peer_addr[i] = (uint8_t)0xff;
  }
  slave.channel = wifi_ch;
  slave.encrypt = false;
  esp_err_t addStatus = esp_now_add_peer(&slave);
  if (addStatus == ESP_OK) {  // Pair success
    USBSerial.println("Pair success");
  }

  // ESP-NOWコールバック登録
  esp_now_register_recv_cb(callback);
}
}  // namespace espnowManager