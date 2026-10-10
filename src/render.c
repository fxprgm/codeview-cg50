// Fixed-width text straight into gint's VRAM.

#include "cv.h"
#include <gint/display.h>

static int glyph(const cvfont_t *f, uint16_t cp)
{
	int lo = 0, hi = f->count - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (f->cps[mid] == cp)
			return mid;
		if (f->cps[mid] < cp)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return -1;
}

void draw_char(const cvfont_t *f, int x, int y, uint16_t cp, uint16_t fg, uint16_t bg)
{
	int g = glyph(f, cp);
	if (g < 0)
		g = glyph(f, 0x25A1);
	if (g < 0)
		g = glyph(f, '?');
	const uint16_t *rows = f->bits + g * f->h;
	int x0 = x < 0 ? -x : 0, x1 = x + f->w > SCREEN_W ? SCREEN_W - x : f->w;
	for (int r = 0; r < f->h; r++) {
		int yy = y + r;
		if (yy < 0 || yy >= SCREEN_H)
			continue;
		uint16_t bits = rows[r];
		uint16_t *p = gint_vram + yy * SCREEN_W + x;
		for (int c = x0; c < x1; c++)
			p[c] = (bits & (0x8000 >> c)) ? fg : bg;
	}
}

int draw_text(const cvfont_t *f, int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
	for (; *s; s++, x += f->w)
		draw_char(f, x, y, (uint8_t)*s, fg, bg);
	return x;
}

int draw_utext(const cvfont_t *f, int x, int y, const uint16_t *s, uint16_t fg, uint16_t bg)
{
	for (; *s; s++, x += f->w)
		draw_char(f, x, y, *s, fg, bg);
	return x;
}
