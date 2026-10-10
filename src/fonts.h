// Fixed-width fonts (X.org misc-fixed) at six sizes; tools/mkdata.py generates fonts.c.
#pragma once
#include <stdint.h>

#define CV_NFONTS 6

typedef struct {
	uint8_t w, h;
	uint16_t count;
	const uint16_t *cps;   /* sorted Unicode code points */
	const uint16_t *bits;  /* count * h rows, bit 15 = leftmost pixel */
	const char *name;
} cvfont_t;

extern const cvfont_t cv_fonts[CV_NFONTS];
