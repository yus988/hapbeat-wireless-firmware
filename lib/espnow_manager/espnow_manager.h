#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include <stdint.h>
#include <esp_now.h>

namespace espnowManager {
void init_esp_now(esp_now_recv_cb_t callback);

// WiFi チャンネル選択 (NVS 永続化)
// 非重複3チャンネル (1, 6, 11) を順送り
uint8_t getChannel();           // 現在チャンネルを返す
void cycleChannel();            // 次のチャンネルに切り替え (NVS保存 + 即時適用)
void setChannel(uint8_t ch);    // 指定チャンネルに切り替え
}  // namespace espnowManager

#endif
