// Fixed-width fonts: X.org misc-fixed at six sizes, then two of C47's fonts in fixed-width cells
// (CodeView.cfg keeps the index, so new fonts go at the end); tools/mkdata.py generates fonts.c.
#pragma once
#include <stdint.h>

#define CV_NFONTS 8

typedef struct {
	uint8_t w, h;
	uint16_t count;
	const uint16_t *cps;   /* sorted Unicode code points */
	const uint16_t *bits;  /* count * h rows, bit 15 = leftmost pixel */
	const char *name;
} cvfont_t;

extern const cvfont_t cv_fonts[CV_NFONTS];
/* the fonts from small to large, as FONT- and FONT+ step through them */
extern const uint8_t cv_font_order[CV_NFONTS];
