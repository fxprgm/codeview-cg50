// The viewer: one source, in the chosen font size, with line numbers, block indentation (with
// faint guides), colours, and either wrapping or horizontal scrolling.

#include "cv.h"
#include "theme.h"
#include <gint/keyboard.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fkey_bar(const char *labels[6]);
void title_bar(const char *left, const char *right);

#define BAR_H 18
#define FKEY_H 16
#define TEXT_Y (BAR_H + 2)
#define TEXT_H (SCREEN_H - BAR_H - FKEY_H - 4)
#define INDENT 2   /* characters per block level */

typedef struct {
	doc_t doc;
	const source_t *src;
	int line, sub;   /* first visible row: a line, and a row inside it when wrapping */
	int hscroll;     /* first visible column when not wrapping */
} view_t;

static const cvfont_t *font(void)
{
	return &cv_fonts[settings.font];
}

static int digits(int n)
{
	int d = 1;
	while (n >= 10)
		n /= 10, d++;
	return d;
}

static int gutter_cols(const view_t *v)
{
	return settings.numbers ? digits(v->doc.nlines) + 1 : 0;
}

static int text_cols(const view_t *v)
{
	return SCREEN_W / font()->w - gutter_cols(v);
}

static int per_row(const view_t *v, int line)
{
	int n = text_cols(v) - v->doc.indent[line] * INDENT;
	return n < 8 ? 8 : n;
}

static int rows_of(const view_t *v, int line)
{
	if (!settings.wrap)
		return 1;
	int len = v->doc.len[line], n = per_row(v, line);
	return len ? (len + n - 1) / n : 1;
}

static int screen_rows(void)
{
	return TEXT_H / font()->h;
}

/* whether rows remain below the screen when the top row is (line, sub) */
static bool more_below(const view_t *v)
{
	int left = screen_rows();
	int line = v->line, sub = v->sub;
	while (line < v->doc.nlines) {
		left -= rows_of(v, line) - sub;
		if (left < 0)
			return true;
		line++;
		sub = 0;
	}
	return false;
}

/* moves the top row by delta rows, down only while the end isn't on screen yet */
static void scroll(view_t *v, int delta)
{
	while (delta > 0) {
		if (!more_below(v))
			break;
		if (v->sub + 1 < rows_of(v, v->line))
			v->sub++;
		else if (v->line + 1 < v->doc.nlines)
			v->line++, v->sub = 0;
		else
			break;
		delta--;
	}
	while (delta < 0) {
		if (v->sub > 0)
			v->sub--;
		else if (v->line > 0)
			v->line--, v->sub = rows_of(v, v->line) - 1;
		else
			break;
		delta++;
	}
}

static void draw(view_t *v)
{
	const theme_t *t = theme();
	const cvfont_t *f = font();
	dclear(t->bg);

	char name[24], right[48];
	int k = 0;
	for (; v->src->name[k] && k < 20; k++)
		name[k] = v->src->name[k] < 128 ? v->src->name[k] : '?';
	name[k] = 0;
	int rows = screen_rows();
	sprintf(right, "%d/%d  %s", v->line + 1, v->doc.nlines, f->name);
	title_bar(name, right);

	int gc = gutter_cols(v), gw = gc * f->w;
	if (gc)
		drect(0, TEXT_Y, gw - f->w / 2 - 1, TEXT_Y + rows * f->h - 1, t->gutterbg);
	int line = v->line, sub = v->sub;
	for (int r = 0; r < rows && line < v->doc.nlines; r++) {
		int y = TEXT_Y + r * f->h;
		int ind = v->doc.indent[line];
		if (gc && sub == 0) {
			/* right-aligned in gc - 1 columns (gint's printf has no '*' width) */
			char num[12];
			int n = sprintf(num, "%d", line + 1);
			draw_text(f, (gc - 1 - n) * f->w, y, num, t->gutter, t->gutterbg);
		}
		/* indentation guides: a faint line at each block level */
		for (int l = 0; l < ind; l++) {
			int gx = gw + (l * INDENT) * f->w + f->w / 2 - (settings.wrap ? 0 : v->hscroll * f->w);
			if (gx >= gw)
				drect(gx, y, gx, y + f->h - 1, t->guide);
		}
		int x0 = gw + ind * INDENT * f->w;
		int from, count;
		if (settings.wrap) {
			int n = per_row(v, line);
			from = sub * n;
			count = n;
		}
		else {
			x0 -= v->hscroll * f->w;
			from = 0;
			count = v->doc.len[line];
		}
		int base = v->doc.start[line];
		for (int c = 0; c < count && from + c < v->doc.len[line]; c++) {
			int x = x0 + c * f->w;
			if (x < gw)
				continue;
			if (x + f->w > SCREEN_W)
				break;
			int i = base + from + c;
			draw_char(f, x, y, v->doc.cp[i], t->cls[v->doc.cls[i]], t->bg);
		}
		if (++sub >= rows_of(v, line))
			line++, sub = 0;
	}

	const char *keys[6] = {"FONT-", "FONT+", settings.wrap ? "NOWRAP" : "WRAP",
		settings.numbers ? "NO LN" : "LINES", settings.dark ? "LIGHT" : "DARK", "TOP/END"};
	fkey_bar(keys);
	dupdate();
}

static void message(const char *msg)
{
	const theme_t *t = theme();
	dclear(t->bg);
	title_bar("CodeView", NULL);
	draw_text(&cv_fonts[3], 12, BAR_H + 12, msg, t->cls[CL_OP], t->bg);
	dupdate();
	getkey();
}

void view_screen(const source_t *s)
{
	int len;
	uint8_t *raw = source_load(s, &len);
	if (!raw) {
		message("Can't read this source.");
		return;
	}
	view_t v = {.src = s};
	bool ok = s->type == SRC_BASIC ? doc_basic(&v.doc, raw, len) : doc_python(&v.doc, raw, len);
	free(raw);
	if (!ok) {
		message("Not enough memory for this source.");
		return;
	}

	for (;;) {
		draw(&v);
		key_event_t ev = getkey_opt(GETKEY_DEFAULT | GETKEY_REP_ALL, NULL);
		int page = screen_rows() - 1;
		switch (ev.key) {
		case KEY_UP:
			scroll(&v, ev.shift ? -page : -1);
			break;
		case KEY_DOWN:
			scroll(&v, ev.shift ? page : 1);
			break;
		case KEY_LEFT:
			if (settings.wrap)
				scroll(&v, -page);
			else if (v.hscroll > 0)
				v.hscroll = v.hscroll > 4 ? v.hscroll - 4 : 0;
			break;
		case KEY_RIGHT:
			if (settings.wrap)
				scroll(&v, page);
			else
				v.hscroll += 4;
			break;
		case KEY_F1:
		case KEY_SUB:
			if (settings.font > 0)
				settings.font--, v.sub = 0;
			break;
		case KEY_F2:
		case KEY_ADD:
			if (settings.font < CV_NFONTS - 1)
				settings.font++, v.sub = 0;
			break;
		case KEY_F3:
			settings.wrap = !settings.wrap;
			v.sub = 0;
			v.hscroll = 0;
			break;
		case KEY_F4:
			settings.numbers = !settings.numbers;
			v.sub = 0;
			break;
		case KEY_F5:
			settings.dark = !settings.dark;
			break;
		case KEY_F6:
			if (v.line == 0 && v.sub == 0)
				scroll(&v, 1 << 30);
			else
				v.line = v.sub = 0;
			break;
		case KEY_EXIT:
			doc_free(&v.doc);
			return;
		}
	}
}
