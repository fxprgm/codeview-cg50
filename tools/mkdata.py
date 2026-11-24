"""Generate src/fonts.c and src/charmap.c.

usage: python3 tools/mkdata.py

fonts.c   the fixed-width X.org misc-fixed fonts (public domain, tools/fonts/) at six sizes, and
          two of C47's fonts (GPL-3.0, tools/fonts/C47-*.bdf, made by tools/c47fonts.py) placed
          in fixed-width cells, with the glyphs for ASCII and every character the CASIO
          character map below can produce. Each glyph is `h` rows of uint16, bit 15 = leftmost
          pixel.
charmap.c CASIO fx-9860G/fx-CG character codes (one byte, or a lead byte 7F E5 E6 E7 F7 F9 and a
          second byte) to Unicode, from Cahute's character database (tools/chars.toml,
          copyright Thomas Touhey, CeCILL 2.1).
"""
import os, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = ['5x7', '6x10', '6x13', '8x13', '9x15', '10x20']
# C47's fonts in fixed-width cells: (name, BDF, cell width, misc-fixed font for the characters
# C47 lacks, whether to thicken those to C47's 2-pixel strokes). The fallbacks sit on the
# bottom of the cell: 10x20 has C47 Standard's baseline, 5x7 is one row shorter than Tiny.
C47_FONTS = [('C47 6x8', 'C47-Tiny', 6, '5x7', False),
             ('C47 11x20', 'C47-Standard', 11, '10x20', True)]


def bdf(path):
    glyphs, bb, cur = {}, None, None
    lines = open(path, encoding='latin-1').read().splitlines()
    i = 0
    while i < len(lines):
        p = lines[i].split()
        if p and p[0] == 'FONTBOUNDINGBOX':
            bb = tuple(map(int, p[1:5]))
        elif p and p[0] == 'STARTCHAR':
            cur = {}
        elif p and p[0] == 'ENCODING':
            cur['code'] = int(p[1])
        elif p and p[0] == 'BBX':
            cur['bbx'] = tuple(map(int, p[1:5]))
        elif p and p[0] == 'BITMAP':
            w, h, _, _ = cur['bbx']
            cur['rows'] = [int(lines[i + 1 + r], 16) for r in range(h)]
            cur['nbits'] = ((w + 7) // 8) * 8
            if cur['code'] >= 0:
                glyphs[cur['code']] = cur
            i += h
        i += 1
    return bb, glyphs


def cell(bb, g):
    """The glyph as fh rows of uint16 in an fw x fh cell"""
    fw, fh, fx, fy = bb
    rows = [0] * fh
    w, h, ox, oy = g['bbx']
    x0 = ox - fx
    y0 = (fh + fy) - (h + oy)
    for r, bits in enumerate(g['rows']):
        for c in range(w):
            if bits >> (g['nbits'] - 1 - c) & 1 and 0 <= y0 + r < fh and 0 <= x0 + c < 16:
                rows[y0 + r] |= 0x8000 >> (x0 + c)
    return rows


def ink(rows, nbits):
    """the first and last columns (0 = leftmost) with a pixel in rows of nbits-wide ints"""
    cols = [c for c in range(nbits) if any(r >> (nbits - 1 - c) & 1 for r in rows)]
    return (cols[0], cols[-1]) if cols else None


def fit(rows, nbits, cw):
    """A proportional glyph (rows of nbits-wide ints) in a cw-wide cell, as rows of uint16:
    its ink centred; ink wider than the cell loses doubled columns (a column equal to its
    left neighbour, the middle ones first; C47's strokes are 2 pixels), then is cut evenly."""
    span = ink(rows, nbits)
    if not span:
        return [0] * len(rows)
    lo, hi = span
    cols = [[r >> (nbits - 1 - c) & 1 for r in rows] for c in range(lo, hi + 1)]
    while len(cols) > cw:
        mid = len(cols) / 2
        dup = sorted((i for i in range(1, len(cols)) if cols[i] == cols[i - 1]), key=lambda i: abs(i - mid))
        if not dup:
            cut = len(cols) - cw
            cols = cols[cut // 2:len(cols) - (cut - cut // 2)]
            break
        del cols[dup[0]]
    x0 = (cw - len(cols)) // 2
    out = [0] * len(rows)
    for c, col in enumerate(cols):
        for y, b in enumerate(col):
            if b:
                out[y] |= 0x8000 >> (x0 + c)
    return out


def c47_font(name, bdfname, cw, fallback, thicken, wanted):
    """(width, height, {code point: rows of uint16}) for one of C47_FONTS"""
    bb, glyphs = bdf(os.path.join(ROOT, 'tools', 'fonts', bdfname + '.bdf'))
    fbb, fglyphs = bdf(os.path.join(ROOT, 'tools', 'fonts', fallback + '.bdf'))
    fh = bb[1]
    out, borrowed = {}, []
    for c in sorted(wanted):
        if c in glyphs:  # c47fonts.py writes every glyph at the font's full height
            out[c] = fit(glyphs[c]['rows'], glyphs[c]['nbits'], cw)
        elif c in fglyphs:
            rows = cell(fbb, fglyphs[c])
            if thicken:
                rows = [r | r >> 1 for r in rows]
            out[c] = fit([0] * (fh - fbb[1]) + rows, 16, cw)  # bottom-aligned
            borrowed.append(c)
    if 0x25E2 in wanted and 0x25E2 not in out:
        # ◢ (Casio BASIC's display mark) is in neither font: a triangle on the baseline
        n = min(cw - 1, fh - 3)
        out[0x25E2] = fit([0] * (fh - n) + [((2 << k) - 1) << (16 - n) for k in range(n)], 16, cw)
    print(f'{name}: from {fallback}: {"".join(map(chr, borrowed))}')
    return cw, fh, out


def charmap():
    db = tomllib.load(open(os.path.join(ROOT, 'tools', 'chars.toml'), 'rb'))['chars']
    m = {}
    for c in db:
        if c.get('table') not in (None, '9860') or 'unicode' not in c:
            continue
        # sequences (a letter and a combining mark): keep the base character
        cp = next((u for u in c['unicode'] if not 0x300 <= u <= 0x36F), c['unicode'][0])
        if cp <= 0xFFFF:
            m.setdefault(c['code'], cp)
    # Not in the database: E741-E75A and E761-E77A are CASIO's small letters (as in "nPr"),
    # shown as plain letters; single-byte symbols that only the legacy table lists
    for k in range(26):
        m.setdefault(0xE741 + k, ord('A') + k)
        m.setdefault(0xE761 + k, ord('a') + k)
    for code, cp in ((0x0C, 0x25E2), (0x0E, 0x2192), (0x10, 0x2264), (0x11, 0x2260),
                     (0x12, 0x2265), (0x13, 0x21D2)):
        m.setdefault(code, cp)
    return m


def main():
    cmap = charmap()
    wanted = set(range(0x20, 0x7F)) | set(cmap.values()) | {0x25A1}  # □ for missing glyphs

    out = ['// Generated by tools/mkdata.py from the X.org misc-fixed fonts (public domain) and two of',
           '// C47\'s fonts (copyright The C47 and WP43 Team, GPL-3.0; tools/fonts/C47-*.bdf).',
           '#include "fonts.h"', '']
    names = []
    for f in FONTS:
        bb, glyphs = bdf(os.path.join(ROOT, 'tools', 'fonts', f + '.bdf'))
        cps = sorted(c for c in wanted if c in glyphs)
        fw, fh = bb[0], bb[1]
        out.append(f'static const uint16_t cps_{f.replace("x", "_")}[{len(cps)}] = {{')
        for i in range(0, len(cps), 12):
            out.append('\t' + ', '.join(f'0x{c:04X}' for c in cps[i:i + 12]) + ',')
        out.append('};')
        out.append(f'static const uint16_t bits_{f.replace("x", "_")}[{len(cps) * fh}] = {{')
        for c in cps:
            out.append('\t' + ', '.join(f'0x{r:04X}' for r in cell(bb, glyphs[c])) + ',')
        out.append('};')
        names.append((f, fw, fh, len(cps)))
        print(f'{f}: {len(cps)} of {len(wanted)} characters')
    for f, bdfname, cw, fallback, thicken in C47_FONTS:
        fw, fh, g = c47_font(f, bdfname, cw, fallback, thicken, wanted)
        k = f.replace(' ', '_').replace('x', '_')
        cps = sorted(g)
        out.append(f'static const uint16_t cps_{k}[{len(cps)}] = {{')
        for i in range(0, len(cps), 12):
            out.append('\t' + ', '.join(f'0x{c:04X}' for c in cps[i:i + 12]) + ',')
        out.append('};')
        out.append(f'static const uint16_t bits_{k}[{len(cps) * fh}] = {{')
        for c in cps:
            out.append('\t' + ', '.join(f'0x{r:04X}' for r in g[c]) + ',')
        out.append('};')
        names.append((f, fw, fh, len(cps)))
        print(f'{f}: {len(cps)} of {len(wanted)} characters')
    out.append(f'const cvfont_t cv_fonts[CV_NFONTS] = {{')
    for f, fw, fh, n in names:
        k = f.replace(' ', '_').replace('x', '_')
        out.append(f'\t{{{fw}, {fh}, {n}, cps_{k}, bits_{k}, "{f}"}},')
    out.append('};')
    order = sorted(range(len(names)), key=lambda i: (names[i][2], names[i][1]))  # height, width
    out.append(f'const uint8_t cv_font_order[CV_NFONTS] = {{{", ".join(map(str, order))}}};')
    open(os.path.join(ROOT, 'src', 'fonts.c'), 'w').write('\n'.join(out) + '\n')

    keys = sorted(cmap)
    out = ['// Generated by tools/mkdata.py. CASIO fx-CG character codes to Unicode, from Cahute\'s',
           '// character database (chars.toml, copyright Thomas Touhey, CeCILL 2.1).',
           '#include "charmap.h"', '',
           f'const uint16_t cv_charmap_codes[{len(keys)}] = {{']
    for i in range(0, len(keys), 12):
        out.append('\t' + ', '.join(f'0x{k:04X}' for k in keys[i:i + 12]) + ',')
    out.append('};')
    out.append(f'const uint16_t cv_charmap_unicode[{len(keys)}] = {{')
    for i in range(0, len(keys), 12):
        out.append('\t' + ', '.join(f'0x{cmap[k]:04X}' for k in keys[i:i + 12]) + ',')
    out.append('};')
    out.append(f'const int cv_charmap_count = {len(keys)};')
    open(os.path.join(ROOT, 'src', 'charmap.c'), 'w').write('\n'.join(out) + '\n')
    print(f'charmap: {len(keys)} codes')


main()
