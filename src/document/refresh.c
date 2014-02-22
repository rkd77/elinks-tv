/** Document (meta) refresh.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/document.h"
#include "document/refresh.h"
#include "document/view.h"
#include "main/select.h"
#include "main/timer.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/session.h"
#include "session/task.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


struct document_refresh *
init_document_refresh(unsigned char *url, unsigned long seconds)
{
	struct document_refresh *refresh;

	refresh = mem_alloc(sizeof(*refresh));
	if (!refresh) return NULL;

	refresh->uri = get_uri(url, 0);
	if (!refresh->uri) {
		mem_free(refresh);
		return NULL;
	}

	refresh->seconds = seconds;
	refresh->timer = TIMER_ID_UNDEF;
	refresh->restart = 1;

	return refresh;
}

void
kill_document_refresh(struct document_refresh *refresh)
{
	kill_timer(&refresh->timer);
}

void
done_document_refresh(struct document_refresh *refresh)
{
	kill_document_refresh(refresh);
	done_uri(refresh->uri);
	mem_free(refresh);
}

/** Timer callback for document_refresh.timer.  As explained in
 * install_timer(), this function must erase the expired timer ID from
 * all variables.  */
static void
do_document_refresh(void *data)
{
	struct document_view *doc_view = data;
	struct document_refresh *refresh = doc_view->document->refresh;
	struct type_query *type_query;

	assert(refresh);

	refresh->timer = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */

	/* When refreshing documents that will trigger a download (like
	 * sourceforge's download pages) make sure that we do not endlessly
	 * trigger the download (bug 289). */
	foreach (type_query, doc_view->session->type_queries)
		if (compare_uri(refresh->uri, type_query->uri, URI_BASE))
			return;

	if (compare_uri(refresh->uri, doc_view->document->uri, 0)) {
		/* If the refreshing is for the current URI, force a reload. */
		reload_frame(doc_view->session, doc_view->name,
		             CACHE_MODE_FORCE_RELOAD);
	} else {
		/* This makes sure that we send referer. */
		goto_uri_frame(doc_view->session, refresh->uri, doc_view->name, CACHE_MODE_NORMAL);
		/* XXX: A possible very wrong work-around for refreshing used when
		 * downloading files. */
		refresh->restart = 0;
	}
}

static void
start_document_refresh(struct document_refresh *refresh,
                       struct document_view *doc_view)
{
	milliseconds_T minimum = (milliseconds_T) get_opt_int("document.browse.minimum_refresh_time", doc_view->session);
	milliseconds_T refresh_delay = sec_to_ms(refresh->seconds);
	milliseconds_T time = ms_max(refresh_delay, minimum);
	struct type_query *type_query;

	/* FIXME: This is just a work-around for stopping more than one timer
	 * from being started at anytime. The refresh timer should maybe belong
	 * to the session? The multiple refresh timers is triggered by
	 * http://ttforums.owenrudge.net/login.php when pressing 'Log in' and
	 * waiting for it to refresh. --jonas */
	if (!refresh->restart
	    || refresh->timer != TIMER_ID_UNDEF)
		return;

	/* Like bug 289 another sourceforge download thingy this time with
	 * number 434. It should take care when refreshing to the same URI or
	 * what ever the cause is. */
	foreach (type_query, doc_view->session->type_queries)
		if (compare_uri(refresh->uri, type_query->uri, URI_BASE))
			return;

	if (time == 0) {
		/* Use register_bottom_half() instead of install_timer()
		 * because the latter asserts that the delay is greater
		 * than 0. */
		register_bottom_half(do_document_refresh, doc_view);
		return;
	}

	install_timer(&refresh->timer, time, do_document_refresh, doc_view);
}

void
start_document_refreshes(struct session *ses)
{

	assert(ses);

	if (!ses->doc_view
	    || !ses->doc_view->document
	    || !get_opt_bool("document.browse.refresh", ses))
		return;

	if (document_has_frames(ses->doc_view->document)) {
		struct document_view *doc_view;

		foreach (doc_view, ses->scrn_frames) {
			if (!doc_view->document
			    || !doc_view->document->refresh)
				continue;

			start_document_refresh(doc_view->document->refresh,
			                       doc_view);
		}
	}

	if (!ses->doc_view->document->refresh)
		return;

	start_document_refresh(ses->doc_view->document->refresh, ses->doc_view);
}
