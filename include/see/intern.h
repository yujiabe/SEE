/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_intern_
#define _h_intern_

struct SEE_interpreter;
struct SEE_string;

struct SEE_string *SEE_intern(struct SEE_interpreter *i, struct SEE_string *s);

void SEE_intern_init(struct SEE_interpreter *i);

#endif /* _h_intern_ */
