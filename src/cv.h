// CodeView: a reader for the PRGM app's Casio BASIC programs and Python scripts.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 396
#define SCREEN_H 224

/* ---- sources: programs in main memory, .py files in the storage memory ---- */

#define MAX_SOURCES 128

enum { SRC_BASIC, SRC_PYTHON };

typedef struct {
	uint16_t name[24]; /* display name, Unicode, 0-terminated */
	uint8_t raw[9];    /* program: the 8-byte main-memory item name */
	int type;
	int size;
} source_t;

extern source_t sources[MAX_SOURCES];
extern int nsources;

void sources_scan(void);
/* The source's bytes (a program's tokens after its 10-byte header, or a file's text), in a
   malloc()ed buffer; NULL on failure */
uint8_t *source_load(const source_t *s, int *len);
/* Text of every token, cached at startup: CASIO character bytes, NUL-terminated */
void tokens_init(void);
const uint8_t *token_text(int opcode);

/* ---- documents: decoded lines of Unicode characters, each with a colour class ---- */

enum {
	CL_TEXT, CL_KEYWORD, CL_COMMAND, CL_FUNC, CL_STRING, CL_COMMENT, CL_NUMBER, CL_OP,
	CL_COUNT,
};

typedef struct {
	int nlines;
	int nchars;
	uint32_t *start;   /* first character of each line */
	uint16_t *len;     /* characters in each line */
	uint8_t *indent;   /* block depth, for display */
	uint16_t *cp;      /* Unicode */
	uint8_t *cls;      /* CL_* */
} doc_t;

bool doc_basic(doc_t *d, const uint8_t *tokens, int len);
bool doc_python(doc_t *d, const uint8_t *text, int len);
void doc_free(doc_t *d);
/* CASIO character code (one byte, or lead byte << 8 | second byte) to Unicode */
uint16_t casio_to_unicode(int code);
int casio_is_lead(int byte);

/* ---- drawing: fixed-width fonts straight into VRAM ---- */

#include "fonts.h"
void draw_char(const cvfont_t *f, int x, int y, uint16_t cp, uint16_t fg, uint16_t bg);
/* ASCII text; returns the x after it */
int draw_text(const cvfont_t *f, int x, int y, const char *s, uint16_t fg, uint16_t bg);
/* Unicode text, 0-terminated; returns the x after it */
int draw_utext(const cvfont_t *f, int x, int y, const uint16_t *s, uint16_t fg, uint16_t bg);

/* ---- screens ---- */

typedef struct {
	bool dark;
	int font;          /* index into cv_fonts */
	bool wrap;
	bool numbers;
} settings_t;

extern settings_t settings;
void settings_save(void);

int list_screen(void);   /* returns the chosen source, or -1 */
void view_screen(const source_t *s);
