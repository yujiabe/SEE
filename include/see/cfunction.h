/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_cfunction_
#define _h_cfunction_

#include "object.h"

struct SEE_interpeter;
struct SEE_string;

struct SEE_object *SEE_cfunction_make(struct SEE_interpreter *i,
	SEE_call_fn_t func, struct SEE_string *, int argc);

#endif /* _h_cfunction_ */
