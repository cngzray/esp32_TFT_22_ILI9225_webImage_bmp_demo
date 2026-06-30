# ESP32_TFT_22_ILI9225_webImage_bmp_demo
ESP32 + 2.2寸 ILI9225 SPI TFT 在线加载网络BMP图片示例
无需SD卡，WiFi HTTPS下载BMP并实时渲染到屏幕，低内存逐行绘制方案

## 项目简介
本Demo实现ESP32通过WiFi下载云端BMP位图，直接驱动2.2寸 ILI9225（176*220）显示屏显示图片：
- 使用硬件VSPI驱动TFT屏幕，刷新率稳定
- HTTPS无证书校验，支持腾讯云/阿里云对象存储直链BMP
- 轻量化单行缓冲区渲染，大幅降低内存占用
- 支持 16bit RGB555 / 24bit RGB888 标准BMP图片
- 自动修正BMP上下倒置、左右镜像问题
- 串口+屏幕双端错误提示（下载失败/尺寸不匹配/不支持位深）

<img width="300" alt="image" src="https://github.com/user-attachments/assets/ee929fc8-73ef-4362-9da8-568547092f64" />

## 硬件清单
1. ESP32开发板
2. 2.2寸 SPI ILI9225 TFT LCD（分辨率 176 × 220）
3. 杜邦线若干
4. 2.2inch Arduino SPI Module ILI9225 SKU:MAR2201 官方wiki链接 https://www.lcdwiki.com/zh/2.2inch_Arduino_SPI_Module_ILI9225_SKU:MAR2201

## 硬件接线 VSPI
| TFT 引脚 | ESP32 GPIO | 功能说明 |
|---------|------------|----------|
| RST     | 26         | 屏幕复位 |
| RS/DC   | 25         | 命令/数据引脚 |
| CLK     | 18         | VSPI时钟 SCK |
| SDI/MOSI| 23         | VSPI数据 MOSI |
| CS      | 5          | SPI片选 |
| LED     | 4          | 背光控制 |

## 开发环境
- IDE：Arduino IDE / PlatformIO
- 依赖库：
  1. TFT_22_ILI9225（ILI9225专用驱动库）
  2. WiFi（ESP32内置）
  3. HTTPClient（ESP32内置）
  4. WiFiClientSecure（ESP32内置SSL客户端）

## 使用步骤
### 1. 配置参数
打开 `src/main.cpp` 修改以下配置：
```cpp
// WiFi信息
const char* ssid = "你的WiFi名称";
const char* password = "WiFi密码";
// 云端BMP图片直链（https协议）
const char* imageUrl = "https://xxx.file.myqcloud.com/tft_pic001.bmp";


