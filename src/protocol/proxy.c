/* Proxy handling */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "main/event.h"
#include "network/connection.h"
#include "network/state.h"
#include "protocol/protocol.h"
#include "protocol/proxy.h"
#include "protocol/uri.h"
#include "util/memory.h"
#include "util/string.h"


static int
proxy_probe_no_proxy(unsigned char *url, unsigned char *no_proxy)
{
	unsigned char *slash = (unsigned char *)strchr((char *)url, '/');

	if (slash) *slash = '\0';

	while (no_proxy && *no_proxy) {
		unsigned char *jumper = (unsigned char *)strchr((char *)no_proxy, ',');

		skip_space(no_proxy);
		if (jumper) *jumper = '\0';

		if (c_strcasestr((const char *)url, (const char *)no_proxy)) {
			if (jumper) *jumper = ',';
			if (slash) *slash = '/';
			return 1;
		}
		no_proxy = jumper;
		if (jumper) {
			*jumper = ',';
			no_proxy++;
		}
	}

	if (slash) *slash = '/';
	return 0;
}

static struct uri *
proxy_uri(struct uri *uri, unsigned char *proxy,
          struct connection_state *error_state)
{
	struct string string;

	if (init_string(&string)
	    && string_concat(&string, "proxy://", proxy, "/",
			     (unsigned char *) NULL)
	    && add_uri_to_string(&string, uri, URI_BASE)) {
		/* There is no need to use URI_BASE when calling get_uri()
		 * because URI_BASE should not add any fragments in the first
		 * place. */
		uri = get_uri(string.source, 0);
		/* XXX: Assume the problem is due to @proxy having bad format.
		 * This is a lot faster easier than checking the format. */
		if (!uri)
			*error_state = connection_state(S_PROXY_ERROR);
	} else {
		uri = NULL;
		*error_state = connection_state(S_OUT_OF_MEM);
	}

	done_string(&string);
	return uri;
}

static unsigned char *
strip_proxy_protocol(unsigned char *proxy,
		     unsigned char *strip1, unsigned char *strip2)
{
	assert(proxy && *proxy);

	if (!c_strncasecmp((const char *)proxy, (const char *)strip1, strlen((const char *)strip1)))
		proxy += strlen((const char *)strip1);
	else if (strip2 && !c_strncasecmp((const char *)proxy, (const char *)strip2, strlen((const char *)strip2)))
		proxy += strlen((const char *)strip2);

	return proxy;
}

/* TODO: We could of course significantly simplify the calling convention by
 * autogenerating most of the parameters from protocol name. Having a function
 * exported by protocol/protocol.* dedicated to that would be nice too.
 * --pasky */
static unsigned char *
get_protocol_proxy(unsigned char *opt,
                   unsigned char *env1, unsigned char *env2,
		   unsigned char *strip1, unsigned char *strip2)
{
	unsigned char *proxy;

	proxy = get_opt_str(opt, NULL);
	if (!*proxy) proxy = (unsigned char *)getenv((const char *)env1);
	if (!proxy || !*proxy) proxy = (unsigned char *)getenv((const char *)env2);

	if (proxy && *proxy) {
		proxy = strip_proxy_protocol(proxy, strip1, strip2);
	}

	return proxy;
}

static struct uri *
get_proxy_worker(struct uri *uri, unsigned char *proxy,
                 struct connection_state *error_state)
{
	unsigned char *protocol_proxy = NULL;

	if (proxy) {
		if (*proxy) {
			proxy = strip_proxy_protocol(proxy, (unsigned char *)"http://", (unsigned char *)"ftp://");

			return proxy_uri(uri, proxy, error_state);
		}

		/* "" from script_hook_get_proxy() */
		return get_composed_uri(uri, URI_BASE);
	}

	switch (uri->protocol) {
	case PROTOCOL_HTTP:
		protocol_proxy = get_protocol_proxy((unsigned char *)"protocol.http.proxy.host",
						    (unsigned char *)"HTTP_PROXY", (unsigned char *)"http_proxy",
						    (unsigned char *)"http://", NULL);
		break;

	case PROTOCOL_HTTPS:
		/* As Timo Lindfors explains, the communication between ELinks
		 * and the proxy server is never encrypted, altho the proxy
		 * might be used to transfer encrypted data between Web client
		 * and Web server. (Some proxy servers might allow encrypted
		 * communication between the Web client and the proxy
		 * but ELinks does not support that.) */
		/* So, don't check whether the URI for the proxy begins
		 * with "https://" but rather check for "http://".
		 * Maybe we should allow either -- ELinks uses HTTP
		 * to communicate with the proxy when we use it for FTP, but we
		 * check for "ftp://" below; and what about 'be liberal in what
		 * you accept' (altho that is usually applied to data received
		 * from remote systems, not to user input)?  -- Miciah */
		protocol_proxy = get_protocol_proxy((unsigned char *)"protocol.https.proxy.host",
						    (unsigned char *)"HTTPS_PROXY", (unsigned char *)"https_proxy",
						    (unsigned char *)"http://", NULL);
		break;

	case PROTOCOL_FTP:
		protocol_proxy = get_protocol_proxy((unsigned char *)"protocol.ftp.proxy.host",
						    (unsigned char *)"FTP_PROXY", (unsigned char *)"ftp_proxy",
						    (unsigned char *)"ftp://", (unsigned char *)"http://");
		break;
	}

	if (protocol_proxy && *protocol_proxy) {
		unsigned char *no_proxy;
		unsigned char *slash = (unsigned char *)strchr((char *)protocol_proxy, '/');

		if (slash) *slash = 0;

		no_proxy = get_opt_str((const unsigned char *)"protocol.no_proxy", NULL);
		if (!*no_proxy) no_proxy = (unsigned char *)getenv("NO_PROXY");
		if (!no_proxy || !*no_proxy) no_proxy = (unsigned char *)getenv("no_proxy");

		if (!proxy_probe_no_proxy(uri->host, no_proxy))
			return proxy_uri(uri, protocol_proxy, error_state);
	}

	return get_composed_uri(uri, URI_BASE);
}

struct uri *
get_proxy_uri(struct uri *uri, struct connection_state *error_state)
{
	if (uri->protocol == PROTOCOL_PROXY) {
		return get_composed_uri(uri, URI_BASE);
	} else {
#ifdef CONFIG_SCRIPTING
		unsigned char *tmp = NULL;
		static int get_proxy_event_id = EVENT_NONE;

		set_event_id(get_proxy_event_id, "get-proxy");
		trigger_event(get_proxy_event_id, &tmp, struri(uri));

		uri = get_proxy_worker(uri, tmp, error_state);
		mem_free_if(tmp);
		return uri;
#else
		return get_proxy_worker(uri, NULL, error_state);
#endif
	}
}

struct uri *
get_proxied_uri(struct uri *uri)
{
	if (uri->protocol == PROTOCOL_PROXY)
		return get_uri(uri->data, URI_BASE);

	return get_composed_uri(uri, URI_BASE);
}
