#include "SPI.h"
#include "TFT_22_ILI9225.h"
#include "WiFi.h"
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
const char* imageUrl = "https://xxx.file.myqcloud.com/tft_pic001.bmp";

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
#pragma pack(pop)

// 函数原型声明
bool downloadBMP(const char* url, uint8_t** bmpBuffer, size_t* bmpSize, uint16_t* width, uint16_t* height);
bool drawBMPToScreen(const uint8_t* bmpBuffer, size_t bmpSize, uint16_t width, uint16_t height);

// 下载BMP图片
bool downloadBMP(const char* url, uint8_t** bmpBuffer, size_t* bmpSize, uint16_t* width, uint16_t* height) {
  HTTPClient http;
  WiFiClientSecure client;
  
  // 初始化输出参数
  *bmpBuffer = nullptr;
  *bmpSize = 0;
  *width = 0;
  *height = 0;
  
  // 禁用SSL证书验证
  client.setInsecure();
  
  Serial.println("\nDownloading BMP image...");
  Serial.printf("URL: %s\n", url);
  
  if (http.begin(client, url)) {
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
      size_t rowSize = ((width * 2 + 3) / 4) * 4;
      for (int y = height - 1; y >= 0; y--) {
        const uint8_t* rowData = bmpData + y * rowSize;
        for (int x = 0; x < width; x++) {
          // BMP的16位格式通常是RGB555（小端序）
          // 布局: [高字节][低字节] -> 位15-0: X RRRRR GGGGG BBBBB
          uint16_t pixel = rowData[x * 2] | (rowData[x * 2 + 1] << 8);
          
          // 提取RGB555分量
          uint8_t r5 = (pixel >> 10) & 0x1F;  // 位14-10
          uint8_t g5 = (pixel >> 5) & 0x1F;   // 位9-5
          uint8_t b5 = pixel & 0x1F;          // 位4-0
          
          // 转换为RGB565格式
          // RGB565布局: RRRRRGGGGGGBBBBB
          uint16_t rgb565 = (r5 << 11) | ((g5 << 1) << 5) | b5;
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
  
  // 下载BMP图片
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


void loop() {
  // 图片已显示，进入空闲循环
  delay(100);
}