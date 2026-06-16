#include "SPI.h"
#include "TFT_22_ILI9225.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"
#include <math.h>
#include <string.h>

// ESP32引脚定义
#define TFT_RST 26
#define TFT_RS  25
#define TFT_CLK 18  // VSPI-SCK
#define TFT_SDI 23  // VSPI-MOSI
#define TFT_CS  5   // VSPI-SS
#define TFT_LED 4   // GPIO4 for backlight

SPIClass vspi(VSPI);
TFT_22_ILI9225 tft = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, 255);

#define MAX_AIRCRAFT 50
#define MAX_STRING_LEN 100
#define DIST 50
//22.396°, 114.114

const char* ssid = "xxx";
const char* password = "xxx";
String apiUrl = String("https://opendata.adsb.fi/api/v3/lat/23.206/lon/113.2844/dist/") + DIST;
//String apiUrl = String("https://opendata.adsb.fi/api/v3/lat/22.401/lon/114.130/dist/") + DIST;
unsigned long lastRefresh = 0;
const long refreshInterval = 5000;



typedef struct {
  float dst;
  float dir;
  float track;
  int alt_baro;
  char flight[MAX_STRING_LEN];
  bool valid;
} AircraftData;

AircraftData acArray[MAX_AIRCRAFT];
int acCount = 0;

void convertCoordinate(float dst, float dir, int &x, int &y, int centerX = 88, int centerY = 88);
void drawEquilateralTriangle(int x, int y, uint16_t color = COLOR_BLUE, float heading = 0, const char* flight = NULL);
void drawBackgroundFrame();
int parseADSBPayload(const String& payload, AircraftData* acArray, int maxCount);

void fetchADSBData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();

  if (https.begin(client, apiUrl)) {
    int httpCode = https.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      https.end();
      
      size_t payloadLen = payload.length();
      if (payloadLen == 0) {
        Serial.println("HTTP响应为空");
        return;
      }
      
      if (payloadLen > 16384) {
        Serial.printf("响应数据过大: %d bytes\n", payloadLen);
        return;
      }
      
      acCount = parseADSBPayload(payload, acArray, MAX_AIRCRAFT);
      
      if (acCount < 0) {
        Serial.println("Payload解析失败");
        return;
      }
      
      Serial.printf("ADS-B数据获取成功，共 %d 架飞机\n", acCount);
      
      drawBackgroundFrame();

      for (int i = 0; i < acCount; i++) {
        if (acArray[i].valid) {
          float dst = acArray[i].dst;
          float dir = acArray[i].dir;
          float heading = acArray[i].track;
          int alt_baro = acArray[i].alt_baro;
          uint16_t color;
          if (alt_baro <= 2000) {
            color = COLOR_RED;
          } else if (alt_baro <= 6000) {
            color = COLOR_ORANGE;         
          } else if (alt_baro <= 10000) {
            color = COLOR_YELLOW;                 
          } else if (alt_baro <= 15000) {
            color = COLOR_LIGHTGREEN;            
          } else if (alt_baro <= 22000) {
            color = COLOR_GREEN;
          } else if (alt_baro <= 30000) {
            color = COLOR_LIGHTBLUE;    
          } else if (alt_baro <= 35000) {
            color = COLOR_BLUE;                      
          } else {
            color = COLOR_VIOLET;
          }
          if (dst >= 0 && dst <= 50) {
            int x, y;
            convertCoordinate(dst, dir, x, y);
            if (x >= 0 && x < 176 && y >= 0 && y < 220) {
              drawEquilateralTriangle(x, y, color, heading, acArray[i].flight);
            }
          }
        }
      }
      
    } else {
      Serial.printf("HTTP Error Code: %d\n", httpCode);
      https.end();
    }
  } else {
    Serial.println("Failed to connect to API");
  }
}

void convertCoordinate(float dst, float dir, int &x, int &y, int centerX, int centerY) {
  float scale = 80.0 / DIST;
  float angleRad = (90-dir)* PI / 180.0;
  float dx = dst * scale * cos(angleRad);
  float dy = dst * scale * sin(angleRad);
  x = centerX + (int)dx;
  y = centerY - (int)dy;
}

void drawEquilateralTriangle(int x, int y, uint16_t color, float heading, const char* flight) {
  tft.fillCircle(x, y, 3, color);
  
  int lineLength = 10;
  float angleRad = (90-heading) * PI / 180.0;
  int xEnd = x + (int)(lineLength * cos(angleRad));
  int yEnd = y - (int)(lineLength * sin(angleRad));
  tft.drawLine(x, y, xEnd, yEnd, color);
  
  if (flight != NULL && flight[0] != '\0') {
    tft.setFont(Terminal6x8);
    int textWidth = tft.getTextWidth(flight);
    int textX = x - textWidth / 2;
    int textY = y + 6;
    tft.drawText(textX, textY, flight, COLOR_WHITE);
  }
}

float parseFloatField(const String& objStr, const char* fieldName) {
  String fieldPattern = "\"";
  fieldPattern += fieldName;
  fieldPattern += "\"";
  
  int pos = objStr.indexOf(fieldPattern);
  if (pos == -1) {
    fieldPattern = "'";
    fieldPattern += fieldName;
    fieldPattern += "'";
    pos = objStr.indexOf(fieldPattern);
  }
  
  if (pos == -1) {
    String upperField = String(fieldName);
    upperField.toUpperCase();
    String pattern1 = "\"";
    pattern1 += upperField;
    pattern1 += "\"";
    pos = objStr.indexOf(pattern1);
    if (pos == -1) {
      String pattern2 = "'";
      pattern2 += upperField;
      pattern2 += "'";
      pos = objStr.indexOf(pattern2);
    }
  }
  
  if (pos == -1) {
    String lowerField = String(fieldName);
    lowerField.toLowerCase();
    String pattern1 = "\"";
    pattern1 += lowerField;
    pattern1 += "\"";
    pos = objStr.indexOf(pattern1);
    if (pos == -1) {
      String pattern2 = "'";
      pattern2 += lowerField;
      pattern2 += "'";
      pos = objStr.indexOf(pattern2);
    }
  }
  
  if (pos == -1) {
    return NAN;
  }
  
  pos = objStr.indexOf(":", pos);
  if (pos == -1) return NAN;
  
  pos++;
  while (pos < objStr.length() && (objStr[pos] == ' ' || objStr[pos] == '\t')) {
    pos++;
  }
  
  if (pos >= objStr.length()) {
    return NAN;
  }
  
  int endPos;
  char firstChar = objStr[pos];
  
  if (firstChar == '"') {
    endPos = pos + 1;
    while (endPos < objStr.length()) {
      if (objStr[endPos] == '\\') {
        endPos += 2;
        continue;
      }
      if (objStr[endPos] == '"') {
        break;
      }
      endPos++;
    }
    if (endPos >= objStr.length()) return NAN;
    endPos++;
  } else if (firstChar == '\'') {
    endPos = pos + 1;
    while (endPos < objStr.length()) {
      if (objStr[endPos] == '\\') {
        endPos += 2;
        continue;
      }
      if (objStr[endPos] == '\'') {
        break;
      }
      endPos++;
    }
    if (endPos >= objStr.length()) return NAN;
    endPos++;
  } else {
    endPos = objStr.indexOf(',', pos);
    if (endPos == -1) endPos = objStr.indexOf('}', pos);
    if (endPos == -1) endPos = objStr.indexOf(']', pos);
    if (endPos == -1) endPos = objStr.indexOf('\n', pos);
    if (endPos == -1) endPos = objStr.indexOf('\r', pos);
    if (endPos == -1) endPos = objStr.length();
  }
  
  String valStr = objStr.substring(pos, endPos);
  valStr.trim();
  
  if (valStr.startsWith("\"")) {
    valStr = valStr.substring(1, valStr.length() - 1);
  } else if (valStr.startsWith("'")) {
    valStr = valStr.substring(1, valStr.length() - 1);
  }
  
  return valStr.toFloat();
}

void parseStringField(const String& objStr, const char* fieldName, char* result, int maxLen) {
  result[0] = '\0';
  String fieldPattern = "\"";
  fieldPattern += fieldName;
  fieldPattern += "\"";
  
  int pos = objStr.indexOf(fieldPattern);
  if (pos == -1) {
    fieldPattern = "'";
    fieldPattern += fieldName;
    fieldPattern += "'";
    pos = objStr.indexOf(fieldPattern);
  }
  
  if (pos == -1) {
    String upperField = String(fieldName);
    upperField.toUpperCase();
    String pattern1 = "\"";
    pattern1 += upperField;
    pattern1 += "\"";
    pos = objStr.indexOf(pattern1);
    if (pos == -1) {
      String pattern2 = "'";
      pattern2 += upperField;
      pattern2 += "'";
      pos = objStr.indexOf(pattern2);
    }
  }
  
  if (pos == -1) {
    String lowerField = String(fieldName);
    lowerField.toLowerCase();
    String pattern1 = "\"";
    pattern1 += lowerField;
    pattern1 += "\"";
    pos = objStr.indexOf(pattern1);
    if (pos == -1) {
      String pattern2 = "'";
      pattern2 += lowerField;
      pattern2 += "'";
      pos = objStr.indexOf(pattern2);
    }
  }
  
  if (pos == -1) {
    return;
  }
  
  pos = objStr.indexOf(":", pos);
  if (pos == -1) return;
  
  pos++;
  while (pos < objStr.length() && (objStr[pos] == ' ' || objStr[pos] == '\t')) {
    pos++;
  }
  
  if (pos >= objStr.length()) {
    return;
  }
  
  char quote = objStr[pos];
  if (quote != '"' && quote != '\'') {
    return;
  }
  
  pos++;
  int endPos = pos;
  while (endPos < objStr.length()) {
    if (objStr[endPos] == '\\') {
      endPos += 2;
      continue;
    }
    if (objStr[endPos] == quote) {
      break;
    }
    endPos++;
  }
  
  if (endPos >= objStr.length()) return;
  
  String valStr = objStr.substring(pos, endPos);
  int copyLen = min((int)valStr.length(), maxLen - 1);
  valStr.substring(0, copyLen).toCharArray(result, maxLen);
}

int parseIntField(const String& objStr, const char* fieldName) {
  String fieldPattern = "\"";
  fieldPattern += fieldName;
  fieldPattern += "\"";
  
  int pos = objStr.indexOf(fieldPattern);
  if (pos == -1) {
    fieldPattern = "'";
    fieldPattern += fieldName;
    fieldPattern += "'";
    pos = objStr.indexOf(fieldPattern);
  }
  
  if (pos == -1) {
    String upperField = String(fieldName);
    upperField.toUpperCase();
    String pattern1 = "\"";
    pattern1 += upperField;
    pattern1 += "\"";
    pos = objStr.indexOf(pattern1);
    if (pos == -1) {
      String pattern2 = "'";
      pattern2 += upperField;
      pattern2 += "'";
      pos = objStr.indexOf(pattern2);
    }
  }
  
  if (pos == -1) {
    String lowerField = String(fieldName);
    lowerField.toLowerCase();
    String pattern1 = "\"";
    pattern1 += lowerField;
    pattern1 += "\"";
    pos = objStr.indexOf(pattern1);
    if (pos == -1) {
      String pattern2 = "'";
      pattern2 += lowerField;
      pattern2 += "'";
      pos = objStr.indexOf(pattern2);
    }
  }
  
  if (pos == -1) {
    return INT_MIN;
  }
  
  pos = objStr.indexOf(":", pos);
  if (pos == -1) return INT_MIN;
  
  pos++;
  while (pos < objStr.length() && (objStr[pos] == ' ' || objStr[pos] == '\t')) {
    pos++;
  }
  
  if (pos >= objStr.length()) {
    return INT_MIN;
  }
  
  int endPos;
  char firstChar = objStr[pos];
  
  if (firstChar == '"') {
    endPos = pos + 1;
    while (endPos < objStr.length()) {
      if (objStr[endPos] == '\\') {
        endPos += 2;
        continue;
      }
      if (objStr[endPos] == '"') {
        break;
      }
      endPos++;
    }
    if (endPos >= objStr.length()) return INT_MIN;
    endPos++;
  } else if (firstChar == '\'') {
    endPos = pos + 1;
    while (endPos < objStr.length()) {
      if (objStr[endPos] == '\\') {
        endPos += 2;
        continue;
      }
      if (objStr[endPos] == '\'') {
        break;
      }
      endPos++;
    }
    if (endPos >= objStr.length()) return INT_MIN;
    endPos++;
  } else {
    endPos = objStr.indexOf(',', pos);
    if (endPos == -1) endPos = objStr.indexOf('}', pos);
    if (endPos == -1) endPos = objStr.indexOf(']', pos);
    if (endPos == -1) endPos = objStr.indexOf('\n', pos);
    if (endPos == -1) endPos = objStr.indexOf('\r', pos);
    if (endPos == -1) endPos = objStr.length();
  }
  
  String valStr = objStr.substring(pos, endPos);
  valStr.trim();
  
  if (valStr.startsWith("\"")) {
    valStr = valStr.substring(1, valStr.length() - 1);
  } else if (valStr.startsWith("'")) {
    valStr = valStr.substring(1, valStr.length() - 1);
  }
  
  return valStr.toInt();
}

void drawBackgroundFrame() {
 //   tft.clear();
  tft.fillRectangle(0, 0, 88, 88, COLOR_BLACK);
  tft.drawCircle(88, 88, 87, COLOR_WHITE);
  tft.fillRectangle(0, 88, 88, 180, COLOR_BLACK);
  tft.drawCircle(88, 88, 87, COLOR_WHITE);
  tft.drawCircle(88, 88, 58, 0x0400);
  tft.drawCircle(88, 88, 29, 0x0400);
  tft.fillRectangle(88, 0, 176, 88, COLOR_BLACK);
  tft.drawCircle(88, 88, 87, COLOR_WHITE);
  tft.fillRectangle(88, 88, 176, 180, COLOR_BLACK);
  tft.drawCircle(88, 88, 87, COLOR_WHITE);

 // tft.drawCircle(88, 88, 87, COLOR_WHITE);
  tft.drawCircle(88, 88, 58, 0x0400);
  tft.drawCircle(88, 88, 29, 0x0400);
  tft.fillCircle(88, 88, 1, COLOR_YELLOW);
}

int parseADSBPayload(const String& payload, AircraftData* acArray, int maxCount) {
  for (int i = 0; i < maxCount; i++) {
    acArray[i].valid = false;
  }
  
  int count = 0;
  
  unsigned int payloadLen = payload.length();
  Serial.printf("Payload长度: %u, 前200字符: %s\n", payloadLen, payload.substring(0, min((unsigned int)200, payloadLen)).c_str());
  
  int msgPos = payload.indexOf("\"msg\"");
  if (msgPos == -1) {
    msgPos = payload.indexOf("'msg'");
    if (msgPos == -1) {
      Serial.println("未找到msg字段");
      return -1;
    }
  }
  
  msgPos = payload.indexOf(":", msgPos);
  if (msgPos == -1) {
    Serial.println("未找到msg字段的冒号");
    return -1;
  }
  
  msgPos++;
  while (msgPos < payload.length() && (payload[msgPos] == ' ' || payload[msgPos] == '\t')) {
    msgPos++;
  }
  
  if (msgPos >= payload.length()) {
    Serial.println("msg字段值为空");
    return -1;
  }
  
  char quote = payload[msgPos];
  if (quote != '"' && quote != '\'') {
    Serial.printf("msg字段值格式错误，期望引号，实际: %c\n", quote);
    return -1;
  }
  
  msgPos++;
  int msgEnd = payload.indexOf(quote, msgPos);
  if (msgEnd == -1) {
    Serial.println("msg字段格式错误");
    return -1;
  }
  
  String msg = payload.substring(msgPos, msgEnd);
  Serial.printf("解析到msg字段: %s\n", msg.c_str());
  
  if (msg != "No error") {
    Serial.printf("API返回错误: %s\n", msg.c_str());
    return -1;
  }
  
  int acArrayStart = payload.indexOf("\"ac\"");
  if (acArrayStart == -1) {
    acArrayStart = payload.indexOf("'ac'");
    if (acArrayStart == -1) {
      Serial.println("未找到ac数组");
      return -1;
    }
  }
  
  acArrayStart = payload.indexOf(":", acArrayStart);
  if (acArrayStart == -1) {
    Serial.println("未找到ac字段的冒号");
    return -1;
  }
  
  acArrayStart++;
  while (acArrayStart < payload.length() && (payload[acArrayStart] == ' ' || payload[acArrayStart] == '\t')) {
    acArrayStart++;
  }
  
  if (acArrayStart >= payload.length() || payload[acArrayStart] != '[') {
    Serial.println("ac字段后不是数组");
    return -1;
  }
  
  acArrayStart++;
  
  int braceCount = 0;
  int objStart = -1;
  
  for (int i = acArrayStart; i < payload.length() && count < maxCount; i++) {
    char c = payload[i];
    
    if (c == '{') {
      braceCount++;
      if (braceCount == 1) {
        objStart = i + 1;
      }
    } else if (c == '}') {
      braceCount--;
      if (braceCount == 0 && objStart != -1) {
        String objStr = payload.substring(objStart, i);
        
        float dst = parseFloatField(objStr, "dst");
        float dir = parseFloatField(objStr, "dir");
        float track = parseFloatField(objStr, "track");
        int alt_baro = parseIntField(objStr, "alt_baro");
        parseStringField(objStr, "flight", acArray[count].flight, MAX_STRING_LEN);
        
        if (!isnan(dst) && !isnan(dir) && !isnan(track) && alt_baro != INT_MIN) {
          acArray[count].dst = dst;
          acArray[count].dir = dir;
          acArray[count].track = track;
          acArray[count].alt_baro = alt_baro;
          acArray[count].valid = true;
          count++;
          Serial.printf("成功添加第%d架飞机, flight: %s\n", count, acArray[count-1].flight);
        } else {
          Serial.println("字段不完整，跳过");
        }
        
        objStart = -1;
      }
    } else if (c == ']' && braceCount == 0) {
      break;
    }
  }
  
  return count;
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
  tft.setOrientation(0);
  
  // 设置字体（用于显示错误信息）
  tft.setFont(Terminal11x16);
  
  // 清除屏幕
  tft.clear();
  
  // 绘制背景框架
  drawBackgroundFrame();
  
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


void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastRefresh >= refreshInterval) {
    lastRefresh = currentMillis;
    fetchADSBData();
  }
  delay(100);
}
