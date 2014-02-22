/** Terminal windows stuff.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/menu.h"
#include "terminal/event.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"


void
redraw_windows(enum windows_to_redraw which, struct window *win)
{
	struct terminal *term = win->term;
	struct term_event ev;
	struct window *end;
	enum term_redrawing_state saved_redraw_state = term->redrawing;

	switch (which) {
	case REDRAW_IN_FRONT_OF_WINDOW:
		win = win->prev;
		/* fall through */
	case REDRAW_WINDOW_AND_FRONT:
		end = (struct window *) &term->windows;
		if (term->redrawing != TREDRAW_READY) return;
		term->redrawing = TREDRAW_BUSY;
		break;
	case REDRAW_BEHIND_WINDOW:
		end = win;
		win = (struct window *) term->windows.prev;
		if (term->redrawing == TREDRAW_DELAYED) return;
		term->redrawing = TREDRAW_DELAYED;
		break;
	default:
		ERROR("invalid enum windows_to_redraw: which==%d", (int) which);
		return;
	}

	set_redraw_term_event(&ev, term->width, term->height);
	for (; win != end; win = win->prev) {
		if (!inactive_tab(win))
			win->handler(win, &ev);
	}
	term->redrawing = saved_redraw_state;
}

void
add_window(struct terminal *term, window_handler_T handler, void *data)
{
	struct term_event ev;
	struct window *win = mem_calloc(1, sizeof(*win));

	if (!win) {
		mem_free_if(data);
		return;
	}

	win->handler = handler;
	win->data = data; /* freed later in delete_window() */
	win->term = term;
	win->type = WINDOW_NORMAL;
	add_at_pos((struct window *) &term->windows, win);
	set_init_term_event(&ev, term->width, term->height);
	win->handler(win, &ev);
}

void
delete_window(struct window *win)
{
	struct term_event ev;

	/* Updating the status when destroying tabs needs this before the win
	 * handler call. */
	del_from_list(win);
	set_abort_term_event(&ev);
	win->handler(win, &ev);
	mem_free_if(win->data);
	redraw_terminal(win->term);
	mem_free(win);
}

void
delete_window_ev(struct window *win, struct term_event *ev)
{
	struct window *w;

	w = list_has_next(win->term->windows, win) ? win->next : NULL;

	delete_window(win);

	if (!ev || !w) return;

	/* If next is a tab send it to the current tab */
	if (w->type == WINDOW_TAB) {
		w = get_current_tab(w->term);
	}

	if (w) w->handler(w, ev);
}

void
get_parent_ptr(struct window *win, int *x, int *y)
{
	struct window *parent = win->next;

	if (parent->type == WINDOW_TAB)
		parent = get_tab_by_number(win->term, win->term->current_tab);

	if (parent) {
		*x = parent->x;
		*y = parent->y;
	} else {
		*x = 0;
		*y = 0;
	}
}


struct ewd {
	void (*fn)(void *);
	void *data;
	unsigned int called_once:1;
};

static void
empty_window_handler(struct window *win, struct term_event *ev)
{
	struct terminal *term = win->term;
	struct ewd *ewd = win->data;
	void (*fn)(void *) = ewd->fn;
	void *data = ewd->data;

	if (ewd->called_once) return;

	switch (ev->ev) {
		case EVENT_INIT:
		case EVENT_RESIZE:
		case EVENT_REDRAW:
			get_parent_ptr(win, &win->x, &win->y);
			return;
		case EVENT_ABORT:
			fn(data);
			return;
		case EVENT_KBD:
		case EVENT_MOUSE:
			/* Silence compiler warnings */
			break;
	}

	ewd->called_once = 1;
	delete_window(win);
	fn(data);
	term_send_event(term, ev);
}

void
add_empty_window(struct terminal *term, void (*fn)(void *), void *data)
{
	struct ewd *ewd = mem_alloc(sizeof(*ewd));

	if (!ewd) return;
	ewd->fn = fn;
	ewd->data = data;
	ewd->called_once = 0;
	add_window(term, empty_window_handler, ewd);
}

#if CONFIG_DEBUG
/** Check that terminal.windows are in the documented order.  */
void
assert_window_stacking(struct terminal *term)
{
	enum { WANT_ANY, WANT_TAB, WANT_NONE } want = WANT_ANY;
	const struct window *win;
	const struct window *main_menu_win;

	/* The main menu can be either above or below the tabs.  */
	main_menu_win = term->main_menu ? term->main_menu->win : NULL;

	foreach (win, term->windows) {
		switch (want) {
		case WANT_ANY:
			if (win->type == WINDOW_TAB)
				want = WANT_TAB;
			break;
		case WANT_TAB:
			if (win == main_menu_win)
				want = WANT_NONE;
			else
				assert(win->type == WINDOW_TAB);
			break;
		case WANT_NONE:
			assert(0);
			break;
		}
	}
}
#endif	/* CONFIG_DEBUG */

void
set_dlg_window_ptr(struct dialog_data *dlg_data, struct window *window, int x, int y)
{
	struct box *box = &dlg_data->real_box;

	if (box->height) {
		int y_max = box->y + box->height;

		y -= dlg_data->y;
		if (y < box->y || y >= y_max) return;
	}
	set_window_ptr(window, x, y);
}

#if CONFIG_SCRIPTING_SPIDERMONKEY
/** Check whether keypress events would be directed to @a win.  */
int
would_window_receive_keypresses(const struct window *win)
{
	struct terminal *const term = win->term;
	const struct window *selected;

	/* At least @win must be in the list.  */
	assert(!list_empty(term->windows));
	if_assert_failed return 0;

	selected = term->windows.next;
	if (selected->type != WINDOW_TAB) return 0;

	selected = get_current_tab(term);
	if (selected != win) return 0;

	return 1;
}
#endif	/* CONFIG_SCRIPTING_SPIDERMONKEY */
