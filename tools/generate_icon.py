import os
import math
from PIL import Image, ImageDraw, ImageFilter

def create_flowsight_icon():
    os.makedirs("resources", exist_ok=True)
    
    # 512x512 超采样画布，保证缩放到各种尺寸均极致清晰抗锯齿
    size = 512
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 1. 绘制暗黑科技圆角底座 (Squircle)
    margin = 24
    rect_box = [margin, margin, size - margin, size - margin]
    radius = 110

    # 底座阴影
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_draw.rounded_rectangle(
        [margin + 4, margin + 8, size - margin - 4, size - margin + 8],
        radius=radius,
        fill=(0, 0, 0, 160)
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(16))
    img.paste(shadow, (0, 0), shadow)

    # 主底座深黑曜石渐变填充
    for i in range(margin, size - margin):
        ratio = (i - margin) / (size - 2 * margin)
        # 从 #0f172a 渐变到 #0b0f19
        r = int(15 * (1 - ratio) + 11 * ratio)
        g = int(23 * (1 - ratio) + 15 * ratio)
        b = int(42 * (1 - ratio) + 25 * ratio)
        # 用 mask 绘制切片
        pass

    # 绘制底座背景
    draw.rounded_rectangle(rect_box, radius=radius, fill=(13, 17, 23, 255), outline=(56, 189, 248, 200), width=6)

    # 2. 内发光科技感圆环 (Radar Aperture Ring)
    center = size // 2
    ring_radius = 160
    ring_box = [center - ring_radius, center - ring_radius, center + ring_radius, center + ring_radius]
    draw.ellipse(ring_box, outline=(30, 41, 59, 255), width=8)

    # 动态扫描光弧 (45度到 225度)
    draw.arc(ring_box, start=30, end=150, fill=(56, 189, 248, 255), width=10)
    draw.arc(ring_box, start=210, end=330, fill=(46, 213, 115, 255), width=10)

    # 3. 几何十字准星刻度 (Crosshairs)
    cross_len = 24
    draw.line([center, center - ring_radius - cross_len, center, center - ring_radius + cross_len], fill=(56, 189, 248, 220), width=4)
    draw.line([center, center + ring_radius - cross_len, center, center + ring_radius + cross_len], fill=(56, 189, 248, 220), width=4)
    draw.line([center - ring_radius - cross_len, center, center - ring_radius + cross_len, center], fill=(46, 213, 115, 220), width=4)
    draw.line([center + ring_radius - cross_len, center, center + ring_radius + cross_len, center], fill=(46, 213, 115, 220), width=4)

    # 4. 中心地面客流双向流动矢量箭头 (Flow In / Flow Out Chevrons)
    # 上箭头 (绿色进 - Entry)
    entry_color = (46, 213, 115, 255)
    exit_color = (56, 189, 248, 255)

    # 绘制上行流动箭头 (Entry)
    chevron_w = 64
    chevron_h = 32
    y_entry = center - 52
    draw.polygon([
        (center, y_entry - chevron_h),
        (center + chevron_w, y_entry + 4),
        (center + chevron_w - 18, y_entry + 14),
        (center, y_entry - 8),
        (center - chevron_w + 18, y_entry + 14),
        (center - chevron_w, y_entry + 4),
    ], fill=entry_color)

    # 绘制下行流动箭头 (Exit)
    y_exit = center + 52
    draw.polygon([
        (center, y_exit + chevron_h),
        (center + chevron_w, y_exit - 4),
        (center + chevron_w - 18, y_exit - 14),
        (center, y_exit + 8),
        (center - chevron_w + 18, y_exit - 14),
        (center - chevron_w, y_exit - 4),
    ], fill=exit_color)

    # 5. 核心感知晶体光球 (Vision Pupil Core)
    core_r = 26
    core_box = [center - core_r, center - core_r, center + core_r, center + core_r]
    # 外光晕
    glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    glow_draw.ellipse([center - 45, center - 45, center + 45, center + 45], fill=(56, 189, 248, 120))
    glow = glow.filter(ImageFilter.GaussianBlur(12))
    img.paste(glow, (0, 0), glow)

    # 晶体本体
    draw.ellipse(core_box, fill=(240, 246, 252, 255), outline=(56, 189, 248, 255), width=4)
    # 高光点
    draw.ellipse([center - 12, center - 14, center - 2, center - 4], fill=(255, 255, 255, 230))

    # 6. 保存高质量 PNG 原图
    png_path = "resources/icon.png"
    img.save(png_path, "PNG")
    print(f"Generated PNG: {png_path}")

    # 7. 保存 Windows 标准多规格 ICO 文件 (含 256, 128, 64, 48, 32, 16 像素多尺寸)
    ico_path = "resources/icon.ico"
    icon_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
    img.save(ico_path, format="ICO", sizes=icon_sizes)
    print(f"Generated Multi-Resolution ICO: {ico_path} ({os.path.getsize(ico_path)} bytes)")

if __name__ == "__main__":
    create_flowsight_icon()
