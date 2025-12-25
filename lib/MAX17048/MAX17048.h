/******************************************************************************
MAX17048.h
MAX17048 Arduino Library Header File
燃料ゲージIC MAX17048/MAX17049用ライブラリ

BQ27220と互換性のあるインターフェースを提供
******************************************************************************/

#ifndef MAX17048_H
#define MAX17048_H

#include "Arduino.h"
#include <Wire.h>

// MAX17048 I2Cアドレス
#define MAX17048_I2C_ADDRESS 0x36

// MAX17048 レジスタアドレス
#define MAX17048_VCELL    0x02  // 電圧
#define MAX17048_SOC      0x04  // 残量(%)
#define MAX17048_MODE     0x06  // モード
#define MAX17048_VERSION  0x08  // バージョン
#define MAX17048_HIBRT    0x0A  // ハイバネートしきい値
#define MAX17048_CONFIG   0x0C  // 設定
#define MAX17048_VALRT    0x14  // 電圧アラートしきい値
#define MAX17048_CRATE    0x16  // 充放電レート
#define MAX17048_VRESET   0x18  // リセット電圧
#define MAX17048_STATUS   0x1A  // ステータス
#define MAX17048_CMD      0xFE  // コマンド

class MAX17048 {
public:
  MAX17048();
  
  /**
   * I2C初期化とMAX17048との通信確認
   * @return 通信成功でtrue
   */
  bool begin(void);
  
  /**
   * バッテリー電圧を読み取る
   * @return 電圧値(mV)
   */
  uint16_t voltage(void);
  
  /**
   * バッテリー残量(SOC)を読み取る
   * @return 残量(%)
   */
  uint16_t soc(void);
  
  /**
   * 充放電レートを読み取る
   * @return レート(%/h)
   */
  float chargeRate(void);
  
  /**
   * ICバージョンを読み取る
   * @return バージョンID
   */
  uint16_t version(void);
  
  /**
   * ソフトリセットを実行
   */
  void reset(void);
  
  /**
   * クイックスタートを実行（SOC再計算）
   */
  void quickStart(void);

private:
  uint8_t _deviceAddress;
  
  /**
   * 16ビット値をレジスタから読み取る
   */
  uint16_t readWord(uint8_t reg);
  
  /**
   * 16ビット値をレジスタに書き込む
   */
  void writeWord(uint8_t reg, uint16_t value);
};

namespace MAX17048_Cmd {
void setupMAX17048(uint8_t PIN_SDA, uint8_t PIN_SCL);
void printBatteryStats();
}  // namespace MAX17048_Cmd

// BQ27220互換のグローバル変数
extern MAX17048 lipo;

#endif

