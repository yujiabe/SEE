/* $Id$ */
void shell_strings(void);
void shell_add_globals(struct SEE_interpreter *);
void shell_add_document(struct SEE_interpreter *);

#if HAVE_READLINE
# if HAVE_READLINE_H
#  include <readline.h>
# else
#  if HAVE_READLINE_READLINE_H
#   include <readline/readline.h>
#  endif
# endif
#else
char *readline(const char *);
#endif

extern struct SEE_string *s_interactive;
