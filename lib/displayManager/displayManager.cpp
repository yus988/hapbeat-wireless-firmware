#include <Adafruit_SSD1306.h>
#include <efont.h>
#include <efontEnableJaMini.h>

namespace displayManager {

// 表示位置を保持する変数
int _playCategoryPos[2] = {0, 0};
int _channelIdPos[2] = {0, 0};
int _gainStepPos[2] = {0, 0};
int _textsize = 0;

// メニューやテキストを保持する配列
const char *_menuTxt[99];
const char *_wearerTxt[99];
const char *_decibelTxt[99];

// 位置情報をセットする
void setPositions(const int playPos[2], const int channelPos[2],
                  const int gainPos[2]) {
  _playCategoryPos[0] = playPos[0];
  _playCategoryPos[1] = playPos[1];
  _channelIdPos[0] = channelPos[0];
  _channelIdPos[1] = channelPos[1];
  _gainStepPos[0] = gainPos[0];
  _gainStepPos[1] = gainPos[1];
}

// 日本語テキストのセットアップ
void setTitle(const char **category, int m_size, const char **channel,
              int w_size, const char **gainstep, int d_size) {
  for (int i = 0; i < m_size; i++) {
    _menuTxt[i] = category[i];
  }
  for (int i = 0; i < w_size; i++) {
    _wearerTxt[i] = channel[i];
  }
  for (int i = 0; i < d_size; i++) {
    _decibelTxt[i] = gainstep[i];
  }
}

// OLEDの初期化と位置情報のセット
void initOLED(Adafruit_SSD1306 *display, int rot, const int textsize,
              const int playPos[2], const int channelPos[2],
              const int gainPos[2]) {
#ifdef OLED_I2C
  // I2C接続の場合（V4など）: アドレス0x3Cを指定
  if (!(*display).begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    USBSerial.println(F("SSD1306 I2C allocation failed"));
    for (;;);  // Don't proceed, loop forever
  }
#else
  // SPI接続の場合（V3以前）
  if (!(*display).begin(SSD1306_SWITCHCAPVCC)) {
    USBSerial.println(F("SSD1306 SPI allocation failed"));
    for (;;);  // Don't proceed, loop forever
  }
#endif
  (*display).clearDisplay();
  (*display).setRotation(rot);
  (*display).display();
  _textsize = textsize;
  // 位置情報をセット
  setPositions(playPos, channelPos, gainPos);
}

// 日本語描画
void printEfont(Adafruit_SSD1306 *display, const char *str, int posX,
                int posY) {
  uint32_t textcolor = 1;    // display->textcolor;
  uint32_t textbgcolor = 0;  // display->textbgcolor;
  byte font[32];
  while (*str != 0x00) {
    // 改行処理
    if (*str == '\n') {
      // 改行
      posY += 16 * _textsize;
      posX = display->getCursorX();
      str++;
      continue;
    }
    // フォント取得
    uint16_t strUTF16;
    str = efontUFT8toUTF16(&strUTF16, const_cast<char *>(str));
    getefontData(font, strUTF16);
    int width = 16 * _textsize;
    if (strUTF16 < 0x0100) {
      // 半角
      width = 8 * _textsize;
    }
    // 背景塗りつぶし
    display->fillRect(posX, posY, width, 16 * _textsize, textbgcolor);
    for (uint8_t row = 0; row < 16; row++) {
      word fontdata = font[row * 2] * 256 + font[row * 2 + 1];
      for (uint8_t col = 0; col < 16; col++) {
        if ((0x8000 >> col) & fontdata) {
          int drawX = posX + col * _textsize;
          int drawY = posY + row * _textsize;
          if (_textsize == 1) {
            display->drawPixel(drawX, drawY, textcolor);
          } else {
            display->fillRect(drawX, drawY, _textsize, _textsize, textcolor);
          }
        }
      }
    }
    posX += width;
    if (display->width() <= posX) {
      posX = 0;
      posY += 16 * _textsize;
    }
  }
  (*display).display();
}

// OLEDの更新
void updateOLED(Adafruit_SSD1306 *display, uint8_t categoryNum,
                uint8_t channelNum, uint8_t gainStepNum) {
  USBSerial.printf("categoryNum: %d, channelNum: %d, gainStepNum: %d\n",
                   categoryNum, channelNum, gainStepNum);
  (*display).clearDisplay();

  // カテゴリの更新
  printEfont(display, _menuTxt[categoryNum], _playCategoryPos[0],
             _playCategoryPos[1]);

  // チャンネルの変更
  printEfont(display, _wearerTxt[channelNum], _channelIdPos[0],
             _channelIdPos[1]);

  // ボリュームの更新
  printEfont(display, _decibelTxt[gainStepNum], _gainStepPos[0],
             _gainStepPos[1]);
}
}  // namespace displayManager
// 5段バッテリーメータ（汎用）
namespace displayManager {
void drawBatteryMeter5(Adafruit_SSD1306 *display, int x, int y, int barW, int barH,
                      int gap, int levels, bool blinkOn, bool blinkOne,
                      bool withLabel) {
  int labelX = x - 32 < 0 ? 0 : x - 32;
  int barsY = y;
  if (withLabel) {
    // ラベルはyを上端として16px高さで描画。バーの下辺をラベルの下辺に合わせる
    printEfont(display, "BAT", labelX, y);
    barsY = y + 16 - barH - 1;  // 最小単位で1px上に調整
  }
  for (int i = 0; i < 5; i++) {
    int bx = x + i * (barW + gap);
    // 中空時の枠は2px程度残す
    display->drawRect(bx, barsY, barW, barH, 1);
    display->drawRect(bx+1, barsY+1, barW-2, barH-2, 1);
    if (i < levels) {
      if (blinkOne && i == 0 && !blinkOn) continue;
      display->fillRect(bx + 2, barsY + 2, barW - 4, barH - 4, 1);
    }
  }
}
}  // namespace displayManager
