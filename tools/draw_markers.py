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
    # Use a single ink color, including under transparent texels, so the
    # DXT3 color blocks cannot introduce a dark or colored fringe.
    mask = Image.new("L", (SIZE * SCALE, SIZE * SCALE))
    draw = ImageDraw.Draw(mask)
    stroke_width = 2
    radius = stroke_width / 2

    def box(left, top, right, bottom):
        return tuple(round(n * SCALE) for n in (left, top, right, bottom))

    def stroke(points):
        draw.line([(round(x * SCALE), round(y * SCALE)) for x, y in points],
                  fill=255, width=stroke_width * SCALE, joint="curve")
        for x, y in points:
            draw.ellipse(box(x - radius, y - radius, x + radius, y + radius), fill=255)

    def oval(left, top, right, bottom):
        draw.ellipse(box(left, top, right, bottom), outline=255, width=stroke_width * SCALE)

    def fill(points):
        draw.polygon([(round(x * SCALE), round(y * SCALE)) for x, y in points], fill=255)

    if kind == "RECENT":
        oval(4, 4, 28, 28)
        stroke([(16, 10), (16, 16), (21, 13)])
        draw.ellipse(box(24, 4, 28, 8), fill=255)
    elif kind == "ALL":
        for x, y in ((5, 5), (18, 5), (5, 18), (18, 18)):
            draw.rounded_rectangle(box(x, y, x + 9, y + 9), radius=SCALE, outline=255,
                                   width=stroke_width * SCALE)
    elif kind == "WEAPONS":
        # The metal and wooden arcs both face upward. The X sits between them.
        stroke([(5, 9), (10, 6), (16, 5), (22, 6), (27, 9)])
        stroke([(5, 9), (16, 11), (27, 9)])
        stroke([(16, 3), (16, 10)])
        for direction in (-1, 1):
            def side(points):
                return [(16 + direction * x, y) for x, y in points]

            fill(side([(9, 12), (6, 12.5), (-6.5, 19.5), (-5, 22)]))
            stroke(side([(-7, 18.5), (-4, 23)]))
            stroke(side([(-6, 21), (-9, 23)]))
        stroke([(5, 27), (10, 25), (16, 24.5), (22, 25), (27, 27)])
        stroke([(5, 27), (27, 27)])
    elif kind == "ARMOR":
        stroke([(10, 13), (6, 14), (3, 19), (8, 22), (10, 20),
                (10, 25), (16, 28), (22, 25), (22, 20), (24, 22),
                (29, 19), (26, 14), (22, 13)])
        stroke([(16, 4), (12, 6), (12, 11), (16, 14), (20, 11), (20, 6), (16, 4)])
        stroke([(13, 9), (19, 9)])
        stroke([(10, 16), (16, 19), (22, 16)])
        stroke([(16, 19), (16, 24)])
    elif kind == "AMMO":
        stroke([(11, 13), (24, 17), (19, 25), (15, 28), (8, 25), (7, 22), (11, 13)])
        stroke([(10, 17), (22, 21)])
        stroke([(13, 12), (15, 5)])
        stroke([(18, 14), (21, 4)])
        stroke([(23, 16), (27, 7)])
        fill([(15, 4), (11, 7), (11, 11), (15, 9), (17, 6)])
        fill([(21, 3), (17, 6), (17, 11), (21, 8), (23, 5)])
        fill([(27, 6), (23, 8), (22, 13), (26, 11), (29, 8)])
    elif kind == "MAGIC":
        stroke([(21, 9), (21, 5), (6, 5), (4, 7), (4, 9), (19, 9)])
        stroke([(7, 10), (7, 22), (5, 25), (7, 27), (13, 27)])
        stroke([(11, 13), (16, 13)])
        stroke([(11, 17), (13, 17)])
        stroke([(17, 12), (25, 12), (28, 15), (28, 25), (25, 28),
                (17, 28), (14, 25), (14, 15), (17, 12)])
        stroke([(18, 16), (24, 16), (18, 24), (24, 24), (18, 16)])
    elif kind == "POTIONS":
        stroke([(12, 8), (12, 4), (20, 4), (20, 8)])
        stroke([(11, 8), (21, 8)])
        stroke([(13, 8), (13, 12), (8, 15), (5, 20), (5, 23), (8, 27), (12, 28),
                (20, 28), (24, 27), (27, 23), (27, 20), (24, 15), (19, 12), (19, 8)])
        stroke([(6, 21), (11, 19), (17, 21), (22, 20), (26, 20)])
    elif kind == "SUPPLIES":
        stroke([(5, 7), (9, 4), (14, 4), (19, 7), (21, 11), (21, 15), (18, 20),
                (14, 20), (9, 19), (5, 16), (3, 12), (5, 7)])
        stroke([(8, 11), (8, 9), (11, 7)])
        stroke([(16, 20), (21, 24), (22, 27), (25, 28), (27, 25), (29, 24),
                (29, 21), (26, 20), (23, 21), (20, 17)])
    elif kind == "DOCUMENTS":
        stroke([(16, 9), (12, 6), (7, 6), (3, 8), (3, 24), (8, 23), (12, 24),
                (16, 27), (20, 24), (24, 23), (29, 24), (29, 8), (24, 6), (20, 6), (16, 9), (16, 27)])
        for y in (12, 17):
            stroke([(7, y), (10, y), (12, y + 1)])
            stroke([(20, y + 1), (22, y), (25, y)])
    elif kind == "OTHER":
        stroke([(15, 27), (8, 27), (4, 24), (3, 21), (5, 16), (10, 10),
                (6, 5), (10, 4), (19, 4), (22, 5), (17, 10), (19, 12)])
        stroke([(9, 10), (17, 10)])
        oval(15, 13, 23, 21)
        stroke([(22, 20), (28, 26), (25, 29)])
        stroke([(25, 23), (23, 25)])
    elif kind == "SALE":
        stroke([(17, 5), (27, 5), (27, 15), (14, 28), (4, 18), (17, 5)])
        oval(19, 8, 24, 13)

    image = Image.new("RGBA", (SIZE, SIZE), "#c5a573")
    image.putalpha(mask.resize((SIZE, SIZE), Image.Resampling.LANCZOS))
    return image


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
