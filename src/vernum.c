/* Version numbers etc; always rebuilt to reflect the latest info */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "vernum.h"

const unsigned char *build_date = (const unsigned char *)__DATE__;
const unsigned char *build_time = (const unsigned char *)__TIME__;
const unsigned char *build_id = (const unsigned char *)BUILD_ID;
