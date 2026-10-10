// CodeView: read the PRGM app's Casio BASIC programs (and .py scripts) in a resizable
// fixed-width font, with indentation and colours.

#include "cv.h"
#include <gint/drivers/keydev.h>
#include <gint/gint.h>
#include <gint/keyboard.h>
#include <stdio.h>

settings_t settings = {.dark = false, .font = 2, .wrap = false, .numbers = true};

/* The settings live in \\fls0\CodeView.cfg as "<dark> <font> <wrap> <numbers>" */
#define CONFIG "/CodeView.cfg"
static settings_t saved;

static int do_load(void)
{
	FILE *f = fopen(CONFIG, "r");
	if (!f)
		return -1;
	int dark, font, wrap, numbers;
	if (fscanf(f, "%d %d %d %d", &dark, &font, &wrap, &numbers) == 4 && font >= 0 && font < CV_NFONTS)
		settings = (settings_t){dark != 0, font, wrap != 0, numbers != 0};
	fclose(f);
	return 0;
}

static int do_save(void)
{
	FILE *f = fopen(CONFIG, "w");
	if (!f)
		return -1;
	fprintf(f, "%d %d %d %d\n", settings.dark, settings.font, settings.wrap, settings.numbers);
	fclose(f);
	return 0;
}

/* Writes the settings if they changed (the storage memory is flash: not on every key) */
void settings_save(void)
{
	if (saved.dark == settings.dark && saved.font == settings.font && saved.wrap == settings.wrap &&
	    saved.numbers == settings.numbers)
		return;
	gint_world_switch(GINT_CALL(do_save));
	saved = settings;
}

int main(void)
{
	keydev_set_standard_repeats(keydev_std(), 300 * 1000, 35 * 1000);
	gint_world_switch(GINT_CALL(do_load));
	saved = settings;
	tokens_init();
	sources_scan();
	for (;;) {
		int i = list_screen();
		if (i >= 0)
			view_screen(&sources[i]);
		settings_save();
	}
	return 1;
}
