/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _SEE_h_array_
#define _SEE_h_array_

#include <see/type.h>

struct SEE_object;

int	SEE_is_Array(struct SEE_object *a);
void	SEE_Array_push(struct SEE_interpreter *i, struct SEE_object *a,
		struct SEE_value *val);
SEE_uint32_t SEE_Array_length(struct SEE_interpreter *i, struct SEE_object *a);

#endif /* _SEE_h_array_ */
