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
 */

#include <stdio.h>
#include <see/see.h>
#include "shell.h"

static void print_fn(struct SEE_interpreter *,
        struct SEE_object *, struct SEE_object *, int,
	struct SEE_value **, struct SEE_value *);

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
		printf("\n");
		fflush(stdout);
        }
        SEE_SET_UNDEFINED(res);
}

void
shell_add_globals(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_object *obj;
	struct SEE_value v;
	struct SEE_string *name;

	name = SEE_string_sprintf(interp, "print");
	obj = SEE_cfunction_make(interp, print_fn, name, 1);
	SEE_SET_OBJECT(&v, obj);
	SEE_OBJECT_PUT(interp, interp->Global, name, &v, 0);

	name = SEE_string_sprintf(interp, "version");
	SEE_SET_UNDEFINED(&v);
	SEE_OBJECT_PUT(interp, interp->Global, name, &v, 0);
}

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

void
shell_add_document(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_object *obj, *document, *navigator;
	struct SEE_value v;
	struct SEE_string *name;

	name = SEE_string_sprintf(interp, "document");
	document = SEE_Object_new(interp);
	SEE_SET_OBJECT(&v, document);
	SEE_OBJECT_PUT(interp, interp->Global, name, &v, 0);

	name = SEE_string_sprintf(interp, "write");
	obj = SEE_cfunction_make(interp, document_write, name, 1);
	SEE_SET_OBJECT(&v, obj);
	SEE_OBJECT_PUT(interp, document, name, &v, 0);

	name = SEE_string_sprintf(interp, "navigator");
	navigator = SEE_Object_new(interp);
	SEE_SET_OBJECT(&v, navigator);
	SEE_OBJECT_PUT(interp, interp->Global, name, &v, 0);

	name = SEE_string_sprintf(interp, "userAgent");
	SEE_SET_STRING(&v, SEE_string_sprintf(interp, 
		"SEE-shell (" PACKAGE "-" VERSION ")" ));
	SEE_OBJECT_PUT(interp, navigator, name, &v, 0);

	name = SEE_string_sprintf(interp, "window");
	SEE_SET_OBJECT(&v, interp->Global);
	SEE_OBJECT_PUT(interp, interp->Global, name, &v, 0);

}
