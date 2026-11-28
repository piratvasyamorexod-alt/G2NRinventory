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

    steel, edge, shade = "#a3a094", "#e0dbca", "#615e53"
    amber, leather, hide = "#c39a60", "#674529", "#3f2a1e"
    paper, crease = "#d7bc8d", "#89663e"

    def oval(box, fill, outline=None, width=1):
        draw.ellipse(bounds(box), fill=fill, outline=outline, width=round(width * SCALE))

    def stroke(points, fill, width=1):
        draw.line([(round(x * SCALE), round(y * SCALE)) for x, y in points],
                  fill=fill, width=round(width * SCALE), joint="curve")

    def arch(left, top, right, drop, fill, width):
        points = [(x, top + drop * ((2 * x - left - right) / (right - left)) ** 2)
                  for x in range(left, right + 1)]
        stroke(points, fill, width)

    if kind == "RECENT":
        polygon([(13, 4), (13, 2), (19, 2), (19, 4)], shade)
        oval((3, 4, 29, 30), hide)
        oval((4, 4, 28, 28), bronze, amber)
        oval((6, 6, 26, 26), paper, "#5b452f")
        stroke([(16, 8), (16, 10)], crease)
        stroke([(16, 22), (16, 24)], crease)
        stroke([(8, 16), (10, 16)], crease)
        stroke([(22, 16), (24, 16)], crease)
        stroke([(11, 12), (16, 16), (21, 11)], hide, 1.6)
        oval((15, 15, 17, 17), bronze)
        oval((24, 5, 29, 10), amber, hide)
        stroke([(25, 6), (27, 6)], gold)
    elif kind == "ALL":
        for x, y in ((5, 5), (17, 5), (5, 17), (17, 17)):
            draw.rectangle(bounds((x, y, x + 9, y + 9)), fill=bronze, outline=gold, width=SCALE)
    elif kind == "WEAPONS":
        # Crossbow above, the sword blades in front, and a separate wooden bow below.
        polygon([(14, 5), (18, 5), (18, 24), (14, 24)], hide)
        stroke([(15, 5), (15, 24)], amber, 1.3)
        arch(4, 5, 28, 4, shade, 2.7)
        arch(4, 4.5, 28, 4, edge, 1.1)
        stroke([(4, 9), (16, 12), (28, 9)], bronze)
        polygon([(16, 1), (18, 5), (14, 5)], edge)
        stroke([(16, 5), (16, 10)], paper)
        for mirror in (True, False):
            def flipped(points):
                return [(32 - x if mirror else x, y) for x, y in points]

            polygon(flipped([(5, 10), (10, 12), (24, 21), (21, 24), (8, 14)]), steel)
            stroke(flipped([(5, 10), (21, 22)]), edge, 1.1)
            stroke(flipped([(20, 25), (25, 20)]), amber, 1.5)
            stroke(flipped([(23, 23), (27, 26)]), leather, 2.4)
            x = 5 if mirror else 27
            oval((x - 1, 25, x + 1, 27), edge)
        arch(3, 26, 29, 4, leather, 2.2)
        arch(3, 25.7, 29, 4, amber, 1.25)
        stroke([(3, 30), (29, 30)], paper, 0.8)
    elif kind == "ARMOR":
        polygon([(13, 10), (20, 10), (22, 13), (26, 13), (29, 20),
                 (25, 23), (22, 22), (23, 27), (16, 30), (9, 27), (10, 22),
                 (7, 23), (3, 20), (6, 13), (10, 13)], shade)
        polygon([(10, 13), (16, 16), (22, 13), (23, 20), (21, 22),
                 (22, 26), (16, 29), (10, 26), (11, 22), (9, 20)], steel)
        polygon([(16, 18), (22, 15), (23, 20), (21, 22), (22, 26), (16, 29)], "#777367")
        stroke([(10, 17), (16, 19), (22, 17)], edge)
        stroke([(16, 19), (16, 27)], edge)
        stroke([(11, 25), (16, 27), (21, 25)], edge)
        polygon([(6, 14), (9, 13), (11, 19), (8, 21), (4, 19)], steel)
        polygon([(23, 13), (26, 14), (28, 19), (24, 21), (21, 19)], steel)
        stroke([(4, 19), (8, 20), (10, 18)], edge)
        stroke([(22, 18), (24, 20), (28, 19)], edge)
        stroke([(11, 13), (12, 17)], amber, 1.4)
        stroke([(21, 13), (20, 17)], amber, 1.4)
        polygon([(11, 6), (12, 4), (16, 2), (20, 4), (21, 6), (21, 11),
                 (16, 15), (11, 11)], steel)
        polygon([(16, 2), (20, 4), (21, 7), (21, 11), (16, 15)], shade)
        stroke([(16, 3), (16, 7)], edge)
        stroke([(12, 9), (16, 10), (20, 9)], "#25251f", 1.5)
        stroke([(12, 11), (16, 14), (20, 11)], edge)
    elif kind == "AMMO":
        for x, y, bottom in ((14, 3, 13), (20, 2, 16), (25, 5, 19)):
            polygon([(x, y), (x - 3, y + 3), (x - 3, y + 8), (x, y + 6),
                     (x + 1, y + 9), (x + 3, y + 4), (x + 2, y + 1)], paper)
            stroke([(x + 1, y + 1), (bottom, 17)], crease, 1.1)
        polygon([(10, 11), (24, 16), (17, 29), (13, 30), (6, 27), (5, 24)], leather)
        polygon([(11, 15), (16, 16), (11, 28), (6, 26)], "#90613a")
        polygon([(17, 16), (24, 16), (17, 29), (13, 30), (14, 27)], hide)
        stroke([(20, 17), (14, 28)], bronze, 0.9)
        polygon([(6, 25), (10, 27), (17, 28), (16, 30), (12, 30), (6, 28)], "#815939")
        oval((9.5, 11.5, 24.5, 17.5), "#36271d", amber, 1.3)
        stroke([(11, 15), (17, 17), (23, 16)], amber, 1.6)
        stroke([(10, 17), (17, 19), (22, 18)], bronze, 1.1)
    elif kind == "MAGIC":
        polygon([(5, 5), (22, 5), (20, 24), (17, 27), (4, 26), (5, 22),
                 (3, 20), (5, 13)], paper)
        draw.rounded_rectangle(bounds((4, 4, 24, 8)), radius=2 * SCALE, fill=amber)
        stroke([(6, 5), (22, 5)], gold)
        stroke([(7, 11), (16, 11)], crease, 0.8)
        stroke([(7, 14), (14, 14)], crease, 0.8)
        oval((6, 19, 11, 24), "#96613f")
        polygon([(17, 12), (25, 11), (29, 15), (29, 25), (26, 29),
                 (17, 29), (14, 25), (14, 16)], "#5c6565")
        polygon([(17, 13), (24, 12), (27, 15), (27, 25), (25, 27), (17, 28),
                 (15, 25), (15, 16)], "#3d4749")
        stroke([(17, 13), (24, 12), (27, 15)], "#a29e88")
        stroke([(18, 16), (25, 16), (18, 25), (25, 25), (18, 16)], "#9cbfc0", 1.4)
    elif kind == "POTIONS":
        draw.rounded_rectangle(bounds((12, 2, 20, 8)), radius=1.5 * SCALE, fill=leather)
        stroke([(13, 3), (19, 3)], amber)
        polygon([(12, 7), (20, 7), (20, 12), (25, 15), (28, 20), (27, 26),
                 (23, 29), (9, 29), (5, 26), (4, 20), (7, 15), (12, 12)], "#8a8c65")
        polygon([(13, 9), (19, 9), (19, 13), (24, 16), (26, 20), (25, 25),
                 (22, 27), (10, 27), (7, 25), (6, 20), (8, 16), (13, 13)], "#3f4935")
        polygon([(7, 19), (12, 18), (20, 18), (25, 19), (25, 24),
                 (22, 27), (10, 27), (7, 24)], "#86905a")
        polygon([(12, 22), (24, 21), (24, 24), (21, 26), (11, 26), (8, 24)], "#596941")
        stroke([(7, 19), (12, 20), (20, 20), (25, 19)], "#b0b783")
        stroke([(11, 7), (21, 7)], amber, 1.6)
        stroke([(11, 15), (8, 17), (7, 21)], "#d0c9a1", 1.2)
        stroke([(23, 25), (21, 27), (16, 28)], paper, 0.8)
    elif kind == "SUPPLIES":
        stroke([(17, 17), (25, 25)], "#a4865f", 5.0)
        oval((22.5, 23.5, 29, 29), paper, crease)
        oval((25, 21, 30, 27), paper, crease)
        stroke([(18, 17), (26, 25)], gold, 2.5)
        polygon([(6, 3), (12, 2), (18, 5), (22, 10), (22, 15), (20, 20),
                 (17, 24), (14, 22), (9, 22), (4, 18), (2, 12), (3, 7)], "#593420")
        polygon([(6, 4), (12, 3), (18, 6), (21, 10), (21, 16), (17, 22),
                 (15, 20), (9, 21), (5, 17), (3, 12), (4, 7)], "#965c34")
        polygon([(6, 6), (11, 4), (16, 6), (19, 10), (17, 13), (10, 15),
                 (5, 12), (4, 9)], "#ba8450")
        stroke([(6, 7), (10, 5), (14, 6)], amber, 1.1)
        stroke([(8, 18), (13, 19), (17, 17)], "#734628", 1.3)
    elif kind == "DOCUMENTS":
        polygon([(2, 9), (8, 6), (16, 8), (24, 6), (30, 9), (30, 25),
                 (23, 25), (16, 28), (9, 25), (2, 25)], hide)
        polygon([(3, 9), (8, 7), (16, 9), (24, 7), (29, 9), (29, 24),
                 (23, 24), (16, 27), (9, 24), (3, 24)], amber)
        polygon([(4, 7), (9, 5), (13, 6), (16, 8), (19, 6), (23, 5), (28, 7),
                 (28, 22), (23, 21), (19, 22), (16, 25), (13, 22), (9, 21),
                 (4, 22)], paper)
        stroke([(4, 7), (9, 5), (13, 6), (16, 8), (19, 6), (23, 5), (28, 7)], gold)
        stroke([(16, 8), (16, 24)], crease, 1.35)
        for y in (10, 13, 16):
            stroke([(6, y), (9, y - 0.5), (13, y + 0.5)], crease, 0.8)
            stroke([(19, y + 0.5), (23, y - 0.5), (26, y)], crease, 0.8)
        polygon([(17, 24), (20, 23), (20, 29), (18.5, 28), (17, 29)], leather)
    elif kind == "OTHER":
        polygon([(5, 5), (9, 3), (21, 3), (24, 6), (20, 10), (24, 15), (26, 22),
                 (24, 27), (18, 29), (9, 29), (4, 27), (2, 23), (4, 16), (9, 10)], hide)
        polygon([(6, 5), (10, 4), (21, 4), (22, 6), (18, 9), (21, 13), (23, 22),
                 (21, 26), (16, 28), (8, 27), (4, 24), (4, 20), (7, 14), (11, 10)], leather)
        polygon([(6, 18), (10, 13), (12, 12), (11, 18), (6, 24), (5, 23)], "#926133")
        stroke([(9, 9), (13, 10), (20, 8)], amber, 1.7)
        stroke([(13, 10), (11, 14), (10, 17)], gold, 1.1)
        oval((15, 12, 23, 20), None, "#382b21", 3)
        oval((15, 12, 23, 20), None, steel, 1.7)
        stroke([(21, 19), (28, 27)], steel, 2.8)
        stroke([(26, 26), (29, 23)], steel, 2.2)
        stroke([(27, 28), (30, 25)], steel, 2.0)
        stroke([(16, 14), (18, 12), (20, 12)], edge, 0.9)
        stroke([(22, 20), (28, 26)], edge, 0.85)
    elif kind == "SALE":
        polygon([(18, 4), (28, 4), (28, 14), (14, 28), (4, 18)])
        polygon([(19, 7), (25, 7), (25, 13), (14, 24), (8, 18)], dark)
        draw.ellipse(bounds((20, 8, 24, 12)), fill=gold)
        stroke([(13, 17), (16, 20)], bronze, 2)

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
