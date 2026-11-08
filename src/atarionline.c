/*
 * atarionline.c - browser of the atarionline.pl game catalogue
 *
 * Copyright (C) 2026 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 microcomputers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "atarionline.h"

#ifdef HAVE_DOWNLOAD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_WINDOWS_H
#include <direct.h>
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <time.h>

#include "atari.h"
#include "cfg.h"
#include "download.h"
#include "log.h"
#include "ui.h"
#include "util.h"

#define AO_BASE_URL   "https://atarionline.pl/v01/"
#define AO_ROOT_HREF  "index.php?ct=kazip"
#define AO_TIMEOUT    120L
#define AO_MIN_GAP_MS 1100UL
#define AO_CACHE_MAX_AGE (30L * 24 * 3600)
#define AO_REFRESH_ID 9000
#define AO_MAX_ENTRIES 4000
#define AO_MAX_HREF   1024
#define AO_MAX_LABEL  200

enum { KIND_LETTERS, KIND_TITLES, KIND_FILES };

typedef struct {
	char *label; /* ASCII text shown in the menu */
	char *href;  /* relative link exactly as found on the page, '&amp;' resolved */
} Entry;

typedef struct {
	Entry *e;
	int n;
} List;

static const char * const runnable_exts[] = {
	".atr", ".xfd", ".atx", ".pro", ".dcm", ".xex", ".com", ".exe", ".obx",
	".bas", ".cas", ".car", ".rom", ".bin", ".zip", ".gz", ".atz", ".xfz", NULL
};
/* Extensions accepted inside a ZIP archive. */
static const char *zip_exts[] = {
	".atr", ".xfd", ".atx", ".pro", ".dcm", ".xex", ".com", ".exe", ".obx",
	".bas", ".cas", ".car", ".rom", ".bin", ".gz", ".atz", ".xfz", NULL
};

/* ---------------------------------------------------------------- text */

static int HexVal(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/* URL-decodes src (up to len bytes, '%XX' and '+') into dest of size size. */
static void UrlDecode(const char *src, size_t len, char *dest, size_t size)
{
	size_t o = 0;
	size_t i = 0;
	if (size == 0)
		return;
	while (i < len && src[i] != '\0' && o + 1 < size) {
		if (src[i] == '%' && i + 2 < len
		    && HexVal((unsigned char) src[i + 1]) >= 0
		    && HexVal((unsigned char) src[i + 2]) >= 0) {
			dest[o++] = (char) ((HexVal((unsigned char) src[i + 1]) << 4) | HexVal((unsigned char) src[i + 2]));
			i += 3;
		}
		else if (src[i] == '+') {
			dest[o++] = ' ';
			i++;
		}
		else
			dest[o++] = src[i++];
	}
	dest[o] = '\0';
}

/* Latin-1 Supplement U+00C0..U+00FF; '#' = special, see below. */
static const char latin1_map[] = "AAAAAA#CEEEEIIIIDNOOOOOxOUUUUY##aaaaaa#ceeeeiiiidnooooo/ouuuuy#y";
/* Latin Extended-A U+0100..U+017F */
static const char latinA_map[] =
	"AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIi##JjKkkLlLlLlLlLlNnNnNn#NnOoOoOo##RrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";

/* Appends ASCII transliteration of UTF-8 string src to dest. */
static void Transliterate(const char *src, char *dest, size_t size)
{
	size_t o = 0;
	const unsigned char *p = (const unsigned char *) src;
	if (size == 0)
		return;
	while (*p != '\0' && o + 4 < size) {
		unsigned long cp;
		int extra;
		const char *rep = NULL;
		char single[2];
		if (*p < 0x80) {
			dest[o++] = (*p >= 0x20 && *p != 0x7f) ? (char) *p : '?';
			p++;
			continue;
		}
		if ((*p & 0xe0) == 0xc0) { cp = *p & 0x1f; extra = 1; }
		else if ((*p & 0xf0) == 0xe0) { cp = *p & 0x0f; extra = 2; }
		else if ((*p & 0xf8) == 0xf0) { cp = *p & 0x07; extra = 3; }
		else { dest[o++] = '?'; p++; continue; }
		p++;
		while (extra > 0 && (*p & 0xc0) == 0x80) {
			cp = (cp << 6) | (*p & 0x3f);
			p++;
			extra--;
		}
		if (extra > 0) {
			dest[o++] = '?';
			continue;
		}
		single[1] = '\0';
		if (cp >= 0xc0 && cp <= 0xff) {
			single[0] = latin1_map[cp - 0xc0];
			if (single[0] == '#') {
				switch (cp) {
				case 0xc6: rep = "AE"; break;
				case 0xde: rep = "Th"; break;
				case 0xdf: rep = "ss"; break;
				case 0xe6: rep = "ae"; break;
				default: rep = "th"; break;
				}
			}
			else
				rep = single;
		}
		else if (cp >= 0x100 && cp <= 0x17f) {
			single[0] = latinA_map[cp - 0x100];
			if (single[0] == '#') {
				switch (cp) {
				case 0x132: rep = "IJ"; break;
				case 0x133: rep = "ij"; break;
				case 0x149: rep = "n"; break;
				case 0x152: rep = "OE"; break;
				default: rep = "oe"; break;
				}
			}
			else
				rep = single;
		}
		else if (cp == 0x2018 || cp == 0x2019)
			rep = "'";
		else if (cp == 0x201c || cp == 0x201d)
			rep = "\"";
		else if (cp == 0x2013 || cp == 0x2014)
			rep = "-";
		else
			rep = "?";
		while (*rep != '\0' && o + 1 < size)
			dest[o++] = *rep++;
	}
	dest[o] = '\0';
}

/* Copies the raw (still encoded) value of parameter name ("sub", "title",
   "file") from href into out. Returns TRUE if found. */
static int GetParam(const char *href, const char *name, char *out, size_t size)
{
	char key[16];
	const char *p;
	size_t len;
	snprintf(key, sizeof(key), "&%s=", name);
	p = strstr(href, key);
	if (p == NULL)
		return FALSE;
	p += strlen(key);
	len = strcspn(p, "&#");
	if (len + 1 > size)
		len = size - 1;
	memcpy(out, p, len);
	out[len] = '\0';
	return TRUE;
}

static void Trim(char *s)
{
	size_t len = strlen(s);
	size_t start = 0;
	while (len > 0 && s[len - 1] == ' ')
		s[--len] = '\0';
	while (s[start] == ' ')
		start++;
	if (start > 0)
		memmove(s, s + start, len - start + 1);
}

static int HasExt(const char *name, const char * const exts[])
{
	const char *dot = strrchr(name, '.');
	int i;
	if (dot == NULL)
		return FALSE;
	for (i = 0; exts[i] != NULL; i++)
		if (Util_stricmp(dot, exts[i]) == 0)
			return TRUE;
	return FALSE;
}

/* ------------------------------------------------------------- lists */

static void FreeList(List *l)
{
	int i;
	for (i = 0; i < l->n; i++) {
		free(l->e[i].label);
		free(l->e[i].href);
	}
	free(l->e);
	l->e = NULL;
	l->n = 0;
}

/* Adds an entry unless an entry with the same href exists. Returns FALSE
   only on out of memory. */
static int AddEntry(List *l, const char *label, const char *href)
{
	int i;
	Entry *ne;
	char *lc;
	char *hc;
	for (i = 0; i < l->n; i++)
		if (strcmp(l->e[i].href, href) == 0)
			return TRUE;
	if (l->n >= AO_MAX_ENTRIES)
		return TRUE;
	ne = (Entry *) realloc(l->e, (l->n + 1) * sizeof(Entry));
	if (ne == NULL)
		return FALSE;
	l->e = ne;
	lc = Util_strdup(label);
	hc = Util_strdup(href);
	if (lc == NULL || hc == NULL) {
		free(lc);
		free(hc);
		return FALSE;
	}
	l->e[l->n].label = lc;
	l->e[l->n].href = hc;
	l->n++;
	return TRUE;
}

/* Scans html for href="..." links and fills list according to kind. */
static int ParsePage(const char *html, int kind, List *list)
{
	const char *p = html;
	while ((p = strstr(p, "href=\"")) != NULL) {
		const char *start;
		const char *end;
		char href[AO_MAX_HREF];
		char raw[AO_MAX_HREF];
		char dec[AO_MAX_HREF];
		char label[AO_MAX_LABEL];
		size_t i;
		size_t o;
		size_t len;
		p += 6;
		start = p;
		end = strchr(start, '"');
		if (end == NULL)
			break;
		p = end;
		len = (size_t) (end - start);
		if (len == 0 || len >= sizeof(href))
			continue;
		/* resolve &amp; */
		o = 0;
		for (i = 0; i < len; i++) {
			if (start[i] == '&' && len - i >= 5 && strncmp(start + i, "&amp;", 5) == 0) {
				href[o++] = '&';
				i += 4;
			}
			else
				href[o++] = start[i];
		}
		href[o] = '\0';

		if (kind == KIND_LETTERS || kind == KIND_TITLES) {
			char *hash;
			if (strncmp(href, AO_ROOT_HREF "&sub=", strlen(AO_ROOT_HREF "&sub=")) != 0)
				continue;
			if (kind == KIND_LETTERS) {
				if (strstr(href, "&title=") != NULL || !GetParam(href, "sub", raw, sizeof(raw)))
					continue;
			}
			else {
				if (!GetParam(href, "title", raw, sizeof(raw)))
					continue;
				hash = strchr(href, '#');
				if (hash != NULL)
					*hash = '\0';
			}
		}
		else {
			if (strncmp(href, "kazip.php?", 10) != 0 || !GetParam(href, "file", raw, sizeof(raw)))
				continue;
		}
		UrlDecode(raw, strlen(raw), dec, sizeof(dec));
		if (kind == KIND_FILES && !HasExt(dec, runnable_exts))
			continue;
		Transliterate(dec, label, sizeof(label));
		Trim(label);
		if (label[0] == '\0')
			continue;
		if (!AddEntry(list, label, href))
			return FALSE;
	}
	return TRUE;
}

/* ------------------------------------------------------------ network */

/* Builds absolute URL from a relative href, percent-encoding unsafe bytes. */
static void BuildUrl(const char *href, char *url, size_t size)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = strlen(AO_BASE_URL);
	if (size <= o + 1) {
		url[0] = '\0';
		return;
	}
	memcpy(url, AO_BASE_URL, o);
	for (; *href != '\0' && o + 4 < size; href++) {
		unsigned char c = (unsigned char) *href;
		if (c <= 0x20 || c >= 0x7f || strchr("\"<>\\^`{|}", c) != NULL) {
			url[o++] = '%';
			url[o++] = hex[c >> 4];
			url[o++] = hex[c & 15];
		}
		else
			url[o++] = (char) c;
	}
	url[o] = '\0';
}

static void ShowMessage(const char *msg, int wait)
{
	UI_driver->fMessage(msg, wait);
}

/* Throttling: at least AO_MIN_GAP_MS between two consecutive HTTP requests. */
static unsigned long NowMs(void)
{
#ifdef HAVE_WINDOWS_H
	return (unsigned long) GetTickCount();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned long) tv.tv_sec * 1000UL + (unsigned long) (tv.tv_usec / 1000);
#endif
}

static unsigned long last_request_end = 0;
static int have_last_request = FALSE;

/* One throttled HTTP GET. Every request is logged. */
static int HttpGet(const char *url, unsigned char **data, size_t *size)
{
	int ok;
	if (have_last_request) {
		unsigned long elapsed = NowMs() - last_request_end;
		if (elapsed < AO_MIN_GAP_MS) {
			unsigned long wait = AO_MIN_GAP_MS - elapsed;
#ifdef HAVE_WINDOWS_H
			Sleep(wait);
#else
			usleep(wait * 1000UL);
#endif
		}
	}
	Log_print("ATARIONLINE: HTTP GET %s", url);
	ok = Download_ToMemory(url, data, size, AO_TIMEOUT);
	last_request_end = NowMs();
	have_last_request = TRUE;
	return ok;
}

/* ------------------------------------------------------------ storage */

static int MakeDir(const char *path)
{
#ifdef HAVE_WINDOWS_H
	return mkdir(path);
#else
	return mkdir(path, 0777);
#endif
}

static void MakeDirs(const char *path)
{
	char tmp[FILENAME_MAX];
	char *p;
	Util_strlcpy(tmp, path, sizeof(tmp));
	for (p = tmp + 1; *p != '\0'; p++) {
		if (*p == '/') {
			*p = '\0';
			MakeDir(tmp);
			*p = '/';
		}
	}
	MakeDir(tmp);
}

/* Makes a string safe to be used as one path component. */
static void SanitizeName(char *s)
{
	char *p;
	for (p = s; *p != '\0'; p++) {
		unsigned char c = (unsigned char) *p;
		if (c < 0x20 || c == 0x7f || c == '/' || c == '\\' || c == ':'
		    || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			*p = '_';
	}
	while ((p = strstr(s, "..")) != NULL)
		p[0] = '_';
	if (s[0] == '.')
		s[0] = '_';
	if (s[0] == '\0')
		strcpy(s, "_");
}

static void StorageBase(char *base, size_t size)
{
	const char *home = getenv("HOME");
	if (home != NULL && home[0] != '\0')
		snprintf(base, size, "%s/.atari800/atarionline", home);
	else if (CFG_data_dir[0] != '\0')
		snprintf(base, size, "%s/atarionline", CFG_data_dir);
	else
		snprintf(base, size, "atarionline");
}

static long FileSize(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return -1;
	return (long) st.st_size;
}

/* Makes sure the file linked by href is stored locally; path receives its
   location. Returns TRUE on success (error message already shown otherwise). */
static int FetchFile(const char *href, const char *letter, char *path, size_t path_size)
{
	char base[FILENAME_MAX];
	char dir[FILENAME_MAX];
	char lname[AO_MAX_LABEL];
	char raw[AO_MAX_HREF];
	char fname[AO_MAX_HREF];
	char url[AO_MAX_HREF * 3 + 64];
	char part[FILENAME_MAX + 8];
	unsigned char *data;
	size_t size;
	FILE *f;
	int ok;

	if (!GetParam(href, "file", raw, sizeof(raw))) {
		ShowMessage("Bad file link", 1);
		return FALSE;
	}
	UrlDecode(raw, strlen(raw), fname, sizeof(fname));
	SanitizeName(fname);
	Util_strlcpy(lname, letter, sizeof(lname));
	SanitizeName(lname);
	StorageBase(base, sizeof(base));
	snprintf(dir, sizeof(dir), "%s/%s", base, lname);
	if ((size_t) snprintf(path, path_size, "%s/%s", dir, fname) >= path_size) {
		ShowMessage("File name too long", 1);
		return FALSE;
	}
	if (FileSize(path) > 0)
		return TRUE;

	ShowMessage("Downloading file...", 0);
	BuildUrl(href, url, sizeof(url));
	if (!HttpGet(url, &data, &size)) {
		ShowMessage("Download error (no network?)", 1);
		return FALSE;
	}
	if (size == 0) {
		free(data);
		ShowMessage("Downloaded file is empty", 1);
		return FALSE;
	}
	MakeDirs(dir);
	snprintf(part, sizeof(part), "%s.part", path);
	f = fopen(part, "wb");
	ok = (f != NULL);
	if (ok) {
		ok = (fwrite(data, 1, size, f) == size);
		if (fclose(f) != 0)
			ok = FALSE;
	}
	free(data);
	if (ok) {
		remove(path);
		ok = (rename(part, path) == 0);
	}
	if (!ok) {
		remove(part);
		ShowMessage("Cannot save file", 1);
		return FALSE;
	}
	return TRUE;
}

/* If path is a ZIP archive, extracts it and replaces path with the first
   runnable file. Returns FALSE (message shown) on failure. */
static int UnpackIfZip(char *path, size_t path_size)
{
	FILE *f;
	long size;
	unsigned char *data;
	size_t got;
	char dir[FILENAME_MAX];
	const char *first;

	size = FileSize(path);
	if (size < 4)
		return TRUE;
	f = fopen(path, "rb");
	if (f == NULL)
		return TRUE;
	data = (unsigned char *) malloc((size_t) size);
	if (data == NULL) {
		fclose(f);
		ShowMessage("Out of memory", 1);
		return FALSE;
	}
	got = fread(data, 1, (size_t) size, f);
	fclose(f);
	if (got != (size_t) size || data[0] != 'P' || data[1] != 'K' || data[2] != 3 || data[3] != 4) {
		free(data);
		return TRUE; /* not a ZIP, use as is */
	}
	snprintf(dir, sizeof(dir), "%s.d", path);
	first = Download_ExtractZipBuffer(data, got, zip_exts, dir);
	free(data);
	if (first == NULL) {
		ShowMessage("No runnable file inside ZIP", 1);
		return FALSE;
	}
	if ((size_t) snprintf(path, path_size, "%s/%s", dir, first) >= path_size) {
		ShowMessage("File name too long", 1);
		return FALSE;
	}
	return TRUE;
}

/* ------------------------------------------------------- persistent cache
   Layout (under StorageBase()):
     cache/root.txt            letters
     cache/<Letter>.txt        titles of a letter
     cache/<Letter>/<Title>.txt  files of a title
   Each line: label TAB href. Valid AO_CACHE_MAX_AGE seconds (file mtime). */

static int CachePath(int kind, const char *letter, const char *href, char *out, size_t size)
{
	char base[FILENAME_MAX];
	char lname[AO_MAX_LABEL];
	char raw[AO_MAX_HREF];
	char tname[AO_MAX_HREF];
	StorageBase(base, sizeof(base));
	if (kind == KIND_LETTERS)
		return (size_t) snprintf(out, size, "%s/cache/root.txt", base) < size;
	Util_strlcpy(lname, letter, sizeof(lname));
	SanitizeName(lname);
	if (kind == KIND_TITLES)
		return (size_t) snprintf(out, size, "%s/cache/%s.txt", base, lname) < size;
	if (!GetParam(href, "title", raw, sizeof(raw)))
		return FALSE;
	UrlDecode(raw, strlen(raw), tname, sizeof(tname));
	Trim(tname);
	if (strlen(tname) > 100)
		tname[100] = '\0';
	SanitizeName(tname);
	return (size_t) snprintf(out, size, "%s/cache/%s/%s.txt", base, lname, tname) < size;
}

/* Loads list from a cache file if it exists and is fresh. */
static int LoadCache(const char *path, List *list)
{
	struct stat st;
	FILE *f;
	char line[AO_MAX_HREF + AO_MAX_LABEL + 8];
	if (stat(path, &st) != 0 || st.st_size == 0)
		return FALSE;
	if (difftime(time(NULL), st.st_mtime) > (double) AO_CACHE_MAX_AGE)
		return FALSE;
	f = fopen(path, "r");
	if (f == NULL)
		return FALSE;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *tab;
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';
		tab = strchr(line, '\t');
		if (tab == NULL || tab == line || tab[1] == '\0')
			continue;
		*tab = '\0';
		if (!AddEntry(list, line, tab + 1)) {
			fclose(f);
			FreeList(list);
			return FALSE;
		}
	}
	fclose(f);
	if (list->n == 0) {
		FreeList(list);
		return FALSE;
	}
	return TRUE;
}

static void SaveCache(const char *path, const List *list)
{
	char dir[FILENAME_MAX];
	char *slash;
	FILE *f;
	int i;
	int ok;
	Util_strlcpy(dir, path, sizeof(dir));
	slash = strrchr(dir, '/');
	if (slash != NULL) {
		*slash = '\0';
		MakeDirs(dir);
	}
	f = fopen(path, "w");
	if (f == NULL)
		return;
	ok = TRUE;
	for (i = 0; i < list->n; i++)
		if (fprintf(f, "%s\t%s\n", list->e[i].label, list->e[i].href) < 0)
			ok = FALSE;
	if (fclose(f) != 0)
		ok = FALSE;
	if (!ok)
		remove(path);
}

/* Fills *list (caller frees with FreeList). Uses the disk cache unless
   refresh is TRUE or it is stale; then fetches exactly one page. */
static int GetList(int kind, const char *letter, const char *href, int refresh, List *list)
{
	char cpath[FILENAME_MAX];
	char url[AO_MAX_HREF * 3 + 64];
	unsigned char *data;
	size_t size;
	unsigned char *tmp;
	int have_path;

	list->e = NULL;
	list->n = 0;
	have_path = CachePath(kind, letter, href, cpath, sizeof(cpath));
	if (have_path && !refresh && LoadCache(cpath, list))
		return TRUE;

	ShowMessage("Downloading list...", 0);
	BuildUrl(href, url, sizeof(url));
	if (!HttpGet(url, &data, &size)) {
		ShowMessage("Download error (no network?)", 1);
		return FALSE;
	}
	tmp = (unsigned char *) realloc(data, size + 1);
	if (tmp == NULL) {
		free(data);
		ShowMessage("Out of memory", 1);
		return FALSE;
	}
	data = tmp;
	data[size] = '\0';
	if (!ParsePage((const char *) data, kind, list)) {
		FreeList(list);
		free(data);
		ShowMessage("Out of memory", 1);
		return FALSE;
	}
	free(data);
	if (list->n == 0) {
		FreeList(list);
		ShowMessage(kind == KIND_FILES ? "No runnable files for this title"
		                               : "No entries found", 1);
		return FALSE;
	}
	if (have_path)
		SaveCache(cpath, list);
	return TRUE;
}

/* ----------------------------------------------------------------- UI */

/* Shows list l with a "[Refresh this list]" item on top. Returns chosen
   index, -1 on cancel or -2 if refresh was requested. *pos is the initial
   and (updated) remembered position. */
static int SelectFromList(const char *title, const List *l, int *pos)
{
	UI_tMenuItem *menu;
	int i;
	int choice;
	static char refresh_label[] = "[Refresh this list]";

	menu = (UI_tMenuItem *) malloc((l->n + 2) * sizeof(UI_tMenuItem));
	if (menu == NULL) {
		ShowMessage("Out of memory", 1);
		return -1;
	}
	menu[0].flags = UI_ITEM_ACTION;
	menu[0].retval = AO_REFRESH_ID;
	menu[0].prefix = NULL;
	menu[0].item = refresh_label;
	menu[0].suffix = NULL;
	for (i = 0; i < l->n; i++) {
		menu[i + 1].flags = UI_ITEM_ACTION;
		menu[i + 1].retval = (SWORD) i;
		menu[i + 1].prefix = NULL;
		menu[i + 1].item = l->e[i].label;
		menu[i + 1].suffix = NULL;
	}
	menu[l->n + 1].flags = UI_ITEM_END;
	menu[l->n + 1].retval = 0;
	menu[l->n + 1].prefix = NULL;
	menu[l->n + 1].item = NULL;
	menu[l->n + 1].suffix = NULL;
	if (*pos < 0 || *pos >= l->n)
		*pos = 0;
	choice = UI_driver->fSelect(title, 0, *pos, menu, NULL);
	free(menu);
	if (choice == AO_REFRESH_ID)
		return -2;
	if (choice < 0)
		return -1;
	*pos = choice;
	return choice;
}

int ATARIONLINE_Browse(void)
{
	List letters = {NULL, 0};
	List titles = {NULL, 0};
	List files = {NULL, 0};
	int level = 0;
	int pos[3] = {0, 0, 0};
	int last_letter = -1;
	int last_title = -1;
	int letter = 0;
	int title = 0;
	int refresh = FALSE;
	int result = FALSE;
	char caption[AO_MAX_LABEL + 32];

	if (!GetList(KIND_LETTERS, NULL, AO_ROOT_HREF, FALSE, &letters))
		return FALSE;

	for (;;) {
		int c;
		if (level == 0) {
			c = SelectFromList("Online Games - pick a letter", &letters, &pos[0]);
			if (c == -2) {
				List nl;
				if (GetList(KIND_LETTERS, NULL, AO_ROOT_HREF, TRUE, &nl)) {
					FreeList(&letters);
					letters = nl;
					last_letter = -1;
				}
				continue;
			}
			if (c < 0)
				break;
			letter = c;
			if (letter != last_letter) {
				pos[1] = 0;
				pos[2] = 0;
				last_letter = letter;
				last_title = -1;
				FreeList(&titles);
			}
			level = 1;
		}
		else if (level == 1) {
			if (titles.n == 0 || refresh) {
				List nl;
				if (!GetList(KIND_TITLES, letters.e[letter].label, letters.e[letter].href, refresh, &nl)) {
					refresh = FALSE;
					if (titles.n == 0)
						level = 0;
					continue;
				}
				FreeList(&titles);
				titles = nl;
				refresh = FALSE;
			}
			snprintf(caption, sizeof(caption), "Online Games - %s", letters.e[letter].label);
			c = SelectFromList(caption, &titles, &pos[1]);
			if (c == -2) {
				refresh = TRUE;
				continue;
			}
			if (c < 0) {
				level = 0;
				continue;
			}
			title = c;
			if (title != last_title) {
				pos[2] = 0;
				last_title = title;
			}
			FreeList(&files);
			level = 2;
		}
		else {
			char path[FILENAME_MAX];
			if (files.n == 0 || refresh) {
				List nl;
				if (!GetList(KIND_FILES, letters.e[letter].label, titles.e[title].href, refresh, &nl)) {
					refresh = FALSE;
					if (files.n == 0)
						level = 1;
					continue;
				}
				FreeList(&files);
				files = nl;
				refresh = FALSE;
			}
			c = SelectFromList(titles.e[title].label, &files, &pos[2]);
			if (c == -2) {
				refresh = TRUE;
				continue;
			}
			if (c < 0) {
				FreeList(&files);
				level = 1;
				continue;
			}
			if (!FetchFile(files.e[c].href, letters.e[letter].label, path, sizeof(path)))
				continue;
			if (!UnpackIfZip(path, sizeof(path)))
				continue;
			if (UI_AutostartPath(path)) {
				result = TRUE;
				break;
			}
		}
	}
	FreeList(&letters);
	FreeList(&titles);
	FreeList(&files);
	return result;
}

#endif /* HAVE_DOWNLOAD */
