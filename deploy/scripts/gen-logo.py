#!/usr/bin/env python3
"""ロゴの SVG (docs/development/logo_proposal/e-cards.svg) を PNG と ICO に焼く。

    ./deploy/scripts/gen-logo.py <出力ディレクトリ>

この開発環境には画像変換ツール (ImageMagick / rsvg / cairosvg / Pillow) が
無いので、必要な図形 (角丸長方形の塗りと線) だけを自前で描いている。
汎用の SVG レンダラではない。SVG を変えたらここも合わせる。

なぜ PNG が要るか: 既存サイトは THEME_IMAGE_LOGO に image/logo.png を
保存済みで、svg を既定にしても既存サイトはそちらを見に行く。同じマークを
描いた png に差し替えないと、猫のロゴが残るか画像が壊れる。
"""
import struct
import sys
import zlib

INK = (0x23, 0x28, 0x2c)
ACCENT = (0xd2, 0x60, 0x3a)
WHITE = (0xff, 0xff, 0xff)
VIEWBOX = 48.0
STROKE = 3.5
SAMPLES = 4          # 1 画素あたり SAMPLES x SAMPLES

# (x, y, w, h, r, 線の色, 塗りの色, 塗りの不透明度) — 奥から手前へ
SHAPES = [
    (17.0,  6.75, 24.0, 16.0, 3.5, ACCENT, None,  0.0),
    (11.0, 16.0,  24.0, 16.0, 3.5, INK,    WHITE, 0.55),
    ( 5.0, 25.25, 24.0, 16.0, 3.5, INK,    WHITE, 0.90),
]


def rounded_rect_sdf(px, py, x, y, w, h, r):
    """角丸長方形の符号つき距離。内側が負。"""
    cx, cy = x + w / 2.0, y + h / 2.0
    qx = abs(px - cx) - (w / 2.0 - r)
    qy = abs(py - cy) - (h / 2.0 - r)
    outside = (max(qx, 0.0) ** 2 + max(qy, 0.0) ** 2) ** 0.5
    inside = min(max(qx, qy), 0.0)
    return outside + inside - r


def render(size):
    scale = VIEWBOX / size
    step = scale / SAMPLES
    offset = step / 2.0
    half_stroke = STROKE / 2.0
    canvas = [[0.0, 0.0, 0.0, 0.0] for _ in range(size * size)]

    for py in range(size):
        for px in range(size):
            cell = canvas[py * size + px]
            for x, y, w, h, r, stroke_color, fill_color, fill_alpha in SHAPES:
                fill_hits = 0
                stroke_hits = 0
                for sy in range(SAMPLES):
                    vy = (py + 0.0) * scale + sy * step + offset
                    for sx in range(SAMPLES):
                        vx = (px + 0.0) * scale + sx * step + offset
                        d = rounded_rect_sdf(vx, vy, x, y, w, h, r)
                        if d < 0.0:
                            fill_hits += 1
                        if -half_stroke <= d <= half_stroke:
                            stroke_hits += 1
                total = float(SAMPLES * SAMPLES)
                if fill_color is not None and fill_hits:
                    over(cell, fill_color, fill_alpha * (fill_hits / total))
                if stroke_hits:
                    over(cell, stroke_color, stroke_hits / total)
    return canvas


def over(cell, color, alpha):
    """source-over 合成 (cell は非プリマルチプライの RGBA)。"""
    if alpha <= 0.0:
        return
    dst_a = cell[3]
    out_a = alpha + dst_a * (1.0 - alpha)
    if out_a <= 0.0:
        return
    for i in range(3):
        cell[i] = (color[i] * alpha + cell[i] * dst_a * (1.0 - alpha)) / out_a
    cell[3] = out_a


def to_png(canvas, size):
    raw = bytearray()
    for py in range(size):
        raw.append(0)                      # フィルタ: なし
        for px in range(size):
            r, g, b, a = canvas[py * size + px]
            raw += bytes((int(r + 0.5), int(g + 0.5), int(b + 0.5), int(a * 255 + 0.5)))

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data
                + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))

    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(bytes(raw), 9))
            + chunk(b'IEND', b''))


def to_ico(pngs):
    """PNG をそのまま収める ICO (Windows Vista 以降・主要ブラウザが読める)。"""
    header = struct.pack('<HHH', 0, 1, len(pngs))
    offset = 6 + 16 * len(pngs)
    entries, blobs = b'', b''
    for size, png in pngs:
        entries += struct.pack('<BBBBHHII', size if size < 256 else 0,
                               size if size < 256 else 0, 0, 0, 1, 32,
                               len(png), offset)
        blobs += png
        offset += len(png)
    return header + entries + blobs


def preview(canvas, size):
    """目で見られないので、形だけ文字で確かめる。"""
    ramp = ' .:-=+*#%@'
    out = []
    for py in range(0, size, max(1, size // 24)):
        row = ''
        for px in range(0, size, max(1, size // 48)):
            a = canvas[py * size + px][3]
            row += ramp[min(len(ramp) - 1, int(a * len(ramp)))]
        out.append(row)
    return '\n'.join(out)


if __name__ == '__main__':
    out_dir = sys.argv[1].rstrip('/')
    logo = render(96)
    open(out_dir + '/logo.png', 'wb').write(to_png(logo, 96))
    icons = [(size, to_png(render(size), size)) for size in (16, 32, 48)]
    open(out_dir + '/favicon.ico', 'wb').write(to_ico(icons))
    print(preview(render(48), 48))
    print('logo.png (96x96), favicon.ico (16/32/48) を %s に書きました' % out_dir)
