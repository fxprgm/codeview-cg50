// Decoding sources into documents: lines of Unicode characters, each with a colour class, and
// the block depth of each line for the indentation the PRGM editor doesn't show.

#include "cv.h"
#include "charmap.h"
#include <stdlib.h>
#include <string.h>

int casio_is_lead(int b)
{
	return b == 0x7F || b == 0xE5 || b == 0xE6 || b == 0xE7 || b == 0xF7 || b == 0xF9;
}

uint16_t casio_to_unicode(int code)
{
	int lo = 0, hi = cv_charmap_count - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (cv_charmap_codes[mid] == code)
			return cv_charmap_unicode[mid];
		if (cv_charmap_codes[mid] < code)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	if (code >= 0x20 && code < 0x7F)
		return code;
	return 0x25A1; /* □ */
}

/* ---- builder: the same pass counts (d->cp == NULL) and then fills ---- */

typedef struct {
	doc_t *d;
	int line, nchars, line_start;
} builder_t;

static void put(builder_t *b, uint16_t cp, int cls)
{
	if (b->d->cp) {
		b->d->cp[b->nchars] = cp;
		b->d->cls[b->nchars] = cls;
	}
	b->nchars++;
}

static void end_line(builder_t *b, int indent)
{
	if (b->d->start) {
		b->d->start[b->line] = b->line_start;
		b->d->len[b->line] = b->nchars - b->line_start;
		b->d->indent[b->line] = indent > 255 ? 255 : indent;
	}
	b->line++;
	b->line_start = b->nchars;
}

static bool allocate(doc_t *d, int nlines, int nchars)
{
	d->nlines = nlines;
	d->nchars = nchars;
	d->start = malloc(nlines * sizeof *d->start);
	d->len = malloc(nlines * sizeof *d->len);
	d->indent = malloc(nlines);
	d->cp = malloc((nchars + 1) * sizeof *d->cp);
	d->cls = malloc(nchars + 1);
	if (d->start && d->len && d->indent && d->cp && d->cls)
		return true;
	doc_free(d);
	return false;
}

void doc_free(doc_t *d)
{
	free(d->start);
	free(d->len);
	free(d->indent);
	free(d->cp);
	free(d->cls);
	memset(d, 0, sizeof *d);
}

/* ---- Casio BASIC ---- */

static int classify(int op)
{
	if ((op >= 0xF700 && op <= 0xF70E) || op == 0xE2 || op == 0xEC || op == 0xED ||
	    op == 0xE8 || op == 0xE9 || op == 0x13)
		return CL_KEYWORD;          /* If…Stop, Lbl, Goto, Prog, Dsz, Isz, ⇒ */
	if (op == 0x0E || op == 0x0C || op == 0x3A)
		return CL_OP;               /* →, ◢ (Disp), : */
	if ((op >= '0' && op <= '9') || op == '.' || op == 0x0F)
		return CL_NUMBER;
	if ((op >> 8) == 0xF7 || (op >> 8) == 0xF9 || (op >= 0xD1 && op <= 0xFF))
		return CL_COMMAND;
	if ((op >> 8) == 0x7F || (op >= 0x80 && op < 0xD1 && op != 0x89 && op != 0x99 &&
	    op != 0xA9 && op != 0xB9 && op != 0x87 && op != 0xCC && op != 0xCD))
		return CL_FUNC;
	return CL_TEXT;
}

static int block_delta(int op)
{
	switch (op) {
	case 0xF700: case 0xF704: case 0xF708: case 0xF70A: return 1;   /* If For While Do */
	case 0xF703: case 0xF707: case 0xF709: case 0xF70B: return -1;  /* IfEnd Next WhileEnd LpWhile */
	}
	return 0;
}

static void put_casio_text(builder_t *b, const uint8_t *s, int cls)
{
	for (int i = 0; s[i]; i++) {
		int c = s[i];
		if (casio_is_lead(c) && s[i + 1])
			c = c << 8 | s[++i];
		put(b, casio_to_unicode(c), cls);
	}
}

static void decode_basic(builder_t *b, const uint8_t *t, int len)
{
	int depth = 0, line_indent = 0;
	bool first = true, in_string = false, in_comment = false;
	int i = 0;
	while (i < len && t[i]) {
		int c = t[i];
		if (c == 0x0D) {
			end_line(b, line_indent);
			first = true;
			in_string = in_comment = false;
			i++;
			continue;
		}
		int op = c, n = 1;
		if (casio_is_lead(c) && i + 1 < len) {
			op = c << 8 | t[i + 1];
			n = 2;
		}
		if (in_string) {
			/* characters, not tokens; \" is a quote inside the string */
			if (c == '"')
				in_string = false;
			else if (c == '\\' && i + 1 < len && t[i + 1] == '"') {
				put(b, '\\', CL_STRING);
				op = '"';
				n = 2;
			}
			put(b, casio_to_unicode(op), CL_STRING);
			i += n;
			continue;
		}
		if (first) {
			int dd = block_delta(op);
			line_indent = (dd < 0 || op == 0xF702) && depth > 0 ? depth - 1 : depth;
			first = false;
		}
		if (c == '"' && !in_comment) {
			in_string = true;
			put(b, '"', CL_STRING);
			i++;
			continue;
		}
		if (op == '\'')
			in_comment = true;
		int cls = in_comment ? CL_COMMENT : classify(op);
		if (!in_comment) {
			depth += block_delta(op);
			if (depth < 0)
				depth = 0;
		}
		put_casio_text(b, token_text(op), cls);
		i += n;
	}
	end_line(b, first ? depth : line_indent);
}

bool doc_basic(doc_t *d, const uint8_t *tokens, int len)
{
	memset(d, 0, sizeof *d);
	builder_t b = {d, 0, 0, 0};
	decode_basic(&b, tokens, len);
	if (!allocate(d, b.line, b.nchars))
		return false;
	builder_t f = {d, 0, 0, 0};
	decode_basic(&f, tokens, len);
	return true;
}

/* ---- Python ---- */

static const char *py_keywords[] = {
	"False", "None", "True", "and", "as", "assert", "break", "class", "continue", "def",
	"del", "elif", "else", "except", "finally", "for", "from", "global", "if", "import",
	"in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
	"while", "with", "yield", NULL,
};

static bool is_ident(int c)
{
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static void decode_python(builder_t *b, const uint8_t *t, int len)
{
	int quote = 0;          /* inside a string: its quote character (or -1/-2 for ''' / """) */
	int i = 0;
	bool comment = false;
	while (i < len) {
		int c = t[i];
		if (c == '\r') {
			i++;
			continue;
		}
		if (c == '\n') {
			end_line(b, 0);
			comment = false;
			if (quote > 0)
				quote = 0;
			i++;
			continue;
		}
		/* UTF-8 */
		int cp = c, n = 1;
		if (c >= 0xC0 && c < 0xE0 && i + 1 < len)
			cp = (c & 0x1F) << 6 | (t[i + 1] & 0x3F), n = 2;
		else if (c >= 0xE0 && c < 0xF0 && i + 2 < len)
			cp = (c & 0x0F) << 12 | (t[i + 1] & 0x3F) << 6 | (t[i + 2] & 0x3F), n = 3;
		else if (c >= 0x80)
			cp = 0x25A1;
		if (c == '\t') {
			for (int k = 0; k < 4; k++)
				put(b, ' ', CL_TEXT);
			i++;
			continue;
		}
		if (comment) {
			put(b, cp, CL_COMMENT);
			i += n;
			continue;
		}
		if (quote) {
			put(b, cp, CL_STRING);
			bool triple = quote < 0;
			int q = triple ? (quote == -1 ? '\'' : '"') : quote;
			if (c == '\\' && i + 1 < len && t[i + 1] != '\n') {
				put(b, t[i + 1], CL_STRING);
				i += 2;
				continue;
			}
			if (c == q && (!triple || (i + 2 < len && t[i + 1] == q && t[i + 2] == q))) {
				if (triple) {
					put(b, q, CL_STRING);
					put(b, q, CL_STRING);
					i += 2;
				}
				quote = 0;
			}
			i += n;
			continue;
		}
		if (c == '#') {
			comment = true;
			continue;
		}
		if (c == '\'' || c == '"') {
			if (i + 2 < len && t[i + 1] == c && t[i + 2] == c) {
				quote = c == '\'' ? -1 : -2;
				put(b, c, CL_STRING);
				put(b, c, CL_STRING);
				put(b, c, CL_STRING);
				i += 3;
				continue;
			}
			quote = c;
			put(b, c, CL_STRING);
			i++;
			continue;
		}
		if ((c >= '0' && c <= '9') && (i == 0 || !is_ident(t[i - 1]))) {
			while (i < len && (is_ident(t[i]) || t[i] == '.'))
				put(b, t[i++], CL_NUMBER);
			continue;
		}
		if (is_ident(c) && (i == 0 || !is_ident(t[i - 1]))) {
			int e = i;
			while (e < len && is_ident(t[e]))
				e++;
			int cls = CL_TEXT;
			for (int k = 0; py_keywords[k]; k++)
				if ((int)strlen(py_keywords[k]) == e - i && !memcmp(py_keywords[k], t + i, e - i))
					cls = CL_KEYWORD;
			if (cls == CL_TEXT && e < len && t[e] == '(')
				cls = CL_FUNC;
			while (i < e)
				put(b, t[i++], cls);
			continue;
		}
		put(b, cp, (c == '=' || c == ':') ? CL_OP : CL_TEXT);
		i += n;
	}
	end_line(b, 0);
}

bool doc_python(doc_t *d, const uint8_t *text, int len)
{
	memset(d, 0, sizeof *d);
	builder_t b = {d, 0, 0, 0};
	decode_python(&b, text, len);
	if (!allocate(d, b.line, b.nchars))
		return false;
	builder_t f = {d, 0, 0, 0};
	decode_python(&f, text, len);
	return true;
}
