/* Copyright (c) 2005, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_dprintf_
#define _h_dprintf_

#include <see/string.h>
#include <see/interpreter.h>

void SEE_dprintf(const char *fmt, ...);
void SEE_dprints(struct SEE_string *s);
void SEE_dprintv(struct SEE_interpreter *interp, struct SEE_value *v);
void SEE_dprinto(struct SEE_interpreter *interp, struct SEE_object *o);

#ifndef NDEBUG
#define dprintf SEE_dprintf
#define dprints SEE_dprints
#define dprintv SEE_dprintv
#define dprinto SEE_dprinto
#endif

#endif /* _h_dprintf_ */

