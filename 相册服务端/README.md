这个是相册的服务端

1.安装依赖文件

pip install -r requirements.txt

2.启动服务端程序

python app.py

3.访问前端界面

http://localhost:3000

在界面输入jpg图片的链接，提交后程序会自动转换为tft屏幕可显示的规格，保存在uploads目录内

 启动服务器后，访问 http://localhost:3000/random-bmp 即可直接获取随机的 BMP 图片数据（非 HTML 页面）。

 基于这个相册使用的esp32代码在以下位置：
 https://github.com/cngzray/esp32_TFT_22_ILI9225_webImage_bmp_demo/blob/main/src/main_%E7%9B%B8%E5%86%8C.cpp

<img width="300"  alt="image" src="https://github.com/user-attachments/assets/a79f54a7-f26e-47ed-b7d4-cbd5145d923a" />

<img width="300"  alt="image" src="https://github.com/user-attachments/assets/e8956027-a50e-49cc-939c-0f2cff5f763d" />
