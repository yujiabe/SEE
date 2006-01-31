/* Copyright (c) 2003, David Leonard. All rights reserved. */
/* $Id$ */

/*
 * The SEE shell
 *
 * If no files are given, it prompts for javascript code interactively.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#if !HAVE_GETOPT
int getopt(int, char *const [], const char *);
extern char *optarg;
extern int optind;
#endif

#include <see/see.h>
#include "shell.h"
#include "debug.h"

/* Prototypes */
static void debug(struct SEE_interpreter *, int);
static void trace(struct SEE_interpreter *, struct SEE_throw_location *, 
        struct SEE_context *);
static int run_input(struct SEE_interpreter *, struct SEE_input *, 
        struct SEE_value *);
static int run_file(struct SEE_interpreter *, char *);
static void run_interactive(struct SEE_interpreter *);
static void run_html(struct SEE_interpreter *, char *);
static int compatvalue(const char *, int *);

static struct debug *debugger;

/* 
 * Enables the debugging flag given by character c.
 * This relies on the SEE library having been compiled with 
 * debugging support (the default).
 */
static void
debug(interp, c)
	struct SEE_interpreter *interp;
	int c;			/* promoted char */
{
#ifndef NDEBUG
	extern int SEE_native_debug, SEE_Error_debug, 
	SEE_parse_debug, SEE_lex_debug,
	SEE_eval_debug, SEE_error_debug,
	SEE_scope_debug, SEE_regex_debug,
	SEE_mem_debug;

	switch (c) {
	case 'E': SEE_Error_debug = 1; break;
	case 'T': interp->trace = trace; break;
	case 'e': SEE_error_debug = 1; break;
	case 'l': SEE_lex_debug = 1; break;
	case 'm': SEE_mem_debug = 1; break;
	case 'n': SEE_native_debug = 1; break;
	case 'p': SEE_parse_debug = 1; break;
	case 'r': SEE_regex_debug = 1; break;
	case 's': SEE_scope_debug = 1; break;
	case 'v': SEE_eval_debug = 1; break;
	default:
		fprintf(stderr, "unknown debug flag '%c'\n", c);
	}
#endif
}

/*
 * Trace function callback: prints current location to stderr.
 * This function is called when the -dT flag is given to enable
 * tracing. It is called by the parser at each evaluation step in
 * the program parse tree.
 */
static void
trace(interp, loc, context)
	struct SEE_interpreter *interp;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
{
	if (loc) {
	    fprintf(stderr, "trace: ");
	    if (loc->filename) {
		SEE_string_fputs(loc->filename, stderr);
		fprintf(stderr, ", ");
	    }
	    fprintf(stderr, "line %d\n", loc->lineno);
	}
}

/*
 * Runs the input given by inp, printing any exceptions
 * to stderr.
 * This function first establishes a local exception catch context.
 * Next, it passes the unicode input provider ('inp') to the generic
 * evaluation procedure SEE_Global_eval which executes the program
 * text in the ECMAScript global context. This function also examines
 * the result of the evaluation, being careful to print out exceptions
 * correctly.
 */
static int
run_input(interp, inp, res)
	struct SEE_interpreter *interp;
	struct SEE_input *inp;
	struct SEE_value *res;
{
	struct SEE_value v;
	SEE_try_context_t ctxt, ctxt2;

	interp->traceback = NULL;
        SEE_TRY (interp, ctxt) {
	    if (debugger)
	        debug_eval(interp, debugger, inp, res);
	    else
	        SEE_Global_eval(interp, inp, res);
        }
        if (SEE_CAUGHT(ctxt)) {
            fprintf(stderr, "exception:\n");
            SEE_TRY(interp, ctxt2) {
                SEE_ToString(interp, SEE_CAUGHT(ctxt), &v);
                fprintf(stderr, "  ");
                SEE_string_fputs(v.u.string, stderr);
                fprintf(stderr, "\n");
#ifndef NDEBUG
                if (ctxt.throw_file)
		    fprintf(stderr, "  (thrown from %s:%d)\n", 
                        ctxt.throw_file, ctxt.throw_line);
#endif
		SEE_PrintTraceback(interp, stderr);
            }
            if (SEE_CAUGHT(ctxt2)) {
                fprintf(stderr, "exception thrown while printing exception");
		if (ctxt2.throw_file)
		    fprintf(stderr, " at %s:%d",
		        ctxt2.throw_file, ctxt2.throw_line);
                fprintf(stderr, "\n");
	    }
	    return 0;
        }
        return 1;
}

/*
 * Opens the file and runs the contents as if ECMAScript code.
 * This function converts a local file into a unicode input stream,
 * and then calls run_input() above.
 */
static int
run_file(interp, filename)
	struct SEE_interpreter *interp;
	char *filename;
{
	struct SEE_input *inp;
	struct SEE_value res;
	FILE *f;
	int ok;

	if (strcmp(filename, "-") == 0) {
		run_interactive(interp);
		return 1;
	}

	f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		return 0;
	}
	inp = SEE_input_file(interp, f, filename, NULL);
	ok = run_input(interp, inp, &res);
	SEE_INPUT_CLOSE(inp);
	return ok;
}

/*
 * Reads lines of ECMAscript from the user
 * and runs each entered line in the one interpreter instance.
 * This function implements the 'command-prompt' loop. Each line
 * of text typed in is converted to a unicode input stream, and
 * sent to the run_input() function above.
 */
static void
run_interactive(interp)
	struct SEE_interpreter *interp;
{
	char *line;
	struct SEE_input *inp;
	struct SEE_value res;
	int len;

	for (;;) {
	    line = readline("> ");
	    if (line == NULL)
		break;
	    len = strlen(line);
	    while (len && line[len-1] == '\\') {
		char *a, *b;
		a = line;
		b = readline("+ ");
		if (!b) break;
		line = (char *)malloc(len + strlen(b) + 1);
		memcpy(line, a, len - 1);
		line[len-1] = '\n';
		strcpy(line + len, b);
		free(a);
		free(b);
		len = strlen(line);
	    }
	    inp = SEE_input_utf8(interp, line);
	    inp->filename = SEE_intern_ascii(interp, "<interactive>");
	    if (run_input(interp, inp, &res)) {
		printf(" = ");
		SEE_PrintValue(interp, &res, stdout);
		printf("\n");
	    }
            SEE_INPUT_CLOSE(inp);
	    free(line);
	}
}

/* Convert a character to uppercase */
#undef toupper
#define toupper(c) 	(((c) >= 'a' && (c) <= 'z') ? (c)-'a'+'A' : (c))

/*
 * Runs script elements in a HTML file.
 * This function opens a text file, assuming it to be HTML. It copies
 * the HTML verbatim until it finds a <SCRIPT> tag. At that point, it
 * reads and executes the text up to the closing </SCRIPT> tag. Further
 * HTML text read is also copied to standard output.
 */
static void
run_html(interp, filename)
	struct SEE_interpreter *interp;
	char *filename;
{
	FILE *f;
	int ch;
	const char *script_start = "<SCRIPT";
	const char *script_end = "</SCRIPT";
	const char *p;
	struct SEE_string *s = SEE_string_new(interp, 0);
	struct SEE_string *filenamestr;
	int endpos;
	int lineno = 1, first_lineno;
	struct SEE_input *inp;

	f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		return;
	}

	filenamestr = SEE_string_sprintf(interp, "%s", filename);

	p = script_start;
	while ((ch = fgetc(f)) != EOF) {
	    if (ch == '\n' || ch == '\r') lineno++;
	    if (toupper(ch) != *p) {
		if (p != script_start)
		    printf("%.*s", p - script_start, script_start);
		p = script_start;
	    }
	    if (toupper(ch) == *p) {
		p++;
		if (!*p) {
		    /* skip to closing > */
		    while ((ch = fgetc(f)) != EOF)  {
		        if (ch == '\n' || ch == '\r') lineno++;
			if (ch == '>') break;
		    }
		    /* capture content up to the end tag */
		    s->length = 0;
		    first_lineno = lineno;
		    p = script_end;
		    endpos = 0;
		    while ((ch = fgetc(f)) != EOF) {
		        if (ch == '\n' || ch == '\r') lineno++;
			if (toupper(ch) != *p) {
			    p = script_end;
			    endpos = s->length;
			}
			if (toupper(ch) == *p) {
			    p++;
			    if (!*p) {
				/* truncate string and skip to closing > */
				s->length = endpos;
				while ((ch = fgetc(f)) != EOF) {
				    if (ch == '\n' || ch == '\r') lineno++;
				    if (ch == '>') break;
				}
				break;
			    }
			}
			SEE_string_addch(s, ch);
		    }

		    inp = SEE_input_string(interp, s);
		    inp->filename = filenamestr;
		    inp->first_lineno = first_lineno;
		    run_input(interp, inp, NULL);

		    p = script_start;
		    continue;
		}
	    } else
	        putchar(ch);
	}
	fclose(f);
}

/*
 * Convert a compatability flag name into an integer bit-flag.
 * The following compatbility flags are understood (see the documentation
 * for more details). They may be prefixed with 'no' to turn them off.
 *
 *  sgmlcom      - treat SGML comments in program text as normal comments
 *  utf_unsafe   - pass through invalid UTF-8 characters without error
 *  undefdef     - return undefined for unknown names instead of throwing
 *  262_3b       - provide the optional functions in section 3B of standard
 *  ext1         - enable local SEE extension set number 1
 *  arrayjoin1   - emulate old array.join(undefined) bug
 */
static int
compatvalue(name, compatibility)
	const char *name;
	int *compatibility;
{
	static struct { const char *name; int flag; } names[] = {
		{ "sgmlcom",	SEE_COMPAT_SGMLCOM },
		{ "utf_unsafe",	SEE_COMPAT_UTF_UNSAFE },
		{ "undefdef",	SEE_COMPAT_UNDEFDEF },
		{ "262_3b",	SEE_COMPAT_262_3B },
		{ "ext1",	SEE_COMPAT_EXT1 },
		{ "arrayjoin1",	SEE_COMPAT_ARRAYJOIN1 },
	};
	int i;
	int no = 0;
	int bit = 0;

	if (name[0] == 'n' && name[1] == 'o') {
		name += 2;
		no = 1;
	}
	for (i = 0; i < sizeof names / sizeof names[0]; i++) 
		if (strcmp(name, names[i].name) == 0) {
			bit = names[i].flag;
			break;
		}
	if (name[0] >= '0' && name[0] <= '9')
		bit = atoi(name);
	else if (bit == 0) {
		fprintf(stderr, "WARNING: unknown compatability flag '%s'\n",
		    name);
		return -1;
	}

	if (no)
		*compatibility &= ~bit;
	else
		*compatibility |= bit;
	return 0;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct SEE_interpreter interp;
	int ch, error = 0;
	int do_interactive = 1;
	int globals_added = 0;
	int document_added = 0;
	char *s;

	/* Initialise the shell's global strings */
	shell_strings();

	/* Initialise our interpreter */
	SEE_interpreter_init(&interp);
	interp.compatibility = SEE_COMPAT_STRICT;

	while (!error && (ch = getopt(argc, argv, "c:d:f:gh:r:V")) != -1)
	    switch (ch) {
	    case 'c':
		if (compatvalue(optarg, &interp.compatibility) == -1)
		    error = 1;
		break;
	    case 'd':
		if (*optarg == '*')
		    optarg = "nElpvecr";
		for (s = optarg; *s; s++)
		    debug(&interp, *s);
		break;
	    case 'f':
		if (!globals_added) {
		    shell_add_globals(&interp);
		    globals_added = 1;
		}
		do_interactive = 0;
		if (!run_file(&interp, optarg))
			exit(1);
		break;
	    case 'g':
	    	if (!debugger)
			debugger = debug_new(&interp);
		break;
	    case 'h':
		interp.compatibility |= SEE_COMPAT_SGMLCOM;
		if (!document_added) {
		    shell_add_document(&interp);
		    document_added = 1;
		}
		do_interactive = 0;
		run_html(&interp, optarg);
		break;
	    case 'r':
		interp.recursion_limit = atoi(optarg);
		printf("(Set recursion limit to %d)\n", 
			interp.recursion_limit);
		break;
	    case 'V':
	    	printf("SEE API version: %u.%u\n", SEE_VERSION_API_MAJOR,
			SEE_VERSION_API_MINOR);
	    	printf("Library version: %s\n", SEE_version());
		break;
	    default:
		error = 1;
	    }

	/* Don't expect any more arguments */
	if (optind < argc)
	    error = 1;

	if (error) {
	    fprintf(stderr, 
	        "usage: %s [-Vg] [-c flag] %s[-f file.js | -h file.html]...\n",
		argv[0],
#ifndef NDEBUG
	        "[-d[ETelmnprsv]] "
#else
		""
#endif
	    );
	    exit(1);
	}

	if (do_interactive) {
	    if (!globals_added)
		shell_add_globals(&interp);
	    run_interactive(&interp);
	}

	exit(0);
}
