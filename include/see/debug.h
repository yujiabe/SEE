/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_debug_
#define _h_debug_

#include <stdio.h>
struct SEE_value;
struct SEE_object;
struct SEE_interpreter;

void SEE_PrintValue(struct SEE_interpreter *i, struct SEE_value *v, FILE *f);
void SEE_PrintObject(struct SEE_interpreter *i, struct SEE_object *o, FILE *f);

#endif /* _h_debug_ */
