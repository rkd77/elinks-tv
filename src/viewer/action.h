#ifndef EL__VIEWER_ACTION_H
#define EL__VIEWER_ACTION_H

#include "config/kbdbind.h"

struct session;

enum frame_event_status {
	/* The event was not handled */
	FRAME_EVENT_IGNORED,
	/* The event was handled, and the screen should be redrawn */
	FRAME_EVENT_REFRESH,
	/* The event was handled, and the screen should _not_ be redrawn */
	FRAME_EVENT_OK,
	/* The event was handled, and the current session was destroyed */
	FRAME_EVENT_SESSION_DESTROYED,
};

enum frame_event_status do_action(struct session *ses,
                                  action_id_T action_id, int verbose);

void goto_url_action(struct session *ses, unsigned char *(*get_url)(struct session *, unsigned char *, size_t));


#endif
