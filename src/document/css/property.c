/** CSS property info
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/value.h"


/** @todo TODO: Use fastfind when we get a lot of properties.
 * XXX: But only _WHEN_ we get _A LOT_ of properties, zas! ;-) --pasky */
struct css_property_info css_property_info[CSS_PT_LAST] = {
	{ (unsigned char *)"background",		CSS_PT_BACKGROUND,	 CSS_VT_COLOR,		css_parse_background_value },
	{ (unsigned char *)"background-color",	CSS_PT_BACKGROUND_COLOR, CSS_VT_COLOR,		css_parse_color_value },
	{ (unsigned char *)"color",		CSS_PT_COLOR,		 CSS_VT_COLOR,		css_parse_color_value },
	{ (unsigned char *)"display",		CSS_PT_DISPLAY,		 CSS_VT_DISPLAY,	css_parse_display_value },
	{ (unsigned char *)"font-style",		CSS_PT_FONT_STYLE,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_font_style_value },
	{ (unsigned char *)"font-weight",	CSS_PT_FONT_WEIGHT,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_font_weight_value },
	{ (unsigned char *)"list-style",		CSS_PT_LIST_STYLE,	 CSS_VT_LIST_STYLE,	css_parse_list_style_value },
	{ (unsigned char *)"list-style-type",	CSS_PT_LIST_STYLE_TYPE,	 CSS_VT_LIST_STYLE,	css_parse_list_style_value },
	{ (unsigned char *)"text-align",		CSS_PT_TEXT_ALIGN,	 CSS_VT_TEXT_ALIGN,	css_parse_text_align_value },
	{ (unsigned char *)"text-decoration",	CSS_PT_TEXT_DECORATION,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_text_decoration_value },
	{ (unsigned char *)"white-space",	CSS_PT_WHITE_SPACE,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_white_space_value },

	{ NULL, CSS_PT_NONE, CSS_VT_NONE },
};
