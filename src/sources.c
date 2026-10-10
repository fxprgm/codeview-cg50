// Where the code comes from: Casio BASIC programs in main memory (MCS directory "system") and
// .py files at the root of the storage memory. Every OS call runs inside gint_world_switch().

#include "cv.h"
#include <gint/gint.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *os_OpCodeToStr(int opcode, char *result);
int os_MCS_SearchDirectory(const char *dir, void **entry, char *dirno);
int os_MCS_SearchDirectoryItem(const char *dir, const char *item, char *flags, void **itemptr,
	void **data, int *length);

/* Main-memory directory entry and item (as the OS lays them out) */
typedef struct {
	uint8_t name[8];
	void *items;
	uint16_t count;
	uint8_t flags[2];
} mcs_dir_t;

typedef struct {
	uint8_t name[8];
	int offset;
	int length;
	uint8_t flags[4];
} mcs_item_t;

/* A program's data starts with an 8-byte password and two zero bytes */
#define PROGRAM_HEADER 10

source_t sources[MAX_SOURCES];
int nsources;

static void set_name(source_t *s, const uint8_t *raw, int n)
{
	int k = 0;
	for (int i = 0; i < n && raw[i] && k < 23; i++) {
		int c = raw[i];
		if (casio_is_lead(c) && i + 1 < n)
			c = c << 8 | raw[++i];
		s->name[k++] = casio_to_unicode(c);
	}
	s->name[k] = 0;
}

static int scan_programs(void)
{
	void *entry = NULL;
	char dirno;
	if (os_MCS_SearchDirectory("system", &entry, &dirno) != 0 || !entry)
		return 0;
	mcs_dir_t *d = entry;
	mcs_item_t *items = d->items;
	for (int i = 0; i < d->count && nsources < MAX_SOURCES; i++) {
		source_t *s = &sources[nsources++];
		memcpy(s->raw, items[i].name, 8);
		s->raw[8] = 0;
		set_name(s, s->raw, 8);
		s->type = SRC_BASIC;
		s->size = items[i].length > PROGRAM_HEADER ? items[i].length - PROGRAM_HEADER : 0;
	}
	return 0;
}

static int scan_python(void)
{
	DIR *dir = opendir("/");
	if (!dir)
		return 0;
	struct dirent *ent;
	while ((ent = readdir(dir)) && nsources < MAX_SOURCES) {
		int n = strlen(ent->d_name);
		if (n < 4 || n > 23 || strcmp(ent->d_name + n - 3, ".py"))
			continue;
		source_t *s = &sources[nsources++];
		for (int i = 0; i <= n; i++)
			s->name[i] = (uint8_t)ent->d_name[i];
		s->type = SRC_PYTHON;
		s->size = -1;
	}
	closedir(dir);
	return 0;
}

void sources_scan(void)
{
	nsources = 0;
	gint_world_switch(GINT_CALL(scan_programs));
	gint_world_switch(GINT_CALL(scan_python));
}

static int load_program(const source_t *s, uint8_t *buf, int cap, int *len)
{
	char flags;
	void *item, *data = NULL;
	int length = 0;
	if (os_MCS_SearchDirectoryItem("system", (const char *)s->raw, &flags, &item, &data, &length) || !data)
		return -1;
	int n = length - PROGRAM_HEADER;
	if (n > cap)
		n = cap;
	if (n < 0)
		n = 0;
	memcpy(buf, (uint8_t *)data + PROGRAM_HEADER, n);
	*len = n;
	return 0;
}

static int load_file(const char *path, uint8_t **out, int *len)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *buf = malloc(n + 1);
	if (!buf) {
		fclose(f);
		return -1;
	}
	*len = fread(buf, 1, n, f);
	fclose(f);
	*out = buf;
	return 0;
}

uint8_t *source_load(const source_t *s, int *len)
{
	*len = 0;
	if (s->type == SRC_BASIC) {
		uint8_t *buf = malloc(s->size + 1);
		if (!buf)
			return NULL;
		int r = gint_world_switch(GINT_CALL(load_program, (void *)s, (void *)buf, s->size, (void *)len));
		if (r) {
			free(buf);
			return NULL;
		}
		buf[*len] = 0;
		return buf;
	}
	char path[32] = "/";
	for (int i = 0; s->name[i] && i < 24; i++)
		path[i + 1] = (char)s->name[i], path[i + 2] = 0;
	uint8_t *buf = NULL;
	if (gint_world_switch(GINT_CALL(load_file, (void *)path, (void *)&buf, (void *)len)))
		return NULL;
	return buf;
}

/* ---- token texts ---- */

/* single bytes, then the two-byte prefixes in this order */
static const uint8_t prefixes[6] = {0x7F, 0xF7, 0xF9, 0xE5, 0xE6, 0xE7};
static uint16_t tok_off[7 * 256];
static uint8_t *tok_pool;

static int token_index(int opcode)
{
	if (opcode < 0x100)
		return opcode;
	for (int k = 0; k < 6; k++)
		if ((opcode >> 8) == prefixes[k])
			return 256 * (k + 1) + (opcode & 0xff);
	return -1;
}

static int fill_tokens(void)
{
	char tmp[80];
	int used = 0, cap = 24 * 1024;
	tok_pool = malloc(cap);
	if (!tok_pool)
		return -1;
	for (int i = 0; i < 7 * 256; i++) {
		int op = i < 256 ? i : prefixes[i / 256 - 1] << 8 | (i & 0xff);
		memset(tmp, 0, sizeof tmp);
		os_OpCodeToStr(op, tmp);
		int n = strnlen(tmp, sizeof tmp - 1);
		if (used + n + 1 > cap)
			break;
		tok_off[i] = used;
		memcpy(tok_pool + used, tmp, n + 1);
		used += n + 1;
	}
	return 0;
}

void tokens_init(void)
{
	gint_world_switch(GINT_CALL(fill_tokens));
}

const uint8_t *token_text(int opcode)
{
	int i = token_index(opcode);
	if (i < 0 || !tok_pool)
		return (const uint8_t *)"?";
	return tok_pool + tok_off[i];
}
