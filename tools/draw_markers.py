"""Draw the three inventory marks and pack them in Gothic's DXT3 texture format."""

from io import BytesIO
from pathlib import Path
from struct import pack

from PIL import Image, ImageDraw


SIZE = 32
SCALE = 4
TEXTURES = Path(__file__).resolve().parents[1] / "textures"


def make_badge(kind, rim, highlight):
    canvas = Image.new("RGBA", (SIZE * SCALE, SIZE * SCALE))
    draw = ImageDraw.Draw(canvas)

    def polygon(points, fill):
        draw.polygon([(x * SCALE, y * SCALE) for x, y in points], fill=fill)

    def line(points, fill, width):
        draw.line([(x * SCALE, y * SCALE) for x, y in points], fill=fill, width=width * SCALE, joint="curve")

    polygon([(8, 2), (24, 2), (30, 8), (30, 24), (24, 30), (8, 30), (2, 24), (2, 8)], "#0b0c08")
    polygon([(8, 3), (24, 3), (29, 8), (29, 24), (24, 29), (8, 29), (3, 24), (3, 8)], rim)
    polygon([(9, 5), (23, 5), (27, 9), (27, 23), (23, 27), (9, 27), (5, 23), (5, 9)], "#24261a")

    if kind == "LOCKED":
        line([(12, 15), (12, 11), (14, 9), (18, 9), (20, 11), (20, 15)], highlight, 2)
        polygon([(10, 14), (22, 14), (22, 23), (10, 23)], highlight)
        line([(16, 17), (16, 20)], "#3b261a", 2)
    elif kind == "UNREAD":
        polygon([(8, 10), (12, 10), (16, 12), (20, 10), (24, 10), (24, 21),
                 (20, 21), (16, 23), (12, 21), (8, 21)], highlight)
        line([(16, 12), (16, 22)], "#6b4b27", 1)
        line([(10, 14), (14, 15)], "#876536", 1)
        line([(18, 15), (22, 14)], "#876536", 1)
    else:
        line([(9, 16), (14, 21), (23, 11)], highlight, 3)

    return canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def write_texture(kind, rim, highlight):
    image = make_badge(kind, rim, highlight)
    dds = BytesIO()
    image.save(dds, "DDS", pixel_format="DXT3")
    data = dds.getvalue()
    assert data[84:88] == b"DXT3" and len(data[128:]) == SIZE * SIZE
    header = pack("<4s8I", b"ZTEX", 0, 12, SIZE, SIZE, 1, SIZE, SIZE, 0xFF24261A)
    (TEXTURES / f"G2NRI_{kind}-C.TEX").write_bytes(header + data[128:])


if __name__ == "__main__":
    write_texture("LOCKED", "#8d5a43", "#e0a286")
    write_texture("UNREAD", "#8b6e42", "#e8c58c")
    write_texture("EQUIPPED", "#708054", "#c1d297")
