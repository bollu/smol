#!/usr/bin/env python
# make an atlas from the spleen family of fonts:
# https://raw.githubusercontent.com/fcambus/spleen/master/spleen-32x64.bdf
# https://github.com/farsil/ibmfonts/blob/master/bdf/ib8x16u.bdf
# https://www.dcmembers.com/jibsen/download/61/
from os import waitpid, walk, write
import re
import sys
from typing import List, Dict

# build regex engine?
# need some way to to incrementally build a value.
# x : T := {... }; x := ...; x := ..; x := seal x.
class Bitmap:
    def __init__(self, name:str, encoding: int, width:int, height:int, bitmap:List[List[str]]):
        self.name = name
        self.encoding = encoding
        self.width = width
        self.height = height
        self.bitmap = bitmap

    def __repr__(self):
        bitmap_str = "\n".join(self.bitmap)
        print(f"{self.name} {self.width}x{self.height}:\n {bitmap_str}")

# 0003C000
# read as (00) (03) (C0) (00)
# each pixel is 0-255.
# 256 = ^8 = (2^4)^2 = 2 hex characters per pixel.
def read_hex(raw_str: str) -> List[int]:
    assert len(raw_str) % 2 == 0
    out = []
    while raw_str:
        raw_hex = raw_str[:2]
        h = int(f"0x{raw_hex}", 16)
        out.append(h)
        raw_str = raw_str[2:]
    return out

def argparse(PATH):
    if len(sys.argv) != 2:
        print("usage: mk-atlas.py <path/to/spleen-font.bdf>")
        print(f"using default font: {PATH}")
    else:
        PATH = sys.argv[1]
    return PATH


def fileparse(f):
    lines = [l.strip() for l in f.readlines()]

    cur_x = 0;
    cur_y = 0;
    i = 0
    BITMAPS = []
    RE_BBX = re.compile("BBX ([-]?[0-9]+) ([-]?[0-9]+) ([-]?[0-9]+) ([-]?[0-9]+)")
    RE_ENCODING = re.compile("ENCODING ([0-9]+)")
    while i < len(lines):
        re_name = re.fullmatch("STARTCHAR (.*)", lines[i]); i += 1
        if not re_name: continue

        NAME = re_name[1];
        print(f"NAME: {NAME}")
        assert NAME

        re_encoding = re.fullmatch(RE_ENCODING, lines[i]); i += 1
        assert re_encoding, f"expected to find matching ENCODING for STARTCHAR {re_name}"
        ENCODING = int(re_encoding[1])
        print(f"  encoding: {ENCODING}")


        while i < len(lines):
            re_bbx = re.fullmatch(RE_BBX, lines[i])
            if not re_bbx: i += 1; continue
            else: break
        assert re_bbx, f"expected to find matching BBX for STARTCHAR {re_name}"
        WIDTH = int(re_bbx[1])
        HEIGHT = int(re_bbx[2])
        print(f"  WIDTHxHEIGHT: {WIDTH}x{HEIGHT}")

        i += 1
        assert lines[i]  == "BITMAP"
        i += 1
        # argparse bitmap until 
        BITMAP = []
        for _ in range(HEIGHT):
            row = read_hex(lines[i])
            assert len(row) == (WIDTH + 8 - 1)//8
            BITMAP.append(row)
            i += 1
        print("  " + "\n  ".join(map(str, BITMAP)))
        print("---")
        BITMAPS.append(Bitmap(NAME, ENCODING, WIDTH, HEIGHT, BITMAP))
    return BITMAPS

class Rect:
    def __init__(self, x, y, w, h):
        self.x = x
        self.y = y
        self.w = w
        self.h = h

class Atlas:
    def __init__(self, width: int, height: int, bitmaps: List[Bitmap], bitmap2rect: Dict[str, Rect], atlas:List[List[str]]):
        self.width = width
        self.height = height
        self.bitmaps = bitmaps
        self.bitmap2rect = bitmap2rect
        self.atlas = atlas



def make_atlas(BITMAPS: List[Bitmap], ATLAS_WIDTH: int, ATLAS_HEIGHT: int, BITMAP_HEIGHT: int) -> Atlas:
    # vvv HACK! This is because for whatever reason, though 
    # the bitmap width is 32, the grid only contains 4 hex values!!
    # ===
    # STARTCHAR EQUALS SIGN
    # ENCODING 61
    # SWIDTH 500 0
    # DWIDTH 32 0
    # BBX 32 64 0 -12
    # BITMAP
    # 00000000
    # 00000000
    # 00000000
    # ==
    # assert ATLAS_WIDTH % MAX_BITMAP_WIDTH == 0
    # bitmaps_per_x = ATLAS_WIDTH // MAX_BITMAP_WIDTH
    # atlas_num_bitmaps_in_y = (len(BITMAPS) + bitmaps_per_x - 1) // bitmaps_per_x
    # ATLAS_HEIGHT = atlas_num_bitmaps_in_y * BITMAP_HEIGHT

    # atlas[y][x]
    # atlas = [["ATLAS_NONE" for _ in range(ATLAS_WIDTH)] for _ in range(ATLAS_HEIGHT)]
    atlas = [[0 for _ in range(ATLAS_WIDTH)] for _ in range(ATLAS_HEIGHT)]
    bitmap2rect = {}

    (x, y) = (0, 0) # for filling in atlas
    for (count, b) in enumerate(BITMAPS):
        print("writing bitmap %s/%s" % (count, len(BITMAPS)), file=sys.stderr)
        # vvv HACK on the width!
        # assert x + MAX_BITMAP_WIDTH <= ATLAS_WIDTH
        assert len(b.bitmap) == BITMAP_HEIGHT

        if x + len(b.bitmap[0])*8 >= ATLAS_WIDTH: 
            x = 0
            y += BITMAP_HEIGHT
        assert x + len(b.bitmap[0])*8 < ATLAS_WIDTH
        assert y + BITMAP_HEIGHT <= ATLAS_HEIGHT


        bitmap2rect[b.name] = Rect(x, y, len(b.bitmap[0])*8, BITMAP_HEIGHT)

        for dy in range(BITMAP_HEIGHT):
            assert len(b.bitmap[0]) == (b.width + 8 - 1) // 8
            assert x + len(b.bitmap[0])*8 <= ATLAS_WIDTH
            assert len(b.bitmap[0]) == len(b.bitmap[dy])

            for dx in range(len(b.bitmap[0])*8):
                hexval = int(b.bitmap[dy][dx//8]);
                bitix = 7 - dx % 8 # endian-ness issues.
                bitval = bool(hexval & (1 << bitix))
                out = 255 * int(bitval)
                atlas[y + dy][x + dx] = out;
        x += len(b.bitmap[0])*8
        # y += BITMAP_HEIGHT

        # if x == ATLAS_WIDTH:
        #     x = 0
        #     y += BITMAP_HEIGHT
    return Atlas(ATLAS_WIDTH, ATLAS_HEIGHT, BITMAPS, bitmap2rect, atlas)

def serialize_atlas(ATLAS: Atlas) -> str:
    # NOTE: the atlas contains values is 0f, ce, etc. we need to add an 0x prefix
    # to have C process it as hex.
    out = ""

    out += "static const int atlas_text_width = %s;\n" % (ATLAS.bitmaps[0].width, )
    out += "static const int atlas_text_height = %s;\n\n" % (ATLAS.bitmaps[0].height, )
    out += "enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };\n"
    out += "enum { ATLAS_WIDTH = %s, ATLAS_HEIGHT = %s };\n" % (ATLAS.width, ATLAS.height)
    
    # static unsigned char atlas_texture[ATLAS_WIDTH * ATLAS_HEIGHT] = {
    # 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    # 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    print("serializing texture data...", file=sys.stderr)
    texture_data = ["ATLAS_TEXTURE_NONE" for _ in range(ATLAS.width * ATLAS.height)]
    for y in range(ATLAS.height):
        for x in range(ATLAS.width):
            texture_data[x + y * (ATLAS.width)] = ATLAS.atlas[y][x]


    print("writing texture data...", file=sys.stderr)
    out += "static unsigned char atlas_texture[] = {\n";
    for i in range(ATLAS.width * ATLAS.height):
        out += str(texture_data[i]) +  ", "
        if i % 12 == 0: out += "\n  "
    out += "\n};\n"

    # static mu_Rect atlas[] = {
    #   [ MU_ICON_CLOSE ] = { 88, 68, 16, 16 },
    #   [ MU_ICON_CHECK ] = { 0, 0, 18, 18 },
    #   [ MU_ICON_EXPANDED ] = { 118, 68, 7, 5 },
    #   [ MU_ICON_COLLAPSED ] = { 113, 68, 5, 7 },
    #   [ ATLAS_WHITE ] = { 125, 68, 3, 3 },
    #   [ ATLAS_FONT+32 ] = { 84, 68, 2, 17 },
    print("writing atlas index data...", file=sys.stderr)
    out += "\n";
    out += "static mu_Rect atlas[] = {\n";
    for b in ATLAS.bitmaps:
        r = ATLAS.bitmap2rect[b.name]
        # if b.encoding >= 32 and b.encoding <= 126:
        out += "  [ATLAS_FONT + %s] = { %s, %s, %s, %s},\n" % (b.encoding, r.x, r.y, r.w, r.h)

        if b.encoding == ord('x'):
            out += "  [MU_ICON_CLOSE] = { %s, %s, %s, %s },\n" % (r.x, r.y, r.w, r.h)

        if b.encoding == ord('@'):
            out += "  [MU_ICON_CHECK] = { %s, %s, %s, %s },\n" % (r.x, r.y, r.w, r.h)

        if b.encoding == ord('v'):
            out += "  [MU_ICON_EXPANDED] = { %s, %s, %s, %s },\n" % (r.x, r.y, r.w, r.h)

        if b.encoding == ord('>'):
            out += "  [MU_ICON_COLLAPSED] = { %s, %s, %s, %s },\n" % (r.x, r.y, r.w, r.h)

        # TODO: make an actual white color
        if b.name == "ALLWHITE": # TODO: this is special for font Dina _i40010. Do something similar for all fonts.
            out += "  [ATLAS_WHITE] = { %s, %s, %s, %s },\n" % (r.x, r.y, r.w, r.h)


    out += "};\n";
    return out


def filter_bitmaps(BITMAPS: List[Bitmap]) -> List[Bitmap]:
    """only keep bitmaps that are useful"""
    out = []
    # https://en.wikipedia.org/wiki/Mathematical_operators_and_symbols_in_Unicode
    for b in BITMAPS:
        if b.encoding >= 32 and b.encoding <= 256:
            out.append(b)
        # Mathematical Operators block
        if b.encoding >= 0x2200 and b.encoding <= 0x22FF:
            out.append(b)
        # Supplemental Mathematical Operators block
        if b.encoding >= 0x2A00 and b.encoding <= 0x2AFF:
            out.append(b)
        # Mathematical alphanumeric symbols
        if b.encoding >= 0x1D400 and b.encoding <= 0x1D7FF:
            out.append(b)
        # Letterlike
        if b.encoding >= 0x2100 and b.encoding <= 0x214F:
            out.append(b)
        # Misc. A
        if b.encoding >= 0x27C0 and b.encoding <= 0x27EF:
            out.append(b)
        # Misc. B
        if b.encoding >= 0x2980 and b.encoding <= 0x29FF:
            out.append(b)
        # Misc. Tech
        if b.encoding >= 0x2300 and b.encoding <= 0x23FF:
            out.append(b)
        # Geometric shapes
        if b.encoding >= 0x25A0 and b.encoding <= 0x25FF:
            out.append(b)
        # Misc. Symbols and arrows
        if b.encoding >= 0x2B00 and b.encoding <= 0x2BFF:
            out.append(b)
        # Arrows
        if b.encoding >= 0x2190 and b.encoding <= 0x21FF:
            out.append(b)
        # supplemental arrows: A block
        if b.encoding >= 0x27F0 and b.encoding <= 0x27FF:
            out.append(b)
        # supplemental arrows: B block
        if b.encoding >= 0x2900 and b.encoding <= 0x297F:
            out.append(b)
        # combining diacritical marks
        if b.encoding >= 0x20D0 and b.encoding <= 0x20DC:
            out.append(b)
        # combining diacritical marks
        if b.encoding == 0x20E1: out.append(b)
        if b.encoding == 0x20E5 and b.encoding <= 0x20E6: out.append(b)
        if b.encoding == 0x20EB and b.encoding <= 0x20EF: out.append(b)
        # Arabic math symbols (not included).
    return out


def make_all_white_bitmap(w, h):
    name = "ALLWHITE"
    encoding = -1
    assert w % 8 == 0
    return Bitmap(name, encoding, w, h, [[255]*(w//8)]*h)


if __name__ == "__main__":
    # MAX_BITMAP_WIDTH = 8
    # ATLAS_WIDTH = MAX_BITMAP_WIDTH
    # BITMAP_HEIGHT = 16
    # PATH = argparse("Dina_i400-10.bdf")

    # MAX_BITMAP_WIDTH = 8
    # ATLAS_WIDTH = MAX_BITMAP_WIDTH
    # BITMAP_HEIGHT = 16
    # PATH = argparse("spleen-8x16.bdf")

    # ATLAS_WIDTH = 32
    # ATLAS_HEIGHT = 4096
    # BITMAP_HEIGHT = 16
    # PATH = argparse("Dina_r700-10.bdf")


    ATLAS_WIDTH = (1 << 10)
    ATLAS_HEIGHT = (1 << 9)
    BITMAP_HEIGHT = 16
    PATH = argparse("unifont-14.0.01.bdf")

    assert PATH
    print("parsing bitmaps...", file=sys.stderr)
    with open(PATH) as f:
        BITMAPS = fileparse(f)
    # for b in BITMAPS:
    #     assert b.width == MAX_BITMAP_WIDTH, f"{b.name} has unexpected width {b.width}; expected {BITMAP_WIDTH}"
    #     assert b.height == BITMAP_HEIGHT, f"{b.name} has unexpected height {b.height}; expected {BITMAP_HEIGHT}"
    print("filtering bitmaps..", file=sys.stderr)
    BITMAPS = filter_bitmaps(BITMAPS)
    print("num bitmaps: %s" % len(BITMAPS), file=sys.stderr) 
    BITMAPS.append(make_all_white_bitmap(8, BITMAP_HEIGHT))
    ATLAS = make_atlas(BITMAPS, ATLAS_WIDTH, ATLAS_HEIGHT, BITMAP_HEIGHT)
    OUT = serialize_atlas(ATLAS)
    # with open("../microui/atlas.inl", "w") as f:
    with open("atlas.inl", "w") as f:
        f.write(OUT)
    print("copy atlas.inl to ../microui/atlas.inl")

