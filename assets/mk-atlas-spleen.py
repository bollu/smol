#!/usr/bin/env python
# make an atlas from the spleen family of fonts:
# https://raw.githubusercontent.com/fcambus/spleen/master/spleen-32x64.bdf
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
def read_hex(raw_str: str) -> List[str]:
    assert len(raw_str) % 2 == 0
    out = []
    while raw_str:
        out.append(raw_str[:2])
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
            BITMAP.append(read_hex(lines[i]))
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




def make_atlas(BITMAPS: List[Bitmap], ATLAS_WIDTH: int, BITMAP_WIDTH: int, BITMAP_HEIGHT: int) -> Atlas:
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
    BITMAP_WIDTH = (BITMAPS[0].width + 8 - 1) // 8

    assert ATLAS_WIDTH % BITMAP_WIDTH == 0
    bitmaps_per_x = ATLAS_WIDTH // BITMAP_WIDTH
    # ceiling of total number of bitmaps divided by bitmaps per row
    atlas_num_bitmaps_in_y = (len(BITMAPS) + bitmaps_per_x - 1) // bitmaps_per_x
    ATLAS_HEIGHT = atlas_num_bitmaps_in_y * BITMAP_HEIGHT

    # atlas[y][x]
    atlas = [[0 for _ in range(ATLAS_WIDTH)] for _ in range(ATLAS_HEIGHT)]
    bitmap2rect = {}

    (x, y) = (0, 0) # for filling in atlas
    for b in BITMAPS:
        # vvv HACK on the width!
        bitmap2rect[b.name] = Rect(x, y, BITMAP_WIDTH, BITMAP_HEIGHT)
        assert x + BITMAP_WIDTH <= ATLAS_WIDTH
        assert y + BITMAP_HEIGHT <= ATLAS_HEIGHT
        for dy in range(BITMAP_HEIGHT):
            for dx in range(BITMAP_WIDTH):
                v = b.bitmap[dy][dx]
                atlas[y + dy][x + dx] = v
        x += BITMAP_WIDTH
        if x == ATLAS_WIDTH: x = 0; y += dy
    return Atlas(ATLAS_WIDTH, ATLAS_HEIGHT, BITMAPS, bitmap2rect, atlas)

def serialize_atlas(ATLAS: Atlas) -> str:
    # NOTE: the atlas contains values is 0f, ce, etc. we need to add an 0x prefix
    # to have C process it as hex.
    out = ""

    out += "static const int atlas_text_height = %s;\n\n" % (ATLAS.bitmaps[0].height, )

    out += "enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };\n"
    out += "enum { ATLAS_WIDTH = %s, ATLAS_HEIGHT = %s };\n" % (ATLAS.width, ATLAS.height)
    out += "static unsigned char atlas_texture[ATLAS_WIDTH * ATLAS_HEIGHT] = {\n";
    # static unsigned char atlas_texture[ATLAS_WIDTH * ATLAS_HEIGHT] = {
    # 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    # 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    first_write = True

    for y in range(ATLAS.height):
        for x in range(ATLAS.width):
            if not first_write:
                out += ","
            first_write = False
            out += f"0x{ATLAS.atlas[y][x]}"
    out += "};\n"

    # static mu_Rect atlas[] = {
    #   [ MU_ICON_CLOSE ] = { 88, 68, 16, 16 },
    #   [ MU_ICON_CHECK ] = { 0, 0, 18, 18 },
    #   [ MU_ICON_EXPANDED ] = { 118, 68, 7, 5 },
    #   [ MU_ICON_COLLAPSED ] = { 113, 68, 5, 7 },
    #   [ ATLAS_WHITE ] = { 125, 68, 3, 3 },
    #   [ ATLAS_FONT+32 ] = { 84, 68, 2, 17 },
    out += "\n";
    out += "static mu_Rect atlas[] = {\n";
    for b in ATLAS.bitmaps:
        r = ATLAS.bitmap2rect[b.name]
        if b.encoding >= 32 and b.encoding <= 126:
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
        if b.encoding == ord('*'):
            out += "  [ATLAS_WHITE] = { %s, %s, %s, %s },\n" % (r.x, r.y, r.w, r.h)


    out += "};\n";

    return out


def filter_bitmaps(BITMAPS: List[Bitmap]) -> List[Bitmap]:
    """only keep bitmaps that are useful"""
    out = []
    for b in BITMAPS:
        if b.encoding >= 32 and b.encoding <= 126:
            out.append(b)
    return out

if __name__ == "__main__":
    ATLAS_WIDTH = 128
    BITMAP_WIDTH = 8
    BITMAP_HEIGHT = 16
    PATH = argparse("spleen-8x16.bdf")
    assert PATH
    with open(PATH) as f:
        BITMAPS = fileparse(f)
    for b in BITMAPS:
        assert b.width == BITMAP_WIDTH, f"{b.name} has unexpected width {b.width}; expected {BITMAP_WIDTH}"
        assert b.height == BITMAP_HEIGHT, f"{b.name} has unexpected height {b.height}; expected {BITMAP_HEIGHT}"
    BITMAPS = filter_bitmaps(BITMAPS)
    ATLAS = make_atlas(BITMAPS, ATLAS_WIDTH, BITMAP_WIDTH, BITMAP_HEIGHT)
    OUT = serialize_atlas(ATLAS)
    # with open("../microui/atlas.inl", "w") as f:
    with open("atlas.inl", "w") as f:
        f.write(OUT)
    print("copy atlas.inl to ../microui/atlas.inl")

