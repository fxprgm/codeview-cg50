"""Convert C47's pixel fonts to BDF files in tools/fonts/.

usage: python3 tools/c47fonts.py <C47 source tree>/res/fonts

C47 (https://gitlab.com/rpncalculators/c43, GPL-3.0) draws its screen with fonts kept as
TrueType outlines on a pixel grid; its build turns them into bitmaps with
src/ttf2RasterFonts/ttf2RasterFonts.c. This script does the same rendering (FreeType at the
font's own pixel size, threshold 128, the same row and column bookkeeping), so the glyphs are
the pixels C47 shows, and writes them as BDF:

  C47-Standard.bdf  C47__StandardFont.ttf, 20 px high, proportional (menus, labels, text)
  C47-Tiny.bdf      C47__TinyFont.ttf, 8 px high, 6 px wide

tools/mkdata.py then places them in fixed-width cells. Needs freetype-py (pip install
freetype-py).
"""
import os, sys
import freetype

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = (('C47__StandardFont.ttf', 'C47-Standard'), ('C47__TinyFont.ttf', 'C47-Tiny'))


def raster(path):
    """{code point: (advance, rows of bits, MSB = leftmost)}, height, ascent"""
    face = freetype.Face(path)
    upem = face.units_per_EM
    one = 50 if upem == 1000 else 32  # as ttf2RasterFonts
    hpx = upem // one
    face.set_pixel_sizes(0, hpx)
    asc, desc = int(face.ascender / one), int(face.descender / one)  # C truncates toward 0
    height = asc - desc
    face.set_transform(freetype.Matrix(0x10000, 0, 0, 0x10000), freetype.FT_Vector(0, (hpx - asc) * 64))
    glyphs = {}
    cp, gi = face.get_first_char()
    while gi:
        if 32 <= cp <= 0x7FFF:
            face.load_glyph(gi, freetype.FT_LOAD_RENDER)
            m, bm = face.glyph.metrics, face.glyph.bitmap
            before, cols = m.horiBearingX // 64, bm.width
            after = m.horiAdvance // 64 - before - cols
            skipc = 0
            if before < 0:
                skipc, cols, before = -before, cols + before, 0
            if after < 0:
                cols, after = cols + after, 0
            above, skipr = asc - m.horiBearingY // 64, 0
            if above < 0:
                skipr, above = -above, 0
            adv = before + cols + after
            rows = []
            for y in range(height):
                bits, yy = 0, y - above + skipr
                for x in range(cols):
                    if 0 <= yy < bm.rows and bm.buffer[yy * bm.pitch + x + skipc] >= 128:
                        bits |= 1 << (adv - 1 - before - x)
                rows.append(bits)
            glyphs[cp] = (adv, rows)
        cp, gi = face.get_next_char(cp, gi)
    return glyphs, height, asc, face.family_name.decode(), face


def copyright(face):
    for i in range(face.sfnt_name_count):
        n = face.get_sfnt_name(i)
        if n.name_id == 0 and n.platform_id == 3:
            return n.string.decode('utf-16-be')
    return ''


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    for ttf, name in FONTS:
        glyphs, height, asc, family, face = raster(os.path.join(sys.argv[1], ttf))
        maxw = max(a for a, _ in glyphs.values())
        out = ['STARTFONT 2.1', f'FONT -C47-{family.replace(" ", "")}-Medium-R-Normal--{height}-{height * 10}-75-75-P-0-ISO10646-1',
               f'SIZE {height} 75 75', f'FONTBOUNDINGBOX {maxw} {height} 0 {asc - height}',
               f'COMMENT Rendered from {ttf} of C47 by tools/c47fonts.py, as C47\'s ttf2RasterFonts does.',
               f'COMMENT {copyright(face)}', 'STARTPROPERTIES 3', f'FONT_ASCENT {asc}',
               f'FONT_DESCENT {height - asc}', f'COPYRIGHT "{copyright(face)}"', 'ENDPROPERTIES',
               f'CHARS {len(glyphs)}']
        for cp in sorted(glyphs):
            adv, rows = glyphs[cp]
            nbytes = max(1, (adv + 7) // 8)
            out += [f'STARTCHAR U+{cp:04X}', f'ENCODING {cp}', f'SWIDTH {adv * 1000 // height} 0',
                    f'DWIDTH {adv} 0', f'BBX {adv} {height} 0 {asc - height}', 'BITMAP']
            out += [f'{r << (nbytes * 8 - adv):0{nbytes * 2}X}' for r in rows]
            out.append('ENDCHAR')
        out.append('ENDFONT')
        path = os.path.join(ROOT, 'tools', 'fonts', name + '.bdf')
        open(path, 'w').write('\n'.join(out) + '\n')
        print(f'{name}.bdf: {len(glyphs)} glyphs, {height} px high')


main()
