// The list of programs (main memory) and Python scripts (storage memory).

#include "cv.h"
#include "theme.h"
#include <gint/keyboard.h>
#include <stdio.h>

#define UI (&cv_fonts[3])     /* 8x13 for the lists and bars */
#define SMALL (&cv_fonts[1])  /* 6x10 for the F-key labels */
#define BAR_H 18
#define FKEY_H 16
#define ROW_H 17

void fkey_bar(const char *labels[6])
{
	const theme_t *t = theme();
	int y = SCREEN_H - FKEY_H;
	drect(0, y, SCREEN_W - 1, SCREEN_H - 1, t->bg);
	for (int i = 0; i < 6; i++) {
		int x = i * 66;
		drect(x + 1, y + 1, x + 64, SCREEN_H - 1, t->barkey);
		if (labels[i] && labels[i][0]) {
			int n = 0;
			while (labels[i][n])
				n++;
			draw_text(SMALL, x + 33 - n * SMALL->w / 2, y + 4, labels[i], t->bartext, t->barkey);
		}
	}
}

void title_bar(const char *left, const char *right)
{
	const theme_t *t = theme();
	drect(0, 0, SCREEN_W - 1, BAR_H - 1, t->bar);
	draw_text(UI, 6, 3, left, t->bartext, t->bar);
	if (right) {
		int n = 0;
		while (right[n])
			n++;
		draw_text(UI, SCREEN_W - 6 - n * UI->w, 3, right, t->bartext, t->bar);
	}
}

int list_screen(void)
{
	static int sel = 0, top = 0;
	int rows = (SCREEN_H - BAR_H - FKEY_H - 4) / ROW_H;
	for (;;) {
		const theme_t *t = theme();
		dclear(t->bg);
		char right[24];
		sprintf(right, "%d file%s", nsources, nsources == 1 ? "" : "s");
		title_bar("CodeView", right);
		if (sel >= nsources)
			sel = nsources ? nsources - 1 : 0;
		if (sel < top)
			top = sel;
		if (sel >= top + rows)
			top = sel - rows + 1;
		if (!nsources)
			draw_text(UI, 12, BAR_H + 12, "No programs or .py files found.", t->cls[CL_TEXT], t->bg);
		for (int r = 0; r < rows && top + r < nsources; r++) {
			const source_t *s = &sources[top + r];
			int y = BAR_H + 2 + r * ROW_H;
			uint16_t bg = top + r == sel ? t->select : t->bg;
			drect(0, y, SCREEN_W - 1, y + ROW_H - 1, bg);
			const char *tag = s->type == SRC_BASIC ? "BASIC" : " PY  ";
			draw_text(SMALL, 8, y + 4, tag, s->type == SRC_BASIC ? t->cls[CL_KEYWORD] : t->cls[CL_FUNC], bg);
			draw_utext(UI, 52, y + 2, s->name, t->cls[CL_TEXT], bg);
			if (s->size >= 0) {
				char sz[16];
				sprintf(sz, "%d bytes", s->size);
				int n = 0;
				while (sz[n])
					n++;
				draw_text(UI, SCREEN_W - 8 - n * UI->w, y + 2, sz, t->cls[CL_COMMENT], bg);
			}
		}
		static const char *keys[6] = {"OPEN", "", "", "", "THEME", "RESCAN"};
		fkey_bar(keys);
		dupdate();

		key_event_t ev = getkey_opt(GETKEY_DEFAULT | GETKEY_REP_ARROWS, NULL);
		switch (ev.key) {
		case KEY_UP:
			if (nsources)
				sel = (sel + nsources - 1) % nsources;
			break;
		case KEY_DOWN:
			if (nsources)
				sel = (sel + 1) % nsources;
			break;
		case KEY_EXE:
		case KEY_F1:
			if (nsources)
				return sel;
			break;
		case KEY_F5:
			settings.dark = !settings.dark;
			break;
		case KEY_F6:
			sources_scan();
			break;
		}
	}
}
