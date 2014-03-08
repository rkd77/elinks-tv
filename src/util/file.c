/** File utilities
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_DIRENT_H
#include <dirent.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#else
#if defined(TM_IN_SYS_TIME) && defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#elif defined(HAVE_TIME_H)
#include <time.h>
#endif
#endif

#include "elinks.h"

#include "osdep/osdep.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


/* Not that these two would be so useful for portability (they are ANSI C) but
 * they encapsulate the lowlevel stuff (need for <unistd.h>) nicely. */

int
file_exists(const char *filename)
{
#ifdef HAVE_ACCESS
	return access((const char *)filename, F_OK) >= 0;
#else
	struct stat buf;

	return stat((const char *)filename, &buf) >= 0;
#endif
}

int
file_can_read(const char *filename)
{
#ifdef HAVE_ACCESS
	return access((const char *)filename, R_OK) >= 0;
#else
	FILE *f = fopen((const char *)filename, "rb");
	int ok = !!f;

	if (f) fclose(f);
	return ok;
#endif
}

int
file_is_dir(const char *filename)
{
	struct stat st;

	if (stat((const char *)filename, &st))
		return 0;

	return S_ISDIR(st.st_mode);
}

char *
get_filename_position(char *filename)
{
	char *pos;

	assert(filename);
	if_assert_failed return NULL;

	for (pos = filename; *pos; pos++)
		if (dir_sep(*pos)) filename = pos + 1;

	return filename;
}

char *
expand_tilde(char *filename)
{
	struct string file;

	if (!init_string(&file)) return NULL;

	if (filename[0] == '~') {
		if (!filename[1] || dir_sep(filename[1])) {
			char *home = (char *)getenv("HOME");

			if (home) {
				add_to_string(&file, home);
				filename++;
			}
#ifdef HAVE_GETPWNAM
		} else {
			struct passwd *passwd = NULL;
			char *user = filename + 1;
			int userlen = 0;

			while (user[userlen] && !dir_sep(user[userlen]))
				userlen++;

			user = memacpy(user, userlen);
			if (user) {
				passwd = getpwnam((const char *)user);
				mem_free(user);
			}

			if (passwd && passwd->pw_dir) {
				add_to_string(&file, (const char *)passwd->pw_dir);
				filename += 1 + userlen;
			}
#endif
		}
	}

	add_to_string(&file, filename);

	return file.source;
}

char *
get_unique_name(char *fileprefix)
{
	char *file = fileprefix;
	int fileprefixlen = strlen((const char *)fileprefix);
	int memtrigger = 1;
	int suffix = 1;
	int digits = 0;

	while (file_exists(file)) {
		if (!(suffix < memtrigger)) {
			if (suffix >= 10000)
				INTERNAL("Too big suffix in get_unique_name().");
			memtrigger *= 10;
			digits++;

			if (file != fileprefix) mem_free(file);
			file = (char *)mem_alloc(fileprefixlen + 2 + digits);
			if (!file) return NULL;

			memcpy(file, fileprefix, fileprefixlen);
			file[fileprefixlen] = '.';
		}

		longcat(&file[fileprefixlen + 1], NULL, suffix, digits + 1, 0);
		suffix++;
	}

	return file;
}

char *
get_tempdir_filename(char *name)
{
	char *tmpdir = (char *)getenv("TMPDIR");

	if (!tmpdir || !*tmpdir) tmpdir = (char *)getenv("TMP");
	if (!tmpdir || !*tmpdir) tmpdir = (char *)getenv("TEMPDIR");
	if (!tmpdir || !*tmpdir) tmpdir = (char *)getenv("TEMP");
	if (!tmpdir || !*tmpdir) tmpdir = (char *)"/tmp";

	return straconcat(tmpdir, "/", name, (char *) NULL);
}

char *
file_read_line(char *line, size_t *size, FILE *file, int *lineno)
{
	size_t offset = 0;

	if (!line) {
		line = (char *)mem_alloc(MAX_STR_LEN);
		if (!line)
			return NULL;

		*size = MAX_STR_LEN;
	}

	while (fgets((char *)(line + offset), *size - offset, file)) {
		char *linepos = (char *)strchr((char *)(line + offset), '\n');

		if (!linepos) {
			/* Test if the line buffer should be increase because
			 * it was continued and could not fit. */
			char *newline;
			int next = getc(file);

			if (next == EOF) {
				/* We are on the last line. */
				(*lineno)++;
				return line;
			}

			/* Undo our dammage */
			ungetc(next, file);
			offset = *size - 1;
			*size += MAX_STR_LEN;

			newline = (char *)mem_realloc(line, *size);
			if (!newline)
				break;
			line = newline;
			continue;
		}

		/* A whole line was read. Fetch next into the buffer if
		 * the line is 'continued'. */
		(*lineno)++;

		while (line < linepos && isspace(*linepos))
			linepos--;

		if (*linepos != '\\') {
			*(linepos + 1) = '\0';
			return line;
		}
		offset = linepos - line - 1;
	}

	mem_free_if(line);
	return NULL;
}


/* Some mkstemp() implementations do not set safe permissions,
 * so it may result in temporary files readable by all users.
 * This may be a problem when textarea fields are edited through
 * an external editor (see src/viewer/text/textarea.c).
 *
 * From 2001-12-23 mkstemp(3) gnu man page:
 *
 * ...
 * The file is then created with mode read/write and permissions 0666
 * (glibc  2.0.6 and  earlier), 0600 (glibc 2.0.7 and later).
 * ...
 *
 * NOTES:
 * The old behaviour (creating a file with mode 0666) may be a security
 * risk, especially since other Unix flavours use 0600, and somebody
 * might overlook this detail when porting programs.
 * More generally, the POSIX specification does not say anything
 * about file modes, so the application should make sure its umask is
 * set appropriately before calling mkstemp.
 */
int
safe_mkstemp(char *template_)
{
#ifndef CONFIG_OS_WIN32
	mode_t saved_mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
#endif
	int fd = mkstemp((char *)template_);
#ifndef CONFIG_OS_WIN32
	umask(saved_mask);
#endif
	return fd;
}

/* comparison function for qsort() */
int
compare_dir_entries(const void *v1, const void *v2)
{
	const struct directory_entry *d1 = (const struct directory_entry *)v1, *d2 = (const struct directory_entry *)v2;

	if (d1->name[0] == '.' && d1->name[1] == '.' && !d1->name[2]) return -1;
	if (d2->name[0] == '.' && d2->name[1] == '.' && !d2->name[2]) return 1;
	if (d1->attrib[0] == 'd' && d2->attrib[0] != 'd') return -1;
	if (d1->attrib[0] != 'd' && d2->attrib[0] == 'd') return 1;
	return strcmp((const char *)d1->name, (const char *)d2->name);
}


/** This function decides whether a file should be shown in directory
 * listing or not. @returns according boolean value. */
static inline int
file_visible(char *name, int get_hidden_files, int is_root_directory)
{
	/* Always show everything not beginning with a dot. */
	if (name[0] != '.')
		return 1;

	/* Always hide the "." directory. */
	if (name[1] == '\0')
		return 0;

	/* Always show the ".." directory (but for root directory). */
	if (name[1] == '.' && name[2] == '\0')
		return !is_root_directory;

	/* Others like ".x" or "..foo" are shown if get_hidden_files
	 * == 1. */
	return get_hidden_files;
}

/** First information such as permissions is gathered for each directory entry.
 * All entries are then sorted. */
struct directory_entry *
get_directory_entries(char *dirname, int get_hidden)
{
	struct directory_entry *entries = NULL;
	DIR *directory;
	int size = 0;
	struct dirent *entry;
	int is_root_directory = dirname[0] == '/' && !dirname[1];

	directory = opendir((const char *)dirname);
	if (!directory) return NULL;

	while ((entry = readdir(directory))) {
		struct stat st, *stp;
		struct directory_entry *new_entries;
		char *name;
		struct string attrib;

		if (!file_visible((char *)entry->d_name, get_hidden, is_root_directory))
			continue;

		new_entries = (struct directory_entry *)mem_realloc(entries, (size + 2) * sizeof(*new_entries));
		if (!new_entries) continue;
		entries = new_entries;

		/* We allocate the full path because it is used in a few places
		 * which means less allocation although a bit more short term
		 * memory usage. */
		name = straconcat(dirname, entry->d_name,
				  (char *) NULL);
		if (!name) continue;

		if (!init_string(&attrib)) {
			mem_free(name);
			continue;
		}

#ifdef FS_UNIX_SOFTLINKS
		stp = (lstat((const char *)name, &st)) ? NULL : &st;
#else
		stp = (stat((const char *)name, &st)) ? NULL : &st;
#endif

		stat_type(&attrib, stp);
		stat_mode(&attrib, stp);
		stat_links(&attrib, stp);
		stat_user(&attrib, stp);
		stat_group(&attrib, stp);
		stat_size(&attrib, stp);
		stat_date(&attrib, stp);

		entries[size].name = name;
		entries[size].attrib = attrib.source;
		size++;
	}

	closedir(directory);

	if (!size) {
		/* We may have allocated space for entries but added none. */
		mem_free_if(entries);
		return NULL;
	}

	qsort(entries, size, sizeof(*entries), compare_dir_entries);

	memset(&entries[size], 0, sizeof(*entries));

	return entries;
}

/** Recursively create directories in @a path. The last element in the path is
 * taken to be a filename, and simply ignored */
int
mkalldirs(const char *path)
{
	int pos, len, ret = 0;
	char *p;

	if (!*path) return -1;

	/* Make a copy of path, to be able to write to it.  Otherwise, the
	 * function is unsafe if called with a read-only char *argument.  */
	len = strlen((const char *)path) + 1;
	p = (char *)fmem_alloc(len);
	if (!p) return -1;
	memcpy(p, path, len);

	for (pos = 1; p[pos]; pos++) {
		unsigned char separator = p[pos];

		if (!dir_sep(separator))
			continue;

		p[pos] = 0;

		ret = mkdir((const char *)p, S_IRWXU);

		p[pos] = separator;

		if (ret < 0) {
			if (errno != EEXIST) break;
			ret = 0;
		}
	}

	fmem_free(p);
	return ret;
}
