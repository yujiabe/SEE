/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

#ifndef _h_no_
#define _h_no_

struct SEE_object;
struct SEE_string;
struct SEE_value;

void	SEE_no_get(struct SEE_interpreter *, struct SEE_object *, 
		struct SEE_string *, struct SEE_value *val);
void	SEE_no_put(struct SEE_interpreter *, struct SEE_object *, 
		struct SEE_string *, struct SEE_value *val, int);
int	SEE_no_canput(struct SEE_interpreter *, struct SEE_object *,
		struct SEE_string *);
int	SEE_no_hasproperty(struct SEE_interpreter *, struct SEE_object *, 
		struct SEE_string *);
int	SEE_no_delete(struct SEE_interpreter *, struct SEE_object *, 
		struct SEE_string *);
void	SEE_no_defaultvalue(struct SEE_interpreter *, struct SEE_object *, 
		struct SEE_value *, struct SEE_value *);
struct SEE_enum *SEE_no_enumerator(struct SEE_interpreter *, 
		struct SEE_object *);

#endif /* _h_no_ */
