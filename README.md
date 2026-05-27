esp32 wroom连接2.2寸TFT屏幕演示

驱动是 ILI9225

连接wifi，并从网上下载demo图片显示

图片：bmp, 176x220 , 16bit

// ESP32引脚定义

#define TFT_RST 26

#define TFT_RS  25

#define TFT_CLK 18  // VSPI-SCK

#define TFT_SDI 23  // VSPI-MOSI

#define TFT_CS  5   // VSPI-SS

#define TFT_LED 4   // GPIO4 for backlight

<img width="300" alt="image" src="https://github.com/user-attachments/assets/ee929fc8-73ef-4362-9da8-568547092f64" />

补充： 2.2inch Arduino SPI Module ILI9225 SKU:MAR2201 官方wiki链接 https://www.lcdwiki.com/zh/2.2inch_Arduino_SPI_Module_ILI9225_SKU:MAR2201
