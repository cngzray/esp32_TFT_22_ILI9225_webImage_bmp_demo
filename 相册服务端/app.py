from flask import Flask, request, jsonify, send_from_directory, send_file
from PIL import Image
import requests
import os
import struct
import time
import random

app = Flask(__name__)
PORT = 3000

# 添加跨域支持
@app.after_request
def add_cors_headers(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Methods'] = 'GET, POST, OPTIONS'
    response.headers['Access-Control-Allow-Headers'] = 'Content-Type'
    return response

# 确保uploads目录存在
UPLOADS_DIR = os.path.join(os.path.dirname(__file__), 'uploads')
if not os.path.exists(UPLOADS_DIR):
    os.makedirs(UPLOADS_DIR)

# 创建16位真彩色BMP文件 (RGB565格式)
def create_bmp_16bit(image_path, output_path, width=176, height=220):
    # 打开图片
    img = Image.open(image_path).convert('RGB')
    
    # 计算原图尺寸
    src_width, src_height = img.size
    
    # 计算目标宽高比
    target_ratio = width / height
    src_ratio = src_width / src_height
    
    # 等比例裁切 - 以中心为基准
    if src_ratio > target_ratio:
        # 原图更宽，按高度缩放后裁剪宽度
        scale_factor = height / src_height
        new_width = int(src_width * scale_factor)
        new_height = height
        # 计算裁剪位置（居中）
        crop_x = (new_width - width) // 2
        crop_y = 0
    else:
        # 原图更高或相等，按宽度缩放后裁剪高度
        scale_factor = width / src_width
        new_width = width
        new_height = int(src_height * scale_factor)
        # 计算裁剪位置（居中）
        crop_x = 0
        crop_y = (new_height - height) // 2
    
    # 先缩放
    img = img.resize((new_width, new_height), Image.Resampling.LANCZOS)
    
    # 再居中裁剪
    img = img.crop((crop_x, crop_y, crop_x + width, crop_y + height))
    
    # 获取像素数据
    pixels = list(img.getdata())
    
    # BMP文件头 (14字节)
    file_header = bytearray(14)
    file_header[0:2] = b'BM'  # 文件标识
    # 文件大小 = 14 + 40 + 12 + (width * height * 2)
    file_size = 14 + 40 + 12 + (width * height * 2)
    file_header[2:6] = struct.pack('<I', file_size)
    file_header[6:10] = struct.pack('<I', 0)  # 保留
    data_offset = 14 + 40 + 12  # 像素数据偏移
    file_header[10:14] = struct.pack('<I', data_offset)
    
    # BMP信息头 (40字节)
    info_header = bytearray(40)
    info_header[0:4] = struct.pack('<I', 40)  # 信息头大小
    info_header[4:8] = struct.pack('<i', width)  # 宽度
    info_header[8:12] = struct.pack('<i', height)  # 高度
    info_header[12:14] = struct.pack('<H', 1)  # 色彩平面数
    info_header[14:16] = struct.pack('<H', 16)  # 每像素位数
    info_header[16:20] = struct.pack('<I', 3)  # 压缩方式 BI_BITFIELDS
    info_header[20:24] = struct.pack('<I', width * height * 2)  # 图像大小
    info_header[24:28] = struct.pack('<i', 2835)  # 水平分辨率 (96dpi)
    info_header[28:32] = struct.pack('<i', 2835)  # 垂直分辨率
    info_header[32:36] = struct.pack('<I', 0)  # 使用的颜色数
    info_header[36:40] = struct.pack('<I', 0)  # 重要颜色数
    
    # 颜色掩码 (12字节) - RGB565格式
    color_mask = bytearray(12)
    color_mask[0:4] = struct.pack('<I', 0xF800)  # R掩码
    color_mask[4:8] = struct.pack('<I', 0x07E0)  # G掩码
    color_mask[8:12] = struct.pack('<I', 0x001F)  # B掩码
    
    # 像素数据 (BMP从底部开始存储)
    pixel_data = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            idx = y * width + x
            r, g, b = pixels[idx]
            # 转换为RGB565
            r5 = (r >> 3) & 0x1F
            g6 = (g >> 2) & 0x3F
            b5 = (b >> 3) & 0x1F
            rgb565 = (r5 << 11) | (g6 << 5) | b5
            pixel_data.extend(struct.pack('<H', rgb565))
    
    # 合并所有部分
    bmp_data = file_header + info_header + color_mask + pixel_data
    
    # 写入文件
    with open(output_path, 'wb') as f:
        f.write(bmp_data)

@app.route('/')
def index():
    return send_from_directory('.', 'index.html')

@app.route('/convert', methods=['POST'])
def convert_image():
    try:
        data = request.get_json()
        image_url = data.get('imageUrl', '')
        
        if not image_url or not image_url.startswith('http'):
            return jsonify({'error': '请提供有效的图片URL'}), 400
        
        # 下载图片
        response = requests.get(image_url, stream=True)
        if response.status_code != 200:
            return jsonify({'error': '无法下载图片'}), 400
        
        # 保存临时文件
        temp_path = os.path.join(UPLOADS_DIR, 'temp.jpg')
        with open(temp_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=1024):
                f.write(chunk)
        
        # 生成唯一文件名
        timestamp = str(int(time.time()))
        output_path = os.path.join(UPLOADS_DIR, f'{timestamp}.bmp')
        
        # 转换为176x220的16位BMP
        create_bmp_16bit(temp_path, output_path, 176, 220)
        
        # 删除临时文件
        if os.path.exists(temp_path):
            os.remove(temp_path)
        
        return jsonify({
            'success': True,
            'message': '图片转换成功',
            'filename': f'{timestamp}.bmp',
            'downloadUrl': f'/uploads/{timestamp}.bmp'
        })
    
    except Exception as e:
        print(f'转换失败: {str(e)}')
        return jsonify({'error': '转换失败: ' + str(e)}), 500

@app.route('/uploads/<filename>')
def uploads(filename):
    return send_from_directory(UPLOADS_DIR, filename)

@app.route('/random-bmp')
def random_bmp():
    # 获取uploads目录下所有bmp文件
    bmp_files = [f for f in os.listdir(UPLOADS_DIR) if f.lower().endswith('.bmp')]
    
    if not bmp_files:
        return 'No BMP files found in uploads directory', 404
    
    # 随机选择一个文件
    random_file = random.choice(bmp_files)
    file_path = os.path.join(UPLOADS_DIR, random_file)
    
    # 直接返回图片数据
    return send_file(file_path, mimetype='image/bmp')

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=PORT, debug=True)