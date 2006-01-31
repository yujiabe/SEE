/*
 * SEE shell environment objects
 *
 * The shell provides a couple of objects useful for testing.
 *
 *  print	- a function object which takes one
 *		  string argument and prints it, followed
 *		  by a newline.
 *  version	- an undefined value, used to satisfy some tests.
 *
 * In HTML mode the following objects are provided:
 *
 *  document		- a simple Object with some properties:
 *  document.write	- a function to print a string to stdout
 *  document.navigator	- a simple Object used as a placeholder
 *  document.userAgent 	- "SEE-shell"
 *  document.window	- a reference to the Global object
 *
 * If the functions GC_dump and GC_gcollect are found during configure,
 * then the shell adds them as GC_dump and GC_gcollect.
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
#endif

#include <see/see.h>
#include "shell.h"

/* Prototypes */
static void print_fn(struct SEE_interpreter *, struct SEE_object *, 
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void document_write(struct SEE_interpreter *, struct SEE_object *, 
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
#if HAVE_GC_DUMP
static void gc_dump_fn(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
#endif
#if HAVE_GC_GCOLLECT
static void gc_gcollect_fn(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
#endif

/*
 * Adds useful symbols into the interpreter's internal symbol table. 
 * This speeds up access to the symbols and lets us use UTF-16 strings
 * natively when populating the object space. It's not strictly necessary
 * to do this.
 */
void
shell_strings()
{
	SEE_intern_global("print");
	SEE_intern_global("version");
	SEE_intern_global("document");
	SEE_intern_global("write");
	SEE_intern_global("navigator");
	SEE_intern_global("userAgent");
	SEE_intern_global("window");
	SEE_intern_global("gc_dump");
	SEE_intern_global("gc_gcollect");
}

/*
 * A print function that prints a string argument to stdout.
 * A newline is printed at the end.
 */
static void
print_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
        struct SEE_value v;

        if (argc) {
                SEE_ToString(interp, argv[0], &v);
                SEE_string_fputs(v.u.string, stdout);
        }
	printf("\n");
	fflush(stdout);
        SEE_SET_UNDEFINED(res);
}

#if HAVE_GC_DUMP
void GC_dump(void);
/*
 * Dump the garbage collector.
 */
static void
gc_dump_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
	GC_dump();
        SEE_SET_UNDEFINED(res);
}
#endif

#if HAVE_GC_GCOLLECT
void GC_gcollect(void);
/*
 * Force a complete garbage collection
 */
static void
gc_gcollect_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
	GC_gcollect();
        SEE_SET_UNDEFINED(res);
}
#endif

/*
 * Adds global symbols 'print' and 'version' to the interpreter.
 * 'print' is a function and 'version' will be the undefined value.
 */
void
shell_add_globals(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_value v;

	/* Create the print function, and attch to the Globals */
	SEE_CFUNCTION_PUTA(interp, interp->Global, 
		"print", print_fn, 1, 0);

#if HAVE_GC_DUMP
	/* Create the gc_dump function, and attach to the Globals. */
	SEE_CFUNCTION_PUTA(interp, interp->Global, 
		"gc_dump", gc_dump_fn, 0, 0);
#endif

#if HAVE_GC_GCOLLECT
	/* Create the gc_gcollect function, and attach to the Globals. */
	SEE_CFUNCTION_PUTA(interp, interp->Global, 
		"gc_collect", gc_gcollect_fn, 0, 0);
#endif

	SEE_SET_UNDEFINED(&v);
	SEE_OBJECT_PUTA(interp, interp->Global, "version", &v, 0);
}

/*
 * A write function that prints its output to stdout.
 * No newline is appended.
 */
static void
document_write(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
        struct SEE_value v;

        if (argc) {
                SEE_ToString(interp, argv[0], &v);
                SEE_string_fputs(v.u.string, stdout);
		fflush(stdout);
        }
        SEE_SET_UNDEFINED(res);
}

/*
 * Adds the following variables to emulate a browser environment:
 *   document             - dummy object
 *   document.write       - function to print strings
 *   navigator            - dummy object
 *   navigator.userAgent  - dummy string identifier for the SEE shell
 */
void
shell_add_document(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_object *document, *navigator;
	struct SEE_value v;

	/* Create a dummy 'document' object. Add it to the global space */
	document = SEE_Object_new(interp);
	SEE_SET_OBJECT(&v, document);
	SEE_OBJECT_PUTA(interp, interp->Global, "document", &v, 0);

	/* Create a 'write' method and attach to 'document'. */
	SEE_CFUNCTION_PUTA(interp, document, "write", document_write, 1, 0);

	/* Create a 'navigator' object and attach to the global space */
	navigator = SEE_Object_new(interp);
	SEE_SET_OBJECT(&v, navigator);
	SEE_OBJECT_PUTA(interp, interp->Global, "navigator", &v, 0);

	/* Create a string and attach as 'navigator.userAgent' */
	SEE_SET_STRING(&v, SEE_string_sprintf(interp, 
		"SEE-shell (%s-%s)", PACKAGE, VERSION));
	SEE_OBJECT_PUTA(interp, navigator, "userAgent", &v, 0);

	/* Create a dummy 'window' object */
	SEE_SET_OBJECT(&v, interp->Global);
	SEE_OBJECT_PUTA(interp, interp->Global, "window", &v, 0);
}
