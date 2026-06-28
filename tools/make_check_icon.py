from PIL import Image, ImageDraw

SIZE = 64
WHITE = (255, 255, 255, 255)
DARK = (31, 41, 55, 255)   # 深色对勾,用于未勾状态(白底时也能看见)


def draw_check(filename, color=WHITE):
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # 对勾：起点 (16, 33) → (28, 45) → (48, 22)
    points = [(16, 33), (28, 45), (48, 22)]
    d.line(points, fill=color, width=6, joint="curve")
    # 圆头圆尾
    for p in points:
        d.ellipse((p[0] - 3, p[1] - 3, p[0] + 3, p[1] + 3), fill=color)
    img.save(filename, "PNG")
    print("wrote", filename)


if __name__ == "__main__":
    import os
    out = r"d:\Qt\ffmpegProjects\Smart_Player\resources\SmartPlayer-icon"
    os.makedirs(out, exist_ok=True)
    # 白色对勾(用于已选中且 indicator 填充蓝时)
    draw_check(os.path.join(out, "check_white.png"), WHITE)
    # 深色对勾(用于 hover 半透明蓝底时,白勾可能不够对比)
    draw_check(os.path.join(out, "check_skyblue.png"), (14, 165, 233, 255))