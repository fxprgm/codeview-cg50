// Light and dark colour themes (RGB565 via gint's C_RGB, 0..31 per channel).
#pragma once
#include <gint/display.h>
#include "cv.h"

typedef struct {
	uint16_t bg, bar, bartext, barkey, select, gutter, gutterbg, guide;
	uint16_t cls[CL_COUNT];
} theme_t;

static const theme_t themes[2] = {
	{ /* light */
		.bg = C_RGB(31, 31, 31), .bar = C_RGB(4, 7, 14), .bartext = C_RGB(31, 31, 31),
		.barkey = C_RGB(8, 11, 18), .select = C_RGB(25, 28, 31), .gutter = C_RGB(17, 17, 19),
		.gutterbg = C_RGB(29, 29, 30), .guide = C_RGB(27, 27, 28),
		.cls = {
			[CL_TEXT] = C_RGB(0, 0, 0), [CL_KEYWORD] = C_RGB(16, 0, 22),
			[CL_COMMAND] = C_RGB(0, 10, 22), [CL_FUNC] = C_RGB(2, 15, 4),
			[CL_STRING] = C_RGB(22, 6, 0), [CL_COMMENT] = C_RGB(14, 14, 14),
			[CL_NUMBER] = C_RGB(22, 0, 12), [CL_OP] = C_RGB(26, 0, 0),
		},
	},
	{ /* dark */
		.bg = C_RGB(3, 3, 4), .bar = C_RGB(1, 2, 5), .bartext = C_RGB(25, 27, 30),
		.barkey = C_RGB(6, 8, 13), .select = C_RGB(6, 9, 15), .gutter = C_RGB(12, 13, 15),
		.gutterbg = C_RGB(5, 5, 6), .guide = C_RGB(7, 7, 9),
		.cls = {
			[CL_TEXT] = C_RGB(26, 26, 27), [CL_KEYWORD] = C_RGB(24, 16, 30),
			[CL_COMMAND] = C_RGB(11, 25, 22), [CL_FUNC] = C_RGB(27, 27, 20),
			[CL_STRING] = C_RGB(30, 18, 14), [CL_COMMENT] = C_RGB(13, 19, 11),
			[CL_NUMBER] = C_RGB(22, 25, 20), [CL_OP] = C_RGB(31, 13, 13),
		},
	},
};

static inline const theme_t *theme(void)
{
	return &themes[settings.dark];
}
