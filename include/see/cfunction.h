/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_cfunction_
#define _SEE_h_cfunction_

#include <see/object.h>

struct SEE_interpeter;
struct SEE_string;

struct SEE_object *SEE_cfunction_make(struct SEE_interpreter *i,
	SEE_call_fn_t func, struct SEE_string *, int argc);

#endif /* _SEE_h_cfunction_ */
