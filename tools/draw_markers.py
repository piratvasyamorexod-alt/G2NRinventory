"""Draw the inventory marks and tabs in Gothic's DXT3 texture format."""

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
    elif kind in ("UNREAD", "READ"):
        polygon([(8, 10), (12, 10), (16, 12), (20, 10), (24, 10), (24, 21),
                 (20, 21), (16, 23), (12, 21), (8, 21)], highlight)
        line([(16, 12), (16, 22)], "#6b4b27", 1)
        line([(10, 14), (14, 15)], "#876536", 1)
        line([(18, 15), (22, 14)], "#876536", 1)
        if kind == "READ":
            line([(14, 19), (17, 22), (24, 15)], "#24261a", 4)
            line([(14, 19), (17, 22), (24, 15)], "#c1d297", 2)
    else:
        line([(9, 16), (14, 21), (23, 11)], highlight, 3)

    return canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def make_tab(kind):
    canvas = Image.new("RGBA", (SIZE * SCALE, SIZE * SCALE))
    draw = ImageDraw.Draw(canvas)
    gold, bronze, dark = "#e2c18b", "#ad8450", "#584126"

    def bounds(box):
        return tuple(v * SCALE for v in box)

    def polygon(points, fill=gold):
        draw.polygon([(x * SCALE, y * SCALE) for x, y in points], fill=fill)

    def line(points, fill=gold, width=2):
        draw.line([(x * SCALE, y * SCALE) for x, y in points], fill=fill, width=width * SCALE, joint="curve")

    if kind == "RECENT":
        draw.ellipse(bounds((4, 4, 28, 28)), fill=dark, outline=gold, width=2 * SCALE)
        line([(16, 8), (16, 16), (22, 19)])
    elif kind == "ALL":
        for x, y in ((5, 5), (17, 5), (5, 17), (17, 17)):
            draw.rectangle(bounds((x, y, x + 9, y + 9)), fill=bronze, outline=gold, width=SCALE)
    elif kind == "WEAPONS":
        polygon([(5, 4), (10, 5), (24, 20), (26, 18), (28, 20), (25, 23), (28, 26),
                 (26, 28), (23, 25), (20, 28), (18, 26), (20, 23), (5, 9)], bronze)
        polygon([(27, 4), (27, 9), (12, 23), (14, 26), (12, 28), (9, 25), (6, 28),
                 (4, 26), (7, 23), (4, 20), (6, 18), (9, 20), (23, 5)])
        line([(24, 8), (10, 22)], dark, 1)
    elif kind == "ARMOR":
        polygon([(16, 3), (27, 7), (26, 19), (22, 25), (16, 29), (10, 25), (6, 19), (5, 7)])
        polygon([(16, 6), (24, 9), (23, 18), (20, 23), (16, 26), (12, 23), (9, 18), (8, 9)], dark)
        line([(16, 8), (16, 23)], bronze)
        line([(10, 13), (22, 13)], bronze)
    elif kind == "AMMO":
        line([(6, 26), (25, 7)], gold, 2)
        polygon([(28, 4), (17, 7), (25, 15)])
        polygon([(5, 21), (10, 16), (10, 22), (16, 22), (11, 27), (5, 27)], bronze)
    elif kind == "MAGIC":
        polygon([(16, 2), (27, 10), (28, 22), (16, 30), (4, 22), (5, 10)], bronze)
        polygon([(16, 5), (24, 11), (25, 21), (16, 27), (7, 21), (8, 11)], dark)
        polygon([(16, 8), (18, 14), (24, 16), (18, 18), (16, 24), (14, 18), (8, 16), (14, 14)])
    elif kind == "POTIONS":
        polygon([(11, 4), (21, 4), (21, 8), (19, 8), (19, 13), (24, 17), (26, 24),
                 (23, 28), (9, 28), (6, 24), (8, 17), (13, 13), (13, 8), (11, 8)])
        polygon([(15, 8), (17, 8), (17, 14), (22, 18), (24, 24), (22, 26), (10, 26),
                 (8, 24), (10, 18), (15, 14)], dark)
        polygon([(10, 20), (14, 19), (18, 21), (22, 20), (23, 24), (21, 26), (11, 26), (9, 24)], "#a8bd7c")
        line([(12, 18), (11, 22)], "#f1d5a2", 1)
    elif kind == "SUPPLIES":
        draw.rounded_rectangle(bounds((3, 8, 29, 24)), radius=6 * SCALE, fill=bronze, outline=gold, width=2 * SCALE)
        for x in (9, 15, 21):
            line([(x + 2, 11), (x, 16)], dark, 2)
    elif kind == "DOCUMENTS":
        polygon([(7, 7), (26, 7), (24, 11), (24, 22), (20, 26), (3, 26), (7, 22)], bronze)
        draw.rounded_rectangle(bounds((6, 5, 28, 10)), radius=2 * SCALE, fill=gold)
        draw.rounded_rectangle(bounds((3, 23, 23, 27)), radius=2 * SCALE, fill=gold)
        for y in (13, 17, 21):
            line([(10, y), (20, y)], dark, 1)
    elif kind == "OTHER":
        polygon([(10, 4), (22, 4), (19, 11), (25, 17), (27, 23), (24, 27),
                 (8, 27), (5, 23), (7, 17), (13, 11)], bronze)
        line([(11, 11), (21, 11)], gold, 3)
        line([(16, 12), (12, 17), (10, 20)], gold, 1)
    elif kind == "SALE":
        polygon([(18, 4), (28, 4), (28, 14), (14, 28), (4, 18)])
        polygon([(19, 7), (25, 7), (25, 13), (14, 24), (8, 18)], dark)
        draw.ellipse(bounds((20, 8, 24, 12)), fill=gold)
        line([(13, 17), (16, 20)], bronze, 2)

    return canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def write_texture(kind, image):
    dds = BytesIO()
    image.save(dds, "DDS", pixel_format="DXT3")
    data = dds.getvalue()
    assert data[84:88] == b"DXT3" and len(data[128:]) == SIZE * SIZE
    header = pack("<4s8I", b"ZTEX", 0, 12, SIZE, SIZE, 1, SIZE, SIZE, 0xFF24261A)
    (TEXTURES / f"G2NRI_{kind}-C.TEX").write_bytes(header + data[128:])


if __name__ == "__main__":
    for kind, rim, highlight in (
        ("LOCKED", "#8d5a43", "#e0a286"),
        ("UNREAD", "#8b6e42", "#e8c58c"),
        ("READ", "#708054", "#e8c58c"),
        ("EQUIPPED", "#708054", "#c1d297"),
    ):
        write_texture(kind, make_badge(kind, rim, highlight))
    for kind in ("RECENT", "ALL", "WEAPONS", "ARMOR", "AMMO", "MAGIC", "POTIONS", "SUPPLIES", "DOCUMENTS", "OTHER", "SALE"):
        write_texture(f"TAB_{kind}", make_tab(kind))
