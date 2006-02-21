/* David Leonard, 2006. Released into the public domain. */
/* $Id$ */

/*
 * This module is intended to provide an example of how to write a module
 * and host objects for SEE. 
 * When loaded, it provides the following objects based on stdio.
 *
 *	File			- constructor/opener object
 *	File.prototype		- container object for common methods
 *	File.prototype.read	- reads data from the file
 *	File.prototype.eof	- tells if a file is at EOF
 *	File.prototype.write	- writes data to a file
 *	File.prototype.flush	- flushes a file output
 *	File.prototype.close	- closes a file
 *	File.FileError		- object thrown when an error occurs
 *	File.In			- standard input
 *	File.Out		- standard output
 *	File.Err		- standard error
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <see/see.h>

/* Prototypes */
static int File_mod_init(void);
static void File_alloc(struct SEE_interpreter *);
static void File_init(struct SEE_interpreter *);

static void file_construct(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

static void file_proto_read(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void file_proto_eof(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void file_proto_write(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void file_proto_flush(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void file_proto_close(struct SEE_interpreter *, struct SEE_object *,
        struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

static struct file_object *tofile(struct SEE_interpreter *interp,
	struct SEE_object *);
static struct SEE_object *newfile(struct SEE_interpreter *interp,
	FILE *file);
static void file_finalize(struct SEE_interpreter *, void *, void *);

/*
 * This is the only symbol exported by this module. 
 * It contains some information for debugging and 
 * pointers to the major initialisation functions.
 * This structure is passed to SEE_module_add() once and early.
 */
struct SEE_module File_module = {
	SEE_MODULE_MAGIC,		/* magic */
	"File",				/* name */
	"1.0",				/* version */
	0,				/* index (set by SEE) */
	File_mod_init,			/* mod_init */
	File_alloc,			/* alloc */
	File_init			/* init */
};

/*
 * We use a private structure to hold per-interpeter data private to this
 * module. It can be accessed through the SEE_MODULE_PRIVATE() macro, or
 * through the simpler PRIVATE macro that we define below.
 * The private data we hold is simply original pointers to the objects
 * that we make during alloc/init. This is because (a) a script is able to
 * change the objects locations at runtime, and (b) this is slightly more
 * efficient than performing a runtime lookup with SEE_OBJECT_GET.
 */
struct module_private {
	struct SEE_object *File;		/* The File object */
	struct SEE_object *File_prototype;	/* File.prototype */
	struct SEE_object *FileError;		/* File.FileError */
};

#define PRIVATE(interp)  \
	((struct module_private *)SEE_MODULE_PRIVATE(interp, &File_module))

/*
 * Global, interned strings and a STR() macro that make code easier 
 * to understand. 
 * The STR() macro simply returns a pointer to a string internalised
 * during mod_init.
 * Internalised strings are guaranteed to have unique pointers,
 * which means you can use '==' instead of 'strcmp()' to compare names.
 * The pointers must all be obtained by mod_init().
 */
#define STR(name) s_##name
static struct SEE_string 
	*STR(Err),
	*STR(File),
	*STR(FileError),
	*STR(In),
	*STR(Out),
	*STR(close),
	*STR(eof),
	*STR(flush),
	*STR(prototype),
	*STR(read),
	*STR(write);

/*
 * The module initialisation function (mod_init).
 * This function is called exactly once, before any interpreters have
 * been created.
 * You can use this to set up global intern strings (like I've done),
 * and/or to initialise global data independent of any single interpreter.
 */
static int
File_mod_init()
{
	STR(Err)       = SEE_intern_global("Err");
	STR(File)      = SEE_intern_global("File");
	STR(FileError) = SEE_intern_global("FileError");
	STR(In)        = SEE_intern_global("In");
	STR(Out)       = SEE_intern_global("Out");
	STR(close)     = SEE_intern_global("close");
	STR(eof)       = SEE_intern_global("eof");
	STR(flush)     = SEE_intern_global("flush");
	STR(prototype) = SEE_intern_global("prototype");
	STR(read)      = SEE_intern_global("read");
	STR(write)     = SEE_intern_global("write");
	return 0;
}

/*
 * This next structure is the one used to represent a stdio file object
 * in SEE.
 * The 'struct file_object' wraps a SEE_native object, but I could have
 * chosen SEE_object if I didn't want users to be able to store values
 * on it.
 *
 * Instances of this structure can be passed wherever a SEE_object 
 * or SEE_native is needed. (That's because the first field of a SEE_native
 * is a SEE_object.) Each file instance's native.object.objectclass field
 * points to the file_inst_class table defined next. And each file instance's
 * [[Prototype]] (or native.object.prototype field) will point to the
 * File.prototype object so that the file methods are found by a normal
 * prototype-chain search. (Thanks SEE_native_get.)
 *
 * The special object File.prototype is also a file_object, except that its
 * file pointer is NULL. This is checked for in all the code below.
 */
struct file_object {
	struct SEE_native	 native;
	FILE			*file;
};

/*
 * A class structure describes how to carry out all object operations
 * on an object. For the file instance object class, you will note that
 * many of the function pointers are to SEE_native_* functions. These
 * provide the standard ECMAScript behaviour for native objects, and
 * work because of the 'struct SEE_native' at the beginning of the
 * structure.
 *
 * File instance objects have no [[Construct]] nor [[Call]] property,
 * so those are left at NULL.
 */
static struct SEE_objectclass file_inst_class = {
	"File",				/* Class */
	SEE_native_get,			/* Get */
	SEE_native_put,			/* Put */
	SEE_native_canput,		/* CanPut */
	SEE_native_hasproperty,		/* HasProperty */
	SEE_native_delete,		/* Delete */
	SEE_native_defaultvalue,	/* DefaultValue */
	SEE_native_enumerator,		/* DefaultValue */
	NULL,				/* Construct */
	NULL				/* Call */
};

/*
 * This class structure is for the toplevel File object, which doubles
 * as both a constructor function and as a container to hold File.prototype
 * and some other useful objects. File has only one important property, 
 * namely the [[Construct]] method.
 */
static struct SEE_objectclass file_constructor_class = {
	"File",				/* Class */
	SEE_native_get,			/* Get */
	SEE_native_put,			/* Put */
	SEE_native_canput,		/* CanPut */
	SEE_native_hasproperty,		/* HasProperty */
	SEE_native_delete,		/* Delete */
	SEE_native_defaultvalue,	/* DefaultValue */
	SEE_native_enumerator,		/* DefaultValue */
	file_construct,			/* Construct */
	NULL				/* Call */
};

/*
 * Module per-interpreter allocation (alloc)
 * This function is called during interpreter initialisation;
 * The interpreter is not ready for use; only some storage has been
 * allocated. This is useful if you have dependent modules that
 * during init() need to lookup pointers in other modules. Here
 * we use the time to allocate the per-interpreter private 
 * module structure storage.
 */
static void
File_alloc(interp)
	struct SEE_interpreter *interp;
{
	SEE_MODULE_PRIVATE(interp, &File_module) =
		SEE_NEW(interp, struct module_private);
}

/*
 * Module per-interpreter initialisation (init)
 * This is the workhorse of the module. Its job is to build up
 * a fresh collection of host objects and install them into the new 
 * interpreter instance.
 */
static void
File_init(interp)
	struct SEE_interpreter *interp;
{
	struct SEE_object *File;
	struct SEE_object *File_prototype;
	struct SEE_value v;

	/* Create the File.prototype object */
	File_prototype = (struct SEE_object *)SEE_NEW(interp, 
		struct file_object);
	SEE_native_init((struct SEE_native *)File_prototype, interp,
		&file_inst_class, interp->Object_prototype);
	((struct file_object *)File_prototype)->file = NULL;
	PRIVATE(interp)->File_prototype = File_prototype;

	/* Convenience macro for adding functions to File.prototype */
#define PUTFUNC(obj, name, len) 					\
	SEE_SET_OBJECT(&v, SEE_cfunction_make(interp, file_proto_##name,\
		STR(name), len));					\
	SEE_OBJECT_PUT(interp, obj, STR(name), &v, SEE_ATTR_DEFAULT);

	PUTFUNC(File_prototype, read, 0)
	PUTFUNC(File_prototype, write, 1)
	PUTFUNC(File_prototype, close, 0)
	PUTFUNC(File_prototype, eof, 0)
	PUTFUNC(File_prototype, flush, 0)

	/* Create the File object */
	File = (struct SEE_object *)SEE_NEW(interp, struct SEE_native);
	SEE_native_init((struct SEE_native *)File, interp,
		&file_constructor_class, interp->Object_prototype);
	PRIVATE(interp)->File = File;

	/* Convenience macro for adding properties to File */
#define PUTOBJ(parent, name, obj) 					\
	SEE_SET_OBJECT(&v, obj);					\
	SEE_OBJECT_PUT(interp, parent, STR(name), &v, SEE_ATTR_DEFAULT);

	PUTOBJ(File, prototype, File_prototype)
	PUTOBJ(File, In, newfile(interp, stdin))
	PUTOBJ(File, Out, newfile(interp, stdout))
	PUTOBJ(File, Err, newfile(interp, stderr))

	/* Create an FileError error object for I/O exceptions */
	PRIVATE(interp)->FileError = SEE_Error_make(interp, 
		STR(FileError));
	PUTOBJ(File, FileError, PRIVATE(interp)->FileError);

	/* Finally, insert File into the Global object */
	PUTOBJ(interp->Global, File, File);
}

/*
 * Converts an object into file_object, or throws a TypeError.
 *
 * This helper functon is called by the member functions of File
 * instances, mainly to check that it is being called correctly.
 * Because a script may assign the member functions to a different
 * (non-file) object and invoke them, thisobj cannot always be 
 * assumed to be a file instance.
 */
static struct file_object *
tofile(interp, o)
	struct SEE_interpreter *interp;
	struct SEE_object *o;
{
	if (o->objectclass != &file_inst_class)
		SEE_error_throw(interp, interp->TypeError, NULL);
	return (struct file_object *)o;
}

/*
 * This helper function constructs and returns a new instance of a 
 * file_object initialised to the given file pointer.
 */
static struct SEE_object *
newfile(interp, file)
	struct SEE_interpreter *interp;
	FILE *file;
{
	struct file_object *obj;

	obj = SEE_NEW_FINALIZE(interp, struct file_object, file_finalize, NULL);
	SEE_native_init(&obj->native, interp,
		&file_inst_class, PRIVATE(interp)->File_prototype);
	obj->file = file;
	return (struct SEE_object *)obj;
}

/*
 * A finalizer function that is /eventually/ called on file objects,
 * unless a system crash or exit occurs. SEE doesn't guarantee that
 * this is ever called; however the GC implementation may.
 */
static void
file_finalize(interp, obj, closure)
	struct SEE_interpreter *interp;
	void *obj, *closure;
{
	struct file_object *fo = (struct file_object *)obj;

	if (fo->file) {
		fclose(fo->file);
		fo->file = NULL;
	}
}

/*
 * The File.[[Construct]] property is called as a function
 * when the user writes "new File()". This constructor expects
 * two arguments (one is optional): 
 *	argv[0]	the filename to open
 *	argv[1] the file mode (eg "r", "+b", etc) defaults to "r"
 */
static void
file_construct(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	char *mode;
	char modebuf[10];
	char path[1024];
	struct SEE_value v;
	FILE *file;

	if (argc < 1)
		SEE_error_throw(interp, interp->RangeError, "missing argument");
	SEE_ToString(interp, argv[0], &v);
	SEE_string_toutf8(interp, path, sizeof path, v.u.string);

	/*
	 * It is a javascript idiom to treat 'undefined' arguments
	 * as if they didn't exist.
	 */
	if (argc < 2 || SEE_VALUE_GET_TYPE(argv[1]) == SEE_UNDEFINED)
		mode = "r";
	else {
		SEE_ToString(interp, argv[1], &v);
		SEE_string_toutf8(interp, modebuf, sizeof modebuf, v.u.string);
		mode = modebuf;
	}

	file = fopen(path, mode);
	if (!file)
		SEE_error_throw(interp, PRIVATE(interp)->FileError,
			"%s", strerror(errno));

	SEE_SET_OBJECT(res, newfile(interp, file));
}

/*
 * Reads and returns string data. If an argument is given, it limits the 
 * length of the string read. Closed files return undefined.
 */
static void
file_proto_read(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct file_object *fo = tofile(interp, thisobj);
	SEE_uint32_t len, i;
	struct SEE_string *buf;
	int unbound;

	if (argc == 0 || SEE_VALUE_GET_TYPE(argv[1]) == SEE_UNDEFINED) {
		unbound = 1;
		len = 1024;
	} else {
		unbound = 0;
		len = SEE_ToUint32(interp, argv[0]);
	}
	if (!fo->file) {
		SEE_SET_UNDEFINED(res);
		return;
	}
	buf = SEE_string_new(interp, len);
	for (i = 0; unbound || i < len; i++) {
		int ch = fgetc(fo->file);
		if (ch == EOF)
			break;
		SEE_string_addch(buf, ch);
	}
	SEE_SET_STRING(res, buf);
}

/*
 * Returns true if the last read resulted in an EOF. 
 * Closed files return EOF.
 */
static void
file_proto_eof(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct file_object *fo = tofile(interp, thisobj);

	if (!fo->file)
		SEE_SET_UNDEFINED(res);
	else
		SEE_SET_BOOLEAN(res, feof(fo->file));
}

/*
 * Writes the string argument to the file.
 * The string must be 8-bit data only.
 * Closed files throw an exception.
 */
static void
file_proto_write(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct file_object *fo = tofile(interp, thisobj);
	struct SEE_value v;
	unsigned int len;

	if (argc < 1)
		SEE_error_throw(interp, interp->RangeError, "missing argument");
	if (!fo->file)
		SEE_error_throw(interp, PRIVATE(interp)->FileError, 
			"file is closed");
	SEE_ToString(interp, argv[0], &v);
	for (len = 0; len < v.u.string->length; len++) {
	    if (v.u.string->data[len] > 0xff)
		SEE_error_throw(interp, interp->RangeError, 
			"bad data");
	    if (fputc(v.u.string->data[len], fo->file) == EOF)
		SEE_error_throw(interp, PRIVATE(interp)->FileError, 
			"write error");
	}
	SEE_SET_UNDEFINED(res);
}

/* Flushes the file, if not closed */
static void
file_proto_flush(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct file_object *fo = tofile(interp, thisobj);

	if (fo->file)
		fflush(fo->file);
	SEE_SET_UNDEFINED(res);
}

/* Closes the file */
static void
file_proto_close(interp, self, thisobj, argc, argv, res)
	struct SEE_interpreter *interp;
	struct SEE_object *self, *thisobj;
	int argc;
	struct SEE_value **argv, *res;
{
	struct file_object *fo = tofile(interp, thisobj);

	if (fo->file) {
		fclose(fo->file);
		fo->file = NULL;
	}
	SEE_SET_UNDEFINED(res);
}
