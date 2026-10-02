#!/usr/bin/env python3
"""Generate JumiTerm's 4x8 CP437 font.

Glyphs are 8 hex digits, one per pixel row (top first). In each digit bit 3
is the leftmost pixel and bit 0 the rightmost. Letters are 3 pixels wide in
columns 0-2; column 3 is the gap between characters. Box-drawing, block and
shade characters are generated and use all 4 columns so they join up.

Output is a C array in the blitter's 'pair' format: 2 bytes per row, high
nibble = left pixel of the pair, value 1 = ink, 0 = transparent.

  python3 tools/mkfont.py [out.c] [preview.png]
"""
import os
import sys

# 0x20 .. 0x7E
ASCII = [
    "00000000", "04444040", "0AA00000", "0AEAEA00", "046842C4", "0A2448A0", "04A4AA60", "04400000",  # space ! " # $ % & '
    "02444420", "08444480", "00A4E4A0", "0004E400", "00000048", "0000E000", "00000040", "02244880",  # ( ) * + , - . /
    "0EAAAAE0", "04C444E0", "0C2248E0", "0C2422C0", "0AAAE220", "0E8C22C0", "068EAAE0", "0E224440",  # 0-7
    "0EAEAAE0", "0EAAE2C0", "00400400", "00400480", "02484200", "000E0E00", "08424800", "0E224040",  # 8 9 : ; < = > ?
    "04AEE860", "04AAEAA0", "0CACAAC0", "06888860", "0CAAAAC0", "0E8E88E0", "0E8E8880", "0688AA60",  # @ A-G
    "0AAEAAA0", "0E4444E0", "02222A40", "0AACCAA0", "088888E0", "0AEEAAA0", "0CAAAAA0", "04AAAA40",  # H-O
    "0CAAC880", "04AAAA42", "0CAACAA0", "068422C0", "0E444440", "0AAAAAE0", "0AAAA440", "0AAAEEA0",  # P-W
    "0AA44AA0", "0AAA4440", "0E2448E0", "06444460", "08844220", "0C4444C0", "04A00000", "0000000E",  # X Y Z [ \ ] ^ _
    "08400000", "0006AA60", "088CAAC0", "00068860", "0226AA60", "0006AC60", "024E4440", "0006A62C",  # ` a-g
    "088CAAA0", "040C44E0", "020222A4", "088ACAA0", "0C4444E0", "000EEAA0", "000CAAA0", "0004AA40",  # h-o
    "000CAC88", "0006A622", "00068880", "000682C0", "004E4460", "000AAA60", "000AAA40", "000AAEE0",  # p-w
    "000A44A0", "000AA62C", "000E24E0", "06484460", "04444440", "0C4244C0", "0005A000",              # x y z { | } ~
]

DOTLESS_I = "000C44E0"

# Accent marks for rows 0-2, placed above a lowercase letter (rows 3-7).
ACCENTS = {"acute": "240", "grave": "840", "circ": "4A0", "uml": "0A0", "ring": "4A4", "tilde": "5A0"}

COMPOSED = {
    0x81: ("u", "uml"), 0x82: ("e", "acute"), 0x83: ("a", "circ"), 0x84: ("a", "uml"),
    0x85: ("a", "grave"), 0x86: ("a", "ring"), 0x88: ("e", "circ"), 0x89: ("e", "uml"),
    0x8A: ("e", "grave"), 0x8B: ("dotless_i", "uml"), 0x8C: ("dotless_i", "circ"),
    0x8D: ("dotless_i", "grave"), 0x93: ("o", "circ"), 0x94: ("o", "uml"), 0x95: ("o", "grave"),
    0x96: ("u", "circ"), 0x97: ("u", "grave"), 0x98: ("y", "uml"), 0xA0: ("a", "acute"),
    0xA1: ("dotless_i", "acute"), 0xA2: ("o", "acute"), 0xA3: ("u", "acute"), 0xA4: ("n", "tilde"),
}

EXPLICIT = {
    0x80: "06888864", 0x87: "00068864", 0x8E: "A04AEAA0", 0x8F: "4A4AEAA0", 0x90: "20E8E8E0",
    0x91: "0006BE70", 0x92: "07AFAAB0", 0x99: "A04AAA40", 0x9A: "A0AAAAE0", 0x9B: "004E8E40",
    0x9C: "068E88E0", 0x9D: "0AAE4E40", 0x9E: "0CAC8B8B", 0x9F: "0244E448",
    0xA5: "5ACAAAA0", 0xA6: "06A60E00", 0xA7: "04A40E00", 0xA8: "040488E0", 0xA9: "000E8000",
    0xAA: "000E2000", 0xAB: "88A24C26", 0xAC: "88A24AE2", 0xAD: "04044440", 0xAE: "0005A500",
    0xAF: "000A5A00",
    0xB0: "80208020", 0xB1: "A5A5A5A5", 0xB2: "7FDF7FDF",
    0xDB: "FFFFFFFF", 0xDC: "0000FFFF", 0xDD: "CCCCCCCC", 0xDE: "33333333", 0xDF: "FFFF0000",
    0xE0: "0005AA50", 0xE1: "04ACAAC8", 0xE2: "0E888880", 0xE3: "000EAAA0", 0xE4: "0E8448E0",
    0xE5: "0007AA40", 0xE6: "000AAAC8", 0xE7: "000E4420", 0xE8: "04EAAE40", 0xE9: "04AEAA40",
    0xEA: "04AAA4A0", 0xEB: "0684AA40", 0xEC: "0000EAE0", 0xED: "004EAE40", 0xEE: "0006C860",
    0xEF: "004AAAA0", 0xF0: "00E0E0E0", 0xF1: "004E40E0", 0xF2: "084248E0", 0xF3: "024842E0",
    0xF4: "02444444", 0xF5: "44444480", 0xF6: "0040E040", 0xF7: "0005A5A0", 0xF8: "04A40000",
    0xF9: "00006600", 0xFA: "00004000", 0xFB: "03222A40", 0xFC: "0CAA0000", 0xFD: "0C24E000",
    0xFE: "00666600", 0xFF: "00000000",
}

# (up, down, left, right): 0 none, 1 single line, 2 double line.
BOX = {
    0xB3: (1, 1, 0, 0), 0xB4: (1, 1, 1, 0), 0xB5: (1, 1, 2, 0), 0xB6: (2, 2, 1, 0),
    0xB7: (0, 2, 1, 0), 0xB8: (0, 1, 2, 0), 0xB9: (2, 2, 2, 0), 0xBA: (2, 2, 0, 0),
    0xBB: (0, 2, 2, 0), 0xBC: (2, 0, 2, 0), 0xBD: (2, 0, 1, 0), 0xBE: (1, 0, 2, 0),
    0xBF: (0, 1, 1, 0), 0xC0: (1, 0, 0, 1), 0xC1: (1, 0, 1, 1), 0xC2: (0, 1, 1, 1),
    0xC3: (1, 1, 0, 1), 0xC4: (0, 0, 1, 1), 0xC5: (1, 1, 1, 1), 0xC6: (1, 1, 0, 2),
    0xC7: (2, 2, 0, 1), 0xC8: (2, 0, 0, 2), 0xC9: (0, 2, 0, 2), 0xCA: (2, 0, 2, 2),
    0xCB: (0, 2, 2, 2), 0xCC: (2, 2, 0, 2), 0xCD: (0, 0, 2, 2), 0xCE: (2, 2, 2, 2),
    0xCF: (1, 0, 2, 2), 0xD0: (2, 0, 1, 1), 0xD1: (0, 1, 2, 2), 0xD2: (0, 2, 1, 1),
    0xD3: (2, 0, 0, 1), 0xD4: (1, 0, 0, 2), 0xD5: (0, 1, 0, 2), 0xD6: (0, 2, 0, 1),
    0xD7: (2, 2, 1, 1), 0xD8: (1, 1, 2, 2), 0xD9: (1, 0, 1, 0), 0xDA: (0, 1, 0, 1),
}


def box_glyph(up, down, left, right):
    """Single lines use column 1 / row 3; double lines columns 0,2 / rows 2,4."""
    px = [[0] * 4 for _ in range(8)]
    dv = up == 2 or down == 2
    dh = left == 2 or right == 2

    def xs(style):
        return (0, 2) if style == 2 else (1,) if style == 1 else ()

    def ys(style):
        return (2, 4) if style == 2 else (3,) if style == 1 else ()

    for x in xs(up):
        for y in range(0, (4 if dh else 3) + 1):
            px[y][x] = 1
    for x in xs(down):
        for y in range(2 if dh else 3, 8):
            px[y][x] = 1
    for y in ys(left):
        for x in range(0, (2 if dv else 1) + 1):
            px[y][x] = 1
    for y in ys(right):
        for x in range(0 if dv else 1, 4):
            px[y][x] = 1
    return "".join("%X" % (r[0] << 3 | r[1] << 2 | r[2] << 1 | r[3]) for r in px)


def build():
    glyphs = ["00000000"] * 256
    for i, glyph in enumerate(ASCII):
        glyphs[0x20 + i] = glyph
    for code, (base, accent) in COMPOSED.items():
        b = DOTLESS_I if base == "dotless_i" else ASCII[ord(base) - 0x20]
        assert b[:3] == "000", "accent base must leave rows 0-2 empty"
        glyphs[code] = ACCENTS[accent] + b[3:]
    for code, glyph in EXPLICIT.items():
        glyphs[code] = glyph
    for code, arms in BOX.items():
        glyphs[code] = box_glyph(*arms)
    return glyphs


def to_pairs(glyph):
    out = []
    for digit in glyph:
        v = int(digit, 16)
        px = [(v >> 3) & 1, (v >> 2) & 1, (v >> 1) & 1, v & 1]
        out.append(px[0] << 4 | px[1])
        out.append(px[2] << 4 | px[3])
    return out


def write_c(glyphs, path):
    lines = [
        "/* Generated by tools/mkfont.py - do not edit. 4x8 CP437, blitter pair format. */",
        "#include <stdint.h>",
        '#include "font.h"',
        "",
        "const uint8_t font_data[256 * 16] = {",
    ]
    for code, glyph in enumerate(glyphs):
        data = ", ".join("0x%02X" % b for b in to_pairs(glyph))
        lines.append("    %s,  /* %02X */" % (data, code))
    lines.append("};")
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


SAMPLES = [
    bytes([0xC9, 0xCD, 0xCD, 0xCB, 0xCD, 0xBB, 0x20, 0xDA, 0xC4, 0xC2, 0xC4, 0xBF]),
    bytes([0xBA, 0x41, 0x42, 0xBA, 0x43, 0xBA, 0x20, 0xB3, 0x61, 0xB3, 0x62, 0xB3]),
    bytes([0xCC, 0xCD, 0xCD, 0xCE, 0xCD, 0xB9, 0x20, 0xC3, 0xC4, 0xC5, 0xC4, 0xB4]),
    bytes([0xC8, 0xCD, 0xCD, 0xCA, 0xCD, 0xBC, 0x20, 0xC0, 0xC4, 0xC1, 0xC4, 0xD9]),
    b"",
    bytes([0xB0, 0xB0, 0xB1, 0xB1, 0xB2, 0xB2, 0xDB, 0xDB, 0xDC, 0xDF, 0xDD, 0xDE]),
    b"The quick brown fox jumps over",
    b"the lazy dog. 0123456789 (){}[]",
    bytes("Hyv\x84\x84 p\x84iv\x84\x84! \x8e\x99\x86\x94\x81\x82", "latin-1"),
]


def write_preview(glyphs, path, scale=4):
    try:
        from PIL import Image
    except ImportError:
        print("Pillow not installed; skipping preview")
        return
    cols, rows = 16 * 5, 16 * 9 + 2 + len(SAMPLES) * 8
    img = Image.new("RGB", (cols * scale, rows * scale), (60, 60, 60))

    def plot(code, ox, oy):
        for y, digit in enumerate(glyphs[code]):
            v = int(digit, 16)
            for x in range(4):
                colour = (255, 255, 255) if v & (8 >> x) else (0, 0, 0)
                for dy in range(scale):
                    for dx in range(scale):
                        img.putpixel(((ox + x) * scale + dx, (oy + y) * scale + dy), colour)

    for code in range(256):
        plot(code, (code % 16) * 5, (code // 16) * 9)
    for line, sample in enumerate(SAMPLES):
        for i, code in enumerate(sample[:20]):
            plot(code, i * 4, 16 * 9 + 2 + line * 8)
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    img.save(path)


def main(argv):
    c_path = argv[1] if len(argv) > 1 else "src/font_data.c"
    png_path = argv[2] if len(argv) > 2 else None
    glyphs = build()
    write_c(glyphs, c_path)
    if png_path:
        write_preview(glyphs, png_path)


if __name__ == "__main__":
    main(sys.argv)
