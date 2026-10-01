#!/usr/bin/env python
"""生成标炬示例数据集：合成工业缺陷检测图像 + YOLO 标签。

用法：
    python scripts/gen_demo_data.py [--out assets/demo] [--count 12]

说明：
- 类别（与 AppController::ensureDemoProject 保持一致）：
    0 = 划痕 scratch
    1 = 凹陷 dent
    2 = 污染 pollution
- 输出：<out>/images/*.png 与 <out>/labels/*.txt（YOLO 归一化格式）。
- 图像为确定性的合成"金属面板缺陷"样式，用于演示模式与 E2E 验收，
  不含任何真实业务数据。随机种子固定，可重复生成。
"""
import argparse
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

CLASSES = ["scratch", "dent", "pollution"]
W, H = 640, 480


def brushed_metal_base(rng: random.Random) -> Image.Image:
    """拉丝金属质感底图：竖向渐变 + 水平拉丝纹理 + 噪声 + 暗角。"""
    # 竖向渐变
    top = rng.randint(120, 150)
    bottom = rng.randint(70, 100)
    grad = np.linspace(top, bottom, H, dtype=np.float32)[:, None]
    grad = np.repeat(grad, W, axis=1)
    # 水平拉丝：随机亮/暗细线
    texture = np.zeros((H, W), dtype=np.float32)
    for _ in range(rng.randint(140, 200)):
        y = rng.randint(0, H - 1)
        shade = rng.choice([-1, 1]) * rng.uniform(4, 14)
        x0 = rng.randint(0, W // 2)
        x1 = rng.randint(x0 + 40, W)
        thickness = rng.randint(0, 1)
        texture[y:y + thickness + 1, x0:x1] = shade
    # 噪声
    noise = np.random.default_rng(rng.randint(0, 2**31)).normal(0, 5, (H, W)).astype(np.float32)
    img = np.clip(grad + texture + noise, 0, 255).astype(np.uint8)
    base = Image.fromarray(img, mode="L").convert("RGB")

    # 面板边框 + 四角螺钉（像电池托盘面板）
    draw = ImageDraw.Draw(base)
    m = 46
    draw.rounded_rectangle([m, m, W - m, H - m], radius=18, outline=(52, 54, 60), width=5)
    draw.rounded_rectangle([m + 6, m + 6, W - m - 6, H - m - 6], radius=14, outline=(150, 152, 158), width=1)
    for cx, cy in [(m, m), (W - m, m), (m, H - m), (W - m, H - m)]:
        draw.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], fill=(70, 72, 78), outline=(35, 36, 40), width=2)
        draw.line([cx - 5, cy, cx + 5, cy], fill=(35, 36, 40), width=2)
    return base


def draw_scratch(draw: ImageDraw.Draw, rng: random.Random):
    """划痕：斜向细亮线 + 局部暗边，返回 YOLO bbox。"""
    x0 = rng.randint(120, W - 260)
    y0 = rng.randint(90, H - 180)
    length = rng.randint(140, 300)
    angle = rng.uniform(-0.5, 0.5)
    steps = rng.randint(24, 40)
    dx = math.cos(angle) * length / steps
    dy = math.sin(angle) * length / steps
    pts, x, y = [], float(x0), float(y0)
    for _ in range(steps):
        pts.append((x, y))
        x += dx + rng.uniform(-1.5, 1.5)
        y += dy + rng.uniform(-1.5, 1.5)
    width = rng.choice([1, 2])
    draw.line(pts, fill=(222, 224, 228), width=width)
    draw.line([(p[0] + 1, p[1] + 1) for p in pts], fill=(96, 98, 104), width=width)
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    pad = 6
    return norm_bbox(min(xs) - pad, min(ys) - pad, max(xs) + pad, max(ys) + pad)


def dent_on(base: Image.Image, rng: random.Random):
    """在 base 上绘制凹陷并返回 bbox（独立函数避免 draw 句柄问题）。"""
    cx = rng.randint(160, W - 160)
    cy = rng.randint(130, H - 130)
    rx = rng.randint(40, 80)
    ry = int(rx * rng.uniform(0.6, 0.9))
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    md.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=70)
    mask = mask.filter(ImageFilter.GaussianBlur(9))
    m = np.array(mask, dtype=np.float32) / 255.0
    arr = np.array(base, dtype=np.float32)
    m3 = m[:, :, None]
    arr *= (1.0 - m3 * 0.55)  # 变暗
    out = Image.fromarray(arr.astype(np.uint8))
    # 高光弧（凹陷边缘反光）
    od = ImageDraw.Draw(out)
    od.arc([cx - rx, cy - ry, cx + rx, cy + ry], start=rng.randint(180, 250), end=rng.randint(300, 360),
           fill=(210, 214, 220), width=2)
    pad = 8
    bbox = norm_bbox(cx - rx - pad, cy - ry - pad, cx + rx + pad, cy + ry + pad)
    return out, bbox


def pollution_on(base: Image.Image, rng: random.Random):
    """污染：不规则深色斑块群，返回 bbox。"""
    cx = rng.randint(140, W - 140)
    cy = rng.randint(120, H - 120)
    spots = rng.randint(5, 10)
    xs, ys = [], []
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    for _ in range(spots):
        ox = cx + rng.randint(-46, 46)
        oy = cy + rng.randint(-36, 36)
        r = rng.randint(7, 22)
        shade = rng.randint(52, 88)
        alpha = rng.randint(160, 220)
        ld.ellipse([ox - r, oy - r, ox + r, oy + r], fill=(shade, shade - 8, shade - 16, alpha))
        xs += [ox - r, ox + r]; ys += [oy - r, oy + r]
    layer = layer.filter(ImageFilter.GaussianBlur(2))
    base.paste(layer, (0, 0), layer)
    pad = 6
    bbox = norm_bbox(min(xs) - pad, min(ys) - pad, max(xs) + pad, max(ys) + pad)
    return base, bbox


def norm_bbox(x0: float, y0: float, x1: float, y1: float):
    """像素 bbox → YOLO 归一化 (class 无关, cx, cy, w, h)，并夹取到图内。"""
    x0 = max(0.0, min(x0, W)); x1 = max(0.0, min(x1, W))
    y0 = max(0.0, min(y0, H)); y1 = max(0.0, min(y1, H))
    cx = (x0 + x1) / 2 / W
    cy = (y0 + y1) / 2 / H
    bw = (x1 - x0) / W
    bh = (y1 - y0) / H
    return cx, cy, bw, bh


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join("assets", "demo"))
    ap.add_argument("--count", type=int, default=12)
    ap.add_argument("--defect-free-first", type=int, default=0,
                    help="前 K 张无缺陷（不生成标签，用于异常检测 train/good）")
    args = ap.parse_args()

    img_dir = os.path.join(args.out, "images")
    lbl_dir = os.path.join(args.out, "labels")
    os.makedirs(img_dir, exist_ok=True)
    os.makedirs(lbl_dir, exist_ok=True)

    rng = random.Random(42)
    total_boxes = 0
    for i in range(args.count):
        rng_i = random.Random(1000 + i)
        base = brushed_metal_base(rng_i)
        boxes = []

        # 异常检测模式：前 defect_free_first 张无缺陷（train/good）；
        # 常规模式：最后 2 张无标签（演示「未标注」流转）
        if i < args.defect_free_first:
            defects = 0
        elif args.defect_free_first > 0:
            defects = rng_i.randint(1, 3)
        else:
            defects = rng_i.randint(1, 3) if i < args.count - 2 else 0
        kinds = rng_i.sample(range(3), k=min(3, defects))
        for k in kinds:
            if k == 0:
                d = ImageDraw.Draw(base)
                boxes.append((0,) + draw_scratch(d, rng_i))
            elif k == 1:
                base, bb = dent_on(base, rng_i)
                boxes.append((1,) + bb)
            else:
                base, bb = pollution_on(base, rng_i)
                boxes.append((2,) + bb)

        name = f"panel_{i:03d}"
        base.save(os.path.join(img_dir, name + ".jpg"), quality=88, optimize=True)
        if boxes:
            with open(os.path.join(lbl_dir, name + ".txt"), "w", encoding="utf-8") as f:
                for cls, cx, cy, bw, bh in boxes:
                    f.write(f"{cls} {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}\n")
            total_boxes += len(boxes)
        print(f"{name}.jpg  defects={len(boxes)}")

    print(f"done: {args.count} images, {total_boxes} boxes, classes={CLASSES}")


if __name__ == "__main__":
    main()
