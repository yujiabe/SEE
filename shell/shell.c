/*
 * SEE shell environment objects
 *
 * The shell provides a couple of objects useful for testing.
 *
 *  print	- a function object which takes one
 *		  string argument and prints it, followed
 *		  by a newline.
 *  version	- a function that changes compatibility flags
 *		  based on an integer argument
 *  compat      - changes the compat flags. Returns old compat flags
 *  gc          - may force a garbage collection
 *
 *  Shell.args  - command line arguments [TODO]
 *  Shell.exit  - function to force immediate exit
 *  Shell.gcdump - calls GC_dump(), if it was detected
 *
 * In HTML mode the following objects are provided:
 *
 *  document		- a simple Object with some properties:
 *  document.write	- a function to print a string to stdout
 *  document.navigator	- a simple Object used as a placeholder
 *  document.userAgent 	- "SEE-shell"
 *  document.window	- a reference to the Global object
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
#include "compat.h"

/* Prototypes */
static void print_fn(struct SEE_interpreter *, struct SEE_object *, 
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void document_write_fn(struct SEE_interpreter *, struct SEE_object *, 
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void compat_fn(struct SEE_interpreter *, struct SEE_object *, 
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void version_fn(struct SEE_interpreter *, struct SEE_object *, 
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void gc_fn(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void shell_gcdump_fn(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void shell_exit_fn(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

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
	SEE_intern_global("gcdump");
	SEE_intern_global("gc");
	SEE_intern_global("exit");
	SEE_intern_global("args");
	SEE_intern_global("Shell");
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

/*
 * A function to modify the compatibility flags at runtime.
 */
static void
compat_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
        struct SEE_value v;
	char *buf;
	int i;
	struct SEE_string *old;
	
	old = compat_tostring(interp, interp->compatibility);
	if (argc > 0 && SEE_VALUE_GET_TYPE(argv[0]) != SEE_UNDEFINED) {
		SEE_ToString(interp, argv[0], &v);

		/* Convert argument to an ASCII C string */
		buf = SEE_STRING_ALLOCA(interp, char, v.u.string->length + 1);
		for (i = 0; i < v.u.string->length; i++)
		    if (v.u.string->data[i] > 0x7f)
		        SEE_error_throw(interp, interp->RangeError, 
			   "argument is not ASCII");
		    else
		    	buf[i] = v.u.string->data[i] & 0x7f;
		buf[i] = '\0';

		if (compat_fromstring(buf, &interp->compatibility) == -1)
		        SEE_error_throw(interp, interp->Error, 
			   "invalid flags");
	}

        SEE_SET_STRING(res, old);
}

/*
 * Query/change the Netscape JavaScript version compatibility value.
 *
 * If no argument is supplied, returns the current version number.
 * If a number is supplied, it is expected to be one of
 *	120, 130, 140, 150 indicating
 * Netscape JavaScript 1.0, 1.1, 1.2 etc.
 *
 * http://www.mozilla.org/rhino/overview.html#versions
 */
static void
version_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
	SEE_number_t ver;
	struct SEE_value v;
	int compat;

	if (argc == 0) {
	    switch (SEE_GET_JS_COMPAT(interp)) {
	    case SEE_COMPAT_JS11: ver = 110; break;
	    case SEE_COMPAT_JS12: ver = 120; break;
	    case SEE_COMPAT_JS13: ver = 130; break;
	    case SEE_COMPAT_JS14: ver = 140; break;
	    default:
	    case SEE_COMPAT_JS15: ver = 150; break;
	    }
	    SEE_SET_NUMBER(res, ver);
	    return;
	}

	SEE_ToNumber(interp, argv[0], &v);
	ver = v.u.number;
	if (ver >= 150)
		compat = SEE_COMPAT_JS15;
	else if (ver >= 140)
		compat = SEE_COMPAT_JS14;
	else if (ver >= 130)
		compat = SEE_COMPAT_JS13;
	else if (ver >= 120)
		compat = SEE_COMPAT_JS12;
	else if (ver >= 110)
		compat = SEE_COMPAT_JS11;
	else 
		SEE_error_throw(interp, interp->RangeError, 
		   "cannot set version lower than JS1.1");

	SEE_SET_JS_COMPAT(interp, compat);
	SEE_SET_UNDEFINED(res);
}

/*
 * Dump the garbage collector.
 */
static void
shell_gcdump_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
#if HAVE_GC_DUMP
	void GC_dump(void);

	GC_dump();
#endif
        SEE_SET_UNDEFINED(res);
}

/*
 * Exit the shell process
 */
static void
shell_exit_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
	SEE_uint16_t exitcode = 0;

	if (argc > 0)
		exitcode = SEE_ToUint16(interp, argv[0]);
	exit(exitcode);
	/* NOTREACHED */
}

/*
 * Force a complete garbage collection
 */
static void
gc_fn(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
	SEE_gcollect(interp);
        SEE_SET_UNDEFINED(res);
}

/*
 * Adds global symbols 'print' and 'version' to the interpreter.
 * 'print' is a function and 'version' will be the undefined value.
 */
void
shell_add_globals(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_value v;
	struct SEE_object *Shell;

	/* Create the print function, and attch to the Globals */
	SEE_CFUNCTION_PUTA(interp, interp->Global, 
		"print", print_fn, 1, 0);

	SEE_CFUNCTION_PUTA(interp, interp->Global, 
		"compat", compat_fn, 1, 0);

	SEE_CFUNCTION_PUTA(interp, interp->Global, 
		"gc", gc_fn, 0, 0);

	SEE_CFUNCTION_PUTA(interp, interp->Global, 
		"version", version_fn, 1, 0);

	/* Create the Shell object */
	Shell = SEE_Object_new(interp);
	SEE_SET_OBJECT(&v, Shell);
	SEE_OBJECT_PUTA(interp, interp->Global, 
		"Shell", &v, SEE_ATTR_DEFAULT);

	SEE_CFUNCTION_PUTA(interp, Shell,
		"gcdump", shell_gcdump_fn, 0, 0);

	SEE_CFUNCTION_PUTA(interp, Shell,
		"exit", shell_exit_fn, 0, 0);

	/* TODO: args */
}

/*
 * A write function that prints its output to stdout.
 * No newline is appended.
 */
static void
document_write_fn(interp, self, thisobj, argc, argv, res)
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
	SEE_CFUNCTION_PUTA(interp, document, "write", document_write_fn, 1, 0);

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
