/******************************************************************************
MAX17048.cpp
MAX17048 Arduino Library Source File
燃料ゲージIC MAX17048/MAX17049用ライブラリ

BQ27220と互換性のあるインターフェースを提供
******************************************************************************/

#include "MAX17048.h"

namespace MAX17048_Cmd {
uint8_t _PIN_SDA;
uint8_t _PIN_SCL;

void setupMAX17048(uint8_t PIN_SDA, uint8_t PIN_SCL) {
  _PIN_SDA = PIN_SDA;
  _PIN_SCL = PIN_SCL;
  
  if (!lipo.begin()) {
    USBSerial.println("Error: MAX17048 init failed!");
    USBSerial.println("  Check I2C connection.");
  } else {
    USBSerial.println("Connected to MAX17048!");
  }
}

void printBatteryStats() {
  unsigned int socPct = lipo.soc();
  unsigned int volts = lipo.voltage();
  float rate = lipo.chargeRate();
  
  USBSerial.print("Voltage: ");
  USBSerial.print(volts);
  USBSerial.println(" mV");
  
  USBSerial.print("SOC: ");
  USBSerial.print(socPct);
  USBSerial.println(" %");
  
  USBSerial.print("Charge Rate: ");
  USBSerial.print(rate);
  USBSerial.println(" %/hr");
}
}  // namespace MAX17048_Cmd

// グローバルインスタンス
MAX17048 lipo;

// コンストラクタ
MAX17048::MAX17048() : _deviceAddress(MAX17048_I2C_ADDRESS) {}

// 初期化
bool MAX17048::begin(void) {
  // バージョンレジスタを読んで通信確認
  uint16_t ver = version();
  // MAX17048は通常 0x0011 または 0x0012 を返す
  if (ver == 0x0000 || ver == 0xFFFF) {
    return false;
  }
  return true;
}

// 電圧取得 (mV)
uint16_t MAX17048::voltage(void) {
  uint16_t raw = readWord(MAX17048_VCELL);
  // VCELLレジスタ: 12ビット値、78.125μV/LSB
  // 値は上位12ビットに格納、下位4ビットは未使用
  // 電圧(V) = raw * 78.125μV = raw * 0.000078125
  // 電圧(mV) = raw * 0.078125
  // 簡略化: raw >> 4 すると12ビット値、それに1.25mVを掛ける
  // または raw * 78.125 / 1000 で mV
  float voltage_mV = (float)raw * 78.125f / 1000.0f;
  return (uint16_t)voltage_mV;
}

// SOC取得 (%)
uint16_t MAX17048::soc(void) {
  uint16_t raw = readWord(MAX17048_SOC);
  // SOCレジスタ: 上位バイト=整数部(%)、下位バイト=小数部(1/256 %)
  uint8_t socInt = raw >> 8;  // 整数部分
  return (uint16_t)socInt;
}

// 充放電レート取得 (%/h)
float MAX17048::chargeRate(void) {
  uint16_t raw = readWord(MAX17048_CRATE);
  // CRATEレジスタ: 符号付き16ビット、0.208%/hr 単位
  int16_t signedRate = (int16_t)raw;
  return (float)signedRate * 0.208f;
}

// バージョン取得
uint16_t MAX17048::version(void) {
  return readWord(MAX17048_VERSION);
}

// ソフトリセット
void MAX17048::reset(void) {
  // リセットコマンド: 0x5400 をCMDレジスタに書き込む
  writeWord(MAX17048_CMD, 0x5400);
}

// クイックスタート
void MAX17048::quickStart(void) {
  // クイックスタート: MODEレジスタのビット14をセット
  uint16_t mode = readWord(MAX17048_MODE);
  mode |= 0x4000;
  writeWord(MAX17048_MODE, mode);
}

// 16ビット値読み取り
uint16_t MAX17048::readWord(uint8_t reg) {
  Wire.beginTransmission(_deviceAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return 0;
  }
  
  Wire.requestFrom(_deviceAddress, (uint8_t)2);
  if (Wire.available() >= 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    return ((uint16_t)msb << 8) | lsb;
  }
  return 0;
}

// 16ビット値書き込み
void MAX17048::writeWord(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(_deviceAddress);
  Wire.write(reg);
  Wire.write((uint8_t)(value >> 8));   // MSB first
  Wire.write((uint8_t)(value & 0xFF)); // LSB
  Wire.endTransmission();
}

