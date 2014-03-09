#ifndef EL__PROTOCOL_HEADER_H
#define EL__PROTOCOL_HEADER_H

enum parse_header_param {
	HEADER_PARAM_FOUND,
	HEADER_PARAM_NOT_FOUND,
	HEADER_PARAM_OUT_OF_MEMORY
	/* Might add HEADER_PARAM_SYNTAX_ERROR in a later version.
	 * Unknown values should be treated as errors. */
};

char *parse_header(char *, const char *, char **);
enum parse_header_param parse_header_param(char *, char *, char **);
char *get_header_param(char *, char *);

#endif
