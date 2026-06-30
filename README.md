# esp32_TFT_22_ILI9225_webImage_bmp_demo 配套图片服务端
## 项目说明
本仓库配套分为两部分：
1. ESP32 客户端工程：通过WiFi请求图片接口，下载BMP图片渲染至2.2寸ILI9225 SPI TFT屏幕
2. Python Flask 图片服务端（本app.py）：提供HTTP接口，读取本地BMP位图，返回二进制图片流给ESP32设备

服务端作用：本地局域网快速提供BMP图片资源，无需外网HTTPS图床，内网传输速度更快、无证书校验问题，方便调试更换显示图片。

<img width="300" alt="image" src="https://github.com/user-attachments/assets/ee929fc8-73ef-4362-9da8-568547092f64" />

## 一、功能特性
1. 基于Flask搭建轻量HTTP图片接口服务
2. 读取本地 `bmp/` 文件夹下指定 `.bmp` 位图文件
3. 接口直接返回图片二进制流，附带正确 `image/bmp` MIME头部，ESP32可直接解析下载
4. 支持自定义端口、局域网IP访问，同WiFi下ESP32可直接请求
5. 简易错误处理：文件不存在返回404提示
6. 极简代码无多余依赖，Windows / Mac / Linux 全平台兼容

## 二、文件目录结构

<img width="377" height="120" alt="image" src="https://github.com/user-attachments/assets/39583cd3-d80e-49f3-99fb-60538a220cf6" />

## 三、环境依赖
### Python版本
Python 3.7 及以上

### 依赖库
- Flask
- Flask>=2.0.0
一键安装依赖：
```bash
pip install flask
```

## 四、快速启动服务端
1. 在项目根目录创建 `bmp` 文件夹，放入尺寸176×220的标准BMP图片
2. 运行服务脚本
```bash
python app.py
```
3. 启动成功后控制台输出局域网访问地址，例如：
`http://192.168.1.100:5000/getImage`

## 五、接口说明
### 图片获取接口
- 请求地址：`http://本机IP:5000/getImage`
- 请求方式：GET
- 返回内容：指定bmp目录下图片二进制流，Content-Type: image/bmp
- 异常返回：图片缺失返回文本提示 + HTTP 404状态码

### ESP32客户端配置对应修改
将main.cpp中imageUrl改为本机服务地址：
```cpp
const char* imageUrl = "http://192.168.1.100:5000/getImage";
```
> 注意：内网HTTP无需WiFiClientSecure，可改用普通WiFiClient减少资源占用。

## 六、核心逻辑简述
1. 启动Flask监听0.0.0.0，允许局域网其他设备访问
2. 定义路由 `/getImage`，读取本地bmp目录位图
3. 以二进制模式读取图片文件，封装Response返回
4. 捕获文件不存在异常，返回404错误提示

## 七、配套ESP32硬件说明
服务端配套硬件工程：esp32_TFT_22_ILI9225_webImage_bmp_demo
- 主控：ESP32
- 显示屏：2.2寸 SPI ILI9225 TFT LCD 176×220分辨率
- 通信：硬件VSPI驱动屏幕，WiFi连接局域网请求本服务接口
- 客户端功能：逐行解析BMP、低内存渲染、自动色彩转换、图像翻转修正

## 八、使用注意事项
1. BMP图片必须为176×220分辨率，16bit/24bit无压缩位图，否则ESP32渲染报错
2. 电脑与ESP32设备需连接**同一局域网WiFi**，不可跨网段
3. 防火墙放行5000端口，否则ESP32无法访问接口
4. 如需修改监听端口，直接修改app.py内 `app.run(port=5000)` 参数
5. 如需切换图片，替换bmp目录内文件，无需修改代码

