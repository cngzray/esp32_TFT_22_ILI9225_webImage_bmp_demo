#include "SPI.h"
#include "TFT_22_ILI9225.h"
#include "WiFi.h"
#include "WiFiClient.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"

// ESP32引脚定义
#define TFT_RST 26
#define TFT_RS  25
#define TFT_CLK 18  // VSPI-SCK
#define TFT_SDI 23  // VSPI-MOSI
#define TFT_CS  5   // VSPI-SS
#define TFT_LED 4   // GPIO4 for backlight

SPIClass vspi(VSPI);
TFT_22_ILI9225 tft = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, 255);


const char* ssid = "xxx";
const char* password = "xxx";
const char* imageUrl = "http://10.20.2.27:3000/random-bmp";
//const char* imageUrl = "https://pub-1251799822.file.myqcloud.com/tft_pic004.bmp";

// BMP文件头结构体
#pragma pack(push, 1)
struct BMPFileHeader {
  uint16_t bfType;      // 文件类型标识 (0x4D42 = "BM")
  uint32_t bfSize;      // 文件大小
  uint16_t bfReserved1; // 保留
  uint16_t bfReserved2; // 保留
  uint32_t bfOffBits;   // 位图数据起始位置
};

struct BMPInfoHeader {
  uint32_t biSize;         // 信息头大小
  int32_t  biWidth;        // 图片宽度
  int32_t  biHeight;       // 图片高度
  uint16_t biPlanes;       // 平面数（必须为1）
  uint16_t biBitCount;     // 每像素位数
  uint32_t biCompression;  // 压缩方式
  uint32_t biSizeImage;    // 位图数据大小
  int32_t  biXPelsPerMeter;// 水平分辨率
  int32_t  biYPelsPerMeter;// 垂直分辨率
  uint32_t biClrUsed;      // 使用的颜色数
  uint32_t biClrImportant; // 重要颜色数
};

// BMP颜色掩码（用于16位格式判断）
struct BMPColorMasks {
  uint32_t rMask;          // 红色掩码
  uint32_t gMask;          // 绿色掩码
  uint32_t bMask;          // 蓝色掩码
};
#pragma pack(pop)

// BMP压缩类型常量
#define BI_RGB        0   // 无压缩
#define BI_BITFIELDS  3   // 使用位掩码

// 函数原型声明
bool downloadBMP(const char* url, uint8_t** bmpBuffer, size_t* bmpSize, uint16_t* width, uint16_t* height);
bool drawBMPToScreen(const uint8_t* bmpBuffer, size_t bmpSize, uint16_t width, uint16_t height);
bool isHttpsUrl(const char* url);
void updateDisplay();

// 判断URL是否为HTTPS
bool isHttpsUrl(const char* url) {
  if (url == nullptr) return false;
  const char* httpsPrefix = "https://";
  return strncmp(url, httpsPrefix, 8) == 0;
}

// 下载BMP图片
bool downloadBMP(const char* url, uint8_t** bmpBuffer, size_t* bmpSize, uint16_t* width, uint16_t* height) {
  HTTPClient http;
  WiFiClient httpClient;
  WiFiClientSecure httpsClient;
  
  // 初始化输出参数
  *bmpBuffer = nullptr;
  *bmpSize = 0;
  *width = 0;
  *height = 0;
  
  Serial.println("\nDownloading BMP image...");
  Serial.printf("URL: %s\n", url);
  
  bool useHttps = isHttpsUrl(url);
  Serial.printf("Protocol: %s\n", useHttps ? "HTTPS" : "HTTP");
  
  bool success = false;
  
  if (useHttps) {
    // HTTPS方式：使用WiFiClientSecure
    httpsClient.setInsecure();
    if (http.begin(httpsClient, url)) {
      success = true;
    }
  } else {
    // HTTP方式：使用普通WiFiClient
    if (http.begin(httpClient, url)) {
      success = true;
    }
  }
  
  if (success) {
    // 添加禁用缓存的Header，确保每次都获取最新图片
    http.addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    http.addHeader("Pragma", "no-cache");
    http.addHeader("Expires", "0");
    
    int httpCode = http.GET();
    Serial.printf("HTTP code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      size_t size = http.getSize();
      Serial.printf("BMP size: %d bytes\n", size);
      
      if (size == 0 || size > 200000) {
        size = 150000;
        Serial.printf("Using default buffer: %d\n", size);
      }
      
      *bmpBuffer = (uint8_t*)malloc(size + 1);
      if (*bmpBuffer) {
        memset(*bmpBuffer, 0, size + 1);
        WiFiClient* stream = http.getStreamPtr();
        *bmpSize = stream->readBytes(*bmpBuffer, size);
        Serial.printf("Bytes read: %d\n", *bmpSize);
        
        // 解析BMP头
        if (*bmpSize >= sizeof(BMPFileHeader) + sizeof(BMPInfoHeader)) {
          BMPFileHeader* fh = (BMPFileHeader*)*bmpBuffer;
          BMPInfoHeader* ih = (BMPInfoHeader*)((*bmpBuffer) + sizeof(BMPFileHeader));
          
          if (fh->bfType == 0x4D42) {
            *width = (uint16_t)abs(ih->biWidth);
            *height = (uint16_t)abs(ih->biHeight);
            Serial.printf("BMP OK: %dx%d, %d bpp\n", *width, *height, ih->biBitCount);
          } else {
            Serial.println("[Warning] Invalid BMP header");
          }
        }
        
        http.end();
        return true;
      }
      Serial.println("Memory allocation failed");
    } else {
      Serial.printf("HTTP failed: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("Cannot connect");
  }
  return false;
}



// 直接将BMP绘制到屏幕（逐行绘制，节省内存）
bool drawBMPToScreen(const uint8_t* bmpBuffer, size_t bmpSize, uint16_t width, uint16_t height) {
  BMPFileHeader* fh = (BMPFileHeader*)bmpBuffer;
  BMPInfoHeader* ih = (BMPInfoHeader*)(bmpBuffer + sizeof(BMPFileHeader));
  
  // 验证BMP文件头
  if (fh->bfType != 0x4D42) {
    Serial.println("[Error] Not a valid BMP file");
    return false;
  }
  
  size_t dataOffset = fh->bfOffBits;
  const uint8_t* bmpData = bmpBuffer + dataOffset;
  
  Serial.printf("[Info] Drawing BMP: %dx%d, %d bpp\n", width, height, ih->biBitCount);
  
  // 分配单行缓冲区
  uint16_t* rowBuffer = (uint16_t*)malloc(width * 2);
  if (!rowBuffer) {
    Serial.println("[Error] Failed to allocate row buffer");
    return false;
  }
  
  // 根据位深度处理
  switch (ih->biBitCount) {
    case 16: {
      // 判断16位BMP格式是RGB555还是RGB565
      bool isRGB565 = false;
      
      // 检查压缩方式和颜色掩码
      if (ih->biCompression == BI_BITFIELDS) {
        // BI_BITFIELDS模式：读取颜色掩码来判断
        BMPColorMasks* masks = (BMPColorMasks*)(bmpBuffer + sizeof(BMPFileHeader) + sizeof(BMPInfoHeader));
        // RGB565的典型掩码: R=0xF800, G=0x07E0, B=0x001F
        // RGB555的典型掩码: R=0x7C00, G=0x03E0, B=0x001F
        if (masks->rMask == 0xF800 && masks->gMask == 0x07E0 && masks->bMask == 0x001F) {
          isRGB565 = true;
          Serial.println("[Info] Detected RGB565 format");
        } else if (masks->rMask == 0x7C00 && masks->gMask == 0x03E0 && masks->bMask == 0x001F) {
          Serial.println("[Info] Detected RGB555 format");
        } else {
          // 无法通过掩码确定，尝试通过数据分析
          Serial.println("[Info] Unrecognized bitmask, analyzing pixel data...");
        }
      } else {
        // BI_RGB模式：通常是RGB555，但有些软件可能存储为RGB565
        Serial.println("[Info] BI_RGB compression, assuming RGB555 format");
      }
      
      size_t rowSize = ((width * 2 + 3) / 4) * 4;
      for (int y = height - 1; y >= 0; y--) {
        const uint8_t* rowData = bmpData + y * rowSize;
        for (int x = 0; x < width; x++) {
          // BMP小端序：低字节在前，高字节在后
          uint16_t pixel = rowData[x * 2] | (rowData[x * 2 + 1] << 8);
          uint16_t rgb565;
          
          if (isRGB565) {
            // 已经是RGB565格式：RRRRRGGGGGGBBBBB
            // 直接使用，无需转换
            rgb565 = pixel;
          } else {
            // RGB555格式：XRRRRRGGGGGGBBBBB (位15是填充位)
            // 提取RGB555分量
            uint8_t r5 = (pixel >> 10) & 0x1F;  // 位14-10
            uint8_t g5 = (pixel >> 5) & 0x1F;   // 位9-5
            uint8_t b5 = pixel & 0x1F;          // 位4-0
            
            // 转换为RGB565格式：RRRRRGGGGGGBBBBB
            // 绿色需要从5位扩展到6位
            uint16_t r565 = r5;
            uint16_t g565 = (g5 << 1) | (g5 >> 4); // 扩展为6位
            uint16_t b565 = b5;
            rgb565 = (r565 << 11) | (g565 << 5) | b565;
          }
          rowBuffer[x] = rgb565;
        }
        // 左右镜像翻转：反转行缓冲区
        for (int i = 0; i < width / 2; i++) {
          uint16_t temp = rowBuffer[i];
          rowBuffer[i] = rowBuffer[width - 1 - i];
          rowBuffer[width - 1 - i] = temp;
        }
        tft.drawBitmap(0, y, rowBuffer, width, 1);
      }
      break;
    }
    
    case 24: {
      size_t rowSize = ((width * 3 + 3) / 4) * 4;
      for (int y = height - 1; y >= 0; y--) {
        const uint8_t* rowData = bmpData + y * rowSize;
        for (int x = 0; x < width; x++) {
          uint8_t b = rowData[x * 3];
          uint8_t g = rowData[x * 3 + 1];
          uint8_t r = rowData[x * 3 + 2];
          rowBuffer[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
        // 左右镜像翻转：反转行缓冲区
        for (int i = 0; i < width / 2; i++) {
          uint16_t temp = rowBuffer[i];
          rowBuffer[i] = rowBuffer[width - 1 - i];
          rowBuffer[width - 1 - i] = temp;
        }
        tft.drawBitmap(0, y, rowBuffer, width, 1);
      }
      break;
    }
    
    default: {
      Serial.printf("[Error] Unsupported: %d bpp\n", ih->biBitCount);
      free(rowBuffer);
      return false;
    }
  }
  
  free(rowBuffer);
  Serial.println("[Info] BMP drawn to screen!");
  return true;
}


void setup() {
  // 初始化串口
  Serial.begin(115200);
  
  // 初始化背光引脚（GPIO4，避免与内置LED冲突）
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH); // 直接开启背光
  
  // 初始化VSPI
  vspi.begin(TFT_CLK, -1, TFT_SDI, TFT_CS); // SCK, MISO, MOSI, SS
  
  // 初始化TFT
  tft.begin(vspi);
  
  // 设置显示方向（旋转180度）
  tft.setOrientation(2);
  
  // 设置字体（用于显示错误信息）
  tft.setFont(Terminal11x16);
  
  // 清除屏幕
  tft.clear();
  
  // 连接WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// 更新显示：下载并显示最新图片
void updateDisplay() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  
  // 每隔10秒更新一次
  if (now - lastUpdate >= 10000) {
    lastUpdate = now;
    
    Serial.println("\n========== Updating display ==========");
    
    // 检查WiFi连接状态
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Warning] WiFi not connected, attempting to reconnect...");
      
      // 尝试重新连接WiFi
      WiFi.reconnect();
      
      // 等待连接（最多等待5秒）
      unsigned long reconnectStart = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - reconnectStart < 5000) {
        delay(500);
        Serial.print(".");
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi reconnected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("");
        Serial.println("[Error] WiFi reconnection failed");
        tft.clear();
        tft.drawText(10, 10, "WiFi disconnected", 0xFFFF);
        return; // 退出函数，等待下次尝试
      }
    }
    
    // WiFi已连接，开始下载BMP图片
    uint8_t* bmpBuffer = nullptr;
    size_t bmpSize = 0;
    uint16_t bmpWidth = 0, bmpHeight = 0;
    
    if (downloadBMP(imageUrl, &bmpBuffer, &bmpSize, &bmpWidth, &bmpHeight)) {
      Serial.printf("BMP downloaded: %dx%d, %d bytes\n", bmpWidth, bmpHeight, bmpSize);
      
      // 清除屏幕
      tft.clear();
      
      // 直接绘制BMP到屏幕（逐行绘制，节省内存）
      if (bmpWidth == 176 && bmpHeight == 220) {
        // 图片尺寸与屏幕相同
        if (drawBMPToScreen(bmpBuffer, bmpSize, bmpWidth, bmpHeight)) {
          Serial.println("Image displayed on TFT!");
        } else {
          tft.drawText(10, 10, "Draw failed", 0xFFFF);
        }
      } else {
        Serial.printf("[Error] Image size mismatch: %dx%d, expected 176x220\n", bmpWidth, bmpHeight);
        tft.drawText(10, 10, "Size mismatch", 0xFFFF);
      }
      
      free(bmpBuffer);
    } else {
      Serial.println("BMP download failed");
      tft.clear();
      tft.drawText(10, 10, "Download failed", 0xFFFF);
    }
  }
}


void loop() {
  // 持续更新显示（每隔10秒刷新一次）
  updateDisplay();
  delay(100);
}
