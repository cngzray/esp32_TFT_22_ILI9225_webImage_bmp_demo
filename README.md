# ESP32 TFT2.2 ILI9225 ADS-B 飞机雷达显示屏
## 项目简介
本项目基于 **ESP32 + 2.2寸 ILI9225 SPI TFT液晶屏**，通过网络请求公共ADS-B开放接口 `opendata.adsb.fi` 获取周边实时航班数据，在屏幕上绘制简易雷达界面，实时展示范围内飞机位置、航向、航班号，并根据飞行高度区分不同机身颜色。

雷达以设定经纬度为中心点，限定半径范围内抓取航班，每5秒自动刷新一次空中目标，无第三方JSON解析库，纯字符串手动解析接口数据，长期稳定运行无内存崩溃问题。

<img width="301" height="451" alt="image" src="https://github.com/user-attachments/assets/9943e428-25fd-4508-bf91-116057723b1a" />


### 功能特性
1. 联网拉取全球公开ADS-B航班实时数据
2. 2.2寸TFT绘制圆形雷达扫描界面，含多层距离圈
3. 根据气压高度自动分配飞机标识颜色：
    - ≤2000ft：红色
    - 2000~6000ft：橙色
    - 6000~10000ft：黄色
    - 10000~15000ft：浅绿
    - 15000~22000ft：绿色
    - 22000~30000ft：浅蓝
    - 30000~35000ft：蓝色
    - ＞35000ft：紫色
4. 每个飞机绘制定位圆点+航向指示短线，下方显示航班呼号
5. 纯原生字符串解析API返回数据，不依赖ArduinoJson，规避内存溢出
6. 自定义雷达中心点、监控半径、刷新间隔、WiFi账号
7. 独立背光控制引脚，可常亮/自定义开关背光

## 硬件清单
| 器件 | 说明 |
|------|------|
| ESP32开发板 | 任意ESP32-WROOM系列 |
| 2.2寸 SPI TFT屏 ILI9225 | 驱动库 `TFT_22_ILI9225` |
| 杜邦线若干 | 连接SPI总线与控制引脚 |
| 3.3V电源 | TFT屏幕仅支持3.3V，禁止5V直供 |

## 硬件引脚接线（VSPI）
代码固定使用VSPI外设，接线严格对应：
| TFT引脚 | ESP32 GPIO | 宏定义 |
|--------|------------|--------|
| RST    | 26         | TFT_RST |
| RS/DC  | 25         | TFT_RS |
 SCK    | 18 (VSPI-SCK) | TFT_CLK |
| SDI/MOSI | 23 (VSPI-MOSI) | TFT_SDI |
| CS     | 5 (VSPI-SS) | TFT_CS |
| LED背光 | 4          | TFT_LED |

> MISO引脚本项目未使用，无需接线。

## 依赖库安装
Arduino库管理器搜索安装以下库：
1. `TFT_22_ILI9225` — ILI9225 2.2寸SPI TFT驱动
2. ESP32自带内置库（无需手动安装）：
   - WiFi
   - WiFiClientSecure
   - HTTPClient
   - SPI
   - math.h

## 快速配置使用
### 1. WiFi配置
修改代码内WiFi账号密码：
```cpp
const char* ssid = "你的WiFi名称";
const char* password = "你的WiFi密码";
```

### 2. 雷达中心点与监控半径
```cpp
#define DIST 50 // 监控半径，单位km
// 目标中心点经纬度
String apiUrl = String("https://opendata.adsb.fi/api/v3/lat/23.206/lon/113.2844/dist/") + DIST;
```
修改 `lat`/`lon` 为你所在城市坐标，`DIST` 修改监控半径（单位km）。

### 3. 刷新间隔
```cpp
const long refreshInterval = 5000; // 5000ms=5秒刷新一次航班数据
```

### 4. 屏幕参数
雷达中心坐标固定 `centerX=88, centerY=88`，屏幕分辨率176*220，如需旋转屏幕修改：
```cpp
tft.setOrientation(0); // 0=180度旋转，可修改0~3调整显示方向
```

## 数据接口说明
数据源：`opendata.adsb.fi` 公共ADS-B开放API
接口格式：
```
https://opendata.adsb.fi/api/v3/lat/纬度/lon/经度/dist/监控半径km
```
接口返回JSON结构包含：
- `msg`：接口状态，`No error` 代表数据正常
- `ac`：航班数组，单架飞机字段：
  - `dst`：距离中心点距离(km)
  - `dir`：飞机相对中心点方位角（正北0°顺时针）
  - `track`：飞机自身飞行航向
  - `alt_baro`：气压飞行高度(英尺)
  - `flight`：航班呼号

## 核心坐标转换原理（重点）
### 角度坐标系差异（踩坑重点）
1. ADS-B航空方位角 `dir`：
   - 0° = 正北，顺时针递增
   - 90°=东，180°=南，270°=西
2. C语言math库三角函数坐标系：
   - 0°=X轴正东，逆时针递增
   - 90°=北，180°=西，270°=南

直接使用`dir`计算坐标会整体偏移90度，必须转换角度：
```cpp
// 航空方位角 → 数学三角函数角度转换公式
float angleRad = (90.0f - dir) * PI / 180.0f;
float dx = dst * scale * cos(angleRad);
float dy = dst * scale * sin(angleRad);
x = centerX + (int)dx;
y = centerY - (int)dy;
```

## 程序运行逻辑
1. `setup()`
   - 初始化串口、TFT背光、VSPI SPI总线
   - TFT屏幕初始化，绘制静态雷达背景（外圈、两层距离圈、中心黄点）
   - 连接WiFi，打印设备本地IP

2. `loop()`
   - 每5秒触发一次 `fetchADSBData()` 拉取ADS-B接口数据

3. `fetchADSBData()`
   - HTTPS请求API，关闭证书校验 `setInsecure()`
   - 获取返回文本，调用手动字符串解析函数提取航班数组
   - 清空雷达旧画面，重新绘制背景
   - 遍历所有有效航班，根据高度分配颜色，转换坐标绘制飞机标记+航班号

4. 解析函数（无ArduinoJson）
   - `parseFloatField`：提取浮点字段（距离、方位、航向）
   - `parseIntField`：提取整数字段（飞行高度）
   - `parseStringField`：提取字符串字段（航班呼号）
   - `parseADSBPayload`：拆分JSON数组，批量解析所有飞机对象

## 踩过的坑 & 解决方案
### 1. ArduinoJson库频繁内存溢出、ESP32崩溃重启
**问题**：ADS-B接口返回数据量大，ArduinoJson动态分配内存，ESP32小内存场景下极易堆溢出、死机、反复重启。
**解决方案**：放弃JSON库，手写纯字符串匹配解析器，逐字符截取字段值，无动态大内存分配，长期7×24小时稳定运行。

### 2. 飞机方位完全错位，全部跑到屏幕东侧
**问题根源**：航空方位角与数学三角函数坐标系定义完全相反，直接使用`dir`转弧度计算坐标会整体偏移90°。
**修正公式**：
```cpp
float angleRad = (90.0f - dir) * PI / 180.0f;
```
原理：用90度减去航空顺时针方位角，转换为数学逆时针坐标系角度，匹配cos/sin计算逻辑。

### 3. HTTPS接口请求失败、证书报错
**解决方案**：WiFiClientSecure开启 `client.setInsecure()` 关闭SSL证书校验，适配公共免费API。

### 4. TFT屏幕文字/图形显示错位、画面颠倒
- 修改 `tft.setOrientation(0~3)` 调整屏幕旋转；
- 雷达坐标 `centerX/centerY` 基于176宽220高屏幕设计，更换分辨率需重新换算缩放系数 `scale = 80.0 / DIST`。

### 5. 屏幕背光不亮
确认TFT_LED引脚定义为GPIO4，`setup()` 中默认拉高开启背光，如需PWM调光可修改为analogWrite。

## 已知限制
1. 公共接口 `opendata.adsb.fi` 存在访问频率限制，刷新间隔建议≥3秒，不要频繁请求；
2. 免费API数据存在延迟，航班位置为几秒前的历史数据；
3. 屏幕尺寸仅176*220，同时超过50架飞机会丢弃超出部分目标（`MAX_AIRCRAFT=50`）；
4. 仅支持气压高度alt_baro，无GPS高度字段兼容。

## 演示效果
1. 屏幕中心黄色圆点为本机坐标；
2. 两层灰色同心圆代表距离参考圈；
3. 彩色圆点+短线代表飞机，短线指向飞行前进方向；
4. 圆点下方白色文字显示航班呼号；
5. 高度越高，飞机标识颜色由红逐步过渡到紫色。

