/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_enumerate_
#define _SEE_h_enumerate_

struct SEE_string;
struct SEE_object;

struct SEE_string **SEE_enumerate(struct SEE_interpreter *i,
	struct SEE_object *o);

#endif /* _SEE_h_enumerate_ */
