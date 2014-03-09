#ifndef EL__SESSION_DOWNLOAD_H
#define EL__SESSION_DOWNLOAD_H

#include "main/object.h"
#include "network/state.h"
#include "util/lists.h"
#include "util/time.h"

/* Silly BFU stuff */
struct dialog_data;
struct listbox_item;
struct terminal;

struct cache_entry;
struct connection;
struct session;
struct uri;

struct download;

typedef void (download_callback_T)(struct download *, void *);

/** Flags controlling how to download a file.  This is a bit mask.
 * Unrecognized bits should be preserved and ignored.  */
enum download_flags {
	/** Downloading cannot be resumed; do not offer such an option
	 * to the user.  All bits clear.  */
	DOWNLOAD_RESUME_DISABLED = 0,

	/** Downloading can be resumed.  This is the usual value.  */
	DOWNLOAD_RESUME_ALLOWED = 1,

	/** The user wants to resume downloading.  This must not occur
	 * without #DOWNLOAD_RESUME_ALLOWED.  */
	DOWNLOAD_RESUME_SELECTED = 2,

	/** The file will be opened in an external handler.  */
	DOWNLOAD_EXTERNAL = 4
};

typedef int download_flags_T;

struct download {
	/* XXX: order matters there, there's some hard initialization in
	 * src/session/session.c and src/viewer/text/view.c */
	LIST_HEAD(struct download);

	struct connection *conn;
	struct cache_entry *cached;
	/** The callback is called when connection gets into a progress state,
	 * after it's over (in a result state), and also periodically after
	 * the download starts receiving some data. */
	download_callback_T *callback;
	void *data;
	struct progress *progress;

	struct connection_state state;
	struct connection_state prev_error;
	int pri;
};

/** The user has navigated to a resource that ELinks does not display
 * automatically because of its MIME type, and ELinks is asking what
 * to do.
 *
 * These structures are kept in the session.type_queries list, and
 * destroy_session() calls done_type_query() to destroy them too.  */
struct type_query {
	LIST_HEAD(struct type_query);

	/** After ELinks has downloaded enough of the resource to see
	 * that a type query is needed, it moves the download here and
	 * continues it while the user decides what to do.  */
	struct download download;

	/** Cache entry loaded from #uri.  Apparently used only for
	 * displaying the header.  */
	struct cache_entry *cached;

	/** The session in which the user navigated to #uri.  The
	 * type_query is in the session.type_queries list of this
	 * session.  */
	struct session *ses;

	/** The URI of the resource about which ELinks is asking.
	 * This reference must be released with done_uri().  */
	struct uri *uri;

	/** The name of the frame in which the user navigated to #uri.
	 * If the user chooses to display the resource, it goes into
	 * this frame.  This string must be freed with mem_free().  */
	char *target_frame;

	/** Command line for an external handler, to be run when the
	 * download finishes.  When ELinks displays the type query,
	 * it copies this from mime_handler.program of the default
	 * handler of the type.  The user can then edit the string.
	 * This string must be freed with mem_free().  */
	char *external_handler;

	/** Whether the external handler is going to use the terminal.
	 * When ELinks displays the type query, it copies this from
	 * mime_handler.block of the default handler of the type.
	 * The user can then change the flag with a checkbox.  */
	int block;

	/** Whether the resource was generated by ELinks running
	 * a local CGI program.  If the user chooses to open the
	 * resource with an external handler, ELinks normally saves
	 * the resource to a temporary file and passes the name of
	 * that to the external handler.  However, if the resource is
	 * from a "file" URI that does not refer to a local CGI, then
	 * Elinks need not copy the file.  */
	unsigned int cgi:1;

	/** mailcap entry with copiousoutput */
	unsigned int copiousoutput:1;
	/* int frame; */
};

struct file_download {
	OBJECT_HEAD(struct file_download);

	struct uri *uri;
	char *file;
	char *external_handler;
	struct session *ses;

	/** The terminal in which message boxes about the download
	 * should be displayed.  If this terminal is closed, then
	 * detach_downloads_from_terminal() changes the pointer to
	 * NULL, and get_default_terminal() will be used if a
	 * terminal is needed later.  However, if the download has
	 * an external handler, then detach_downloads_from_terminal()
	 * aborts it right away; external handlers always run in the
	 * original terminal, if anywhere.  */
	struct terminal *term;

	time_t remotetime;
	off_t seek;
	int handle;
	int redirect_cnt;
	int notify;
	struct download download;

	/** Should the file be deleted when destroying the structure */
	unsigned int delete_:1;

	/** Should the download be stopped/interrupted when destroying the structure */
	unsigned int stop:1;

	/** Whether to block the terminal when running the external handler. */
	unsigned int block:1;

	/** Mailcap entry with copiousoutput */
	unsigned int copiousoutput:1;
	
	/** The current dialog for this download. Can be NULL. */
	struct dialog_data *dlg_data;
	struct listbox_item *box_item;
};

/** Stack of all running downloads */
extern LIST_OF(struct file_download) downloads;

static inline int
is_in_downloads_list(struct file_download *file_download)
{
	struct file_download *down;

	foreach (down, downloads)
		if (file_download == down) return 1;

	return 0;
}

int download_is_progressing(struct download *download);

int are_there_downloads(void);

/** Type of the callback function that will be called when the file
 * has been opened, or when it is known that the file will not be
 * opened.
 *
 * @param term
 * The terminal on which the callback should display any windows.
 * Comes directly from the @a term argument of create_download_file().
 *
 * @param fd
 * A file descriptor to the opened file, or -1 if the file will not be
 * opened.  If the @a real_file argument of create_download_file()
 * was not NULL, the callback may read the name of this file from
 * *@a real_file.
 *
 * @param data
 * A pointer to any data that the callback cares about.
 * Comes directly from the @a data argument of create_download_file().
 *
 * @param flags
 * The same as the @a flags argument of create_download_file(),
 * except the ::DOWNLOAD_RESUME_SELECTED bit will be changed to match
 * what the user chose.
 *
 * @relates cdf_hop */
typedef void cdf_callback_T(struct terminal *term, int fd,
			    void *data, download_flags_T flags);

void start_download(void *, char *);
void resume_download(void *, char *);
void create_download_file(struct terminal *, char *, char **,
			  download_flags_T, cdf_callback_T *, void *);

void abort_all_downloads(void);
void destroy_downloads(struct session *);
void detach_downloads_from_terminal(struct terminal *);

int setup_download_handler(struct session *, struct download *, struct cache_entry *, int);

void abort_download(struct file_download *file_download);
void done_type_query(struct type_query *type_query);

void tp_display(struct type_query *type_query);
void tp_save(struct type_query *type_query);
void tp_cancel(void *data);
struct file_download *init_file_download(struct uri *uri, struct session *ses, char *file, int fd);

#endif
