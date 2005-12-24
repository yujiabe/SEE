/* Copyright (c) 2005, David Leonard. All rights reserved. */
/* $Id$ */

/*
 * A simple debugger.
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <see/see.h>
#include "shell.h"
#include "debug.h"

/* A breakpoint set by the user */
struct breakpoint {
	struct breakpoint *next;
	struct SEE_throw_location loc;
	int id;				/* unique id */
	int ignore_counter;		/* number of hits to ignore */
	int ignore_reset;
	int temporary;			/* remove this BP when hit */
};

/* The current state of the debugger */
struct debug {
	void *save_host_data;
	void (*save_trace)(struct SEE_interpreter *, 
		struct SEE_throw_location *, struct SEE_context *);
	struct SEE_throw_location last_location;
	int break_immediately;
	struct breakpoint *breakpoints;
	int next_bp_id;
};

/* Command table elements */
struct cmd {
	const char *name;
	int (*fn)(struct SEE_interpreter *, struct debug *, 
		struct SEE_throw_location *, struct SEE_context *,
		char *arg);
	const char *doc;
};

/* Prototypes */
static struct breakpoint *bp_add(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, int, int);
static void loc_print(FILE *, struct SEE_throw_location *);
static int bp_delete(struct SEE_interpreter *, struct debug *, int);
static void bp_print(struct SEE_interpreter *, struct breakpoint *, FILE *);
static void trace_callback(struct SEE_interpreter *, 
        struct SEE_throw_location *, struct SEE_context *);
static int location_matches(struct SEE_interpreter *, 
        struct SEE_throw_location *, struct SEE_throw_location *);
static int should_break(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *);
static int location_parse(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_throw_location *, char **);

static int cmd_where(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_step(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_cont(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_break(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static void bp_show(struct SEE_interpreter *, struct breakpoint *);
static int cmd_show(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_delete(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_eval(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_throw(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_help(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int cmd_info(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *, char *);
static int user_command(struct SEE_interpreter *, struct debug *, 
        struct SEE_throw_location *, struct SEE_context *);

static struct cmd cmdtab[] = {
    { "break",	cmd_break,	"set a breakpoint" },
    { "cont",	cmd_cont,	"continue running" },
    { "delete",	cmd_delete,	"delete a breakpoint" },
    { "eval",	cmd_eval,	"evaluate an expression" },
    { "help",	cmd_help,	"print this information" },
    { "info",	cmd_info,	"print context information" },
    { "show",	cmd_show,	"show current breakpoints" },
    { "step",	cmd_step,	"run until next line change" },
    { "throw",	cmd_throw,	"evaluate an expression and throw it" },
    { "where",	cmd_where,	"show traceback" },
    { NULL }
};


/*
 * Public API 
 */

/* Allocates a new debugger context. There should only be one per interp. */
struct debug *
debug_new(interp)
	struct SEE_interpreter *interp;
{
	struct debug *debug;
	
	debug = SEE_NEW(interp, struct debug);
	debug->last_location.filename = NULL;
	debug->break_immediately = 1;
	debug->breakpoints = NULL;
	debug->next_bp_id = 0;
	return debug;
}

/* Evaluate input inside the debugger */
void
debug_eval(interp, debug, input, res)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_input *input;
	struct SEE_value *res;
{
	void *save_host_data;
	void (*save_trace)(struct SEE_interpreter *, 
		struct SEE_throw_location *, struct SEE_context *);
	SEE_try_context_t ctxt;

	save_host_data = debug->save_host_data;
	save_trace = debug->save_trace;
	debug->save_host_data = interp->host_data;
	debug->save_trace = interp->trace;
	interp->host_data = debug;
	interp->trace = trace_callback;
	SEE_TRY(interp, ctxt) {
		SEE_Global_eval(interp, input, res);
	}
	/* finally */
	interp->host_data = debug->save_host_data;
	interp->trace = debug->save_trace;
	debug->save_host_data = save_host_data;
	debug->save_trace = save_trace;
	SEE_DEFAULT_CATCH(interp, ctxt);
}

/*
 * Internal functions
 */

/* Adds a breakpoint to the breakpoint list, assigning it a unique number */
static struct breakpoint *
bp_add(interp, debug, loc, ignore, temporary)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	int ignore, temporary;
{
	struct breakpoint *bp = SEE_NEW(interp, struct breakpoint);

	bp->loc.filename = loc->filename;
	bp->loc.lineno = loc->lineno;
	bp->id = ++debug->next_bp_id;
	bp->ignore_counter = bp->ignore_reset = ignore;
	bp->temporary = temporary;

	bp->next = debug->breakpoints;
	debug->breakpoints = bp;
	return bp;
}

static void
loc_print(file, loc)
	FILE *file;
	struct SEE_throw_location *loc;
{
	if (!loc || !loc->filename)
		fprintf(file, "<nowhere>");
	else {
		SEE_string_fputs(loc->filename, file);
		fprintf(file, ":%d", loc->lineno);
	}
}

/* Deletes a breakpoint by its unique number. Returns true if deleted. */
static int
bp_delete(interp, debug, id)
	struct SEE_interpreter *interp;
	struct debug *debug;
	int id;
{
	struct breakpoint *bp, **nbp;

	for (bp = *(nbp = &debug->breakpoints); bp; bp = *(nbp = &(bp->next)))
		if (bp->id == id) {
		    *nbp = bp->next;
		    return 1;
		}
	return 0;
}


/* Prints a breakpoint in human-readable form */
static void
bp_print(interp, bp, file)
	struct SEE_interpreter *interp;
	struct breakpoint *bp;
	FILE *file;
{
	fprintf(file, "#%d ", bp->id);
	loc_print(file, &bp->loc);
	if (bp->ignore_reset)
		fprintf(file, " (remain %d reset %d)", 
			bp->ignore_counter, bp->ignore_reset);
	if (bp->temporary)
		fprintf(file, "[temp]");
}


/*
 * Callback function invoked during each step inside the debugger.
 * Transfers control to the user if a breakpoint is hit.
 */
static void
trace_callback(interp, loc, context)
	struct SEE_interpreter *interp;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
{
	struct debug *debug;
	SEE_try_context_t ctxt;

	debug = (struct debug *)interp->host_data;

	/* Call any inner trace functions */
	if (debug->save_trace) {
		interp->host_data = debug->save_host_data;
		interp->trace = debug->save_trace;
		SEE_TRY(interp, ctxt) {
			(*interp->trace)(interp, loc, context);
		}
		interp->host_data = debug;
		interp->trace = trace_callback;
		SEE_DEFAULT_CATCH(interp, ctxt);
	}

	/* Ignore trace calls resulting on the same line */
	if (!loc && !debug->last_location.filename)
		return;
	if (debug->last_location.filename && loc &&
	    debug->last_location.filename == loc->filename &&
	    debug->last_location.lineno == loc->lineno)
	    	return;
	if (loc) {
		debug->last_location.filename = loc->filename;
		debug->last_location.lineno = loc->lineno;
	} else
		debug->last_location.filename = NULL;

	/* Invoke the command prompt on breakpoints */
	if (should_break(interp, debug, loc))
		while (!user_command(interp, debug, loc, context))
			{ /* nothing */ }
}

/* Returns true if the current location is matched by the user location */
static int
location_matches(interp, curloc, usrloc)
	struct SEE_interpreter *interp;
	struct SEE_throw_location *curloc, *usrloc;
{
	return usrloc && usrloc->filename && curloc && curloc->filename &&
	       SEE_string_cmp(curloc->filename, usrloc->filename) == 0 &&
	       curloc->lineno == usrloc->lineno;
}

/* Returns true if a breakpoint was hit, and the debugger should intercede */
static int
should_break(interp, debug, loc)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
{
	struct breakpoint *bp, **lbp;

	/* Break if the break_immediately flag is set */
	if (debug->break_immediately) {
		debug->break_immediately = 0;
		return 1;
	}

	/* Scan the breakpoint list for unignoreable bps */
	for (bp = *(lbp = &debug->breakpoints); bp; bp = *(lbp = &bp->next)) 
		if (location_matches(interp, loc, &bp->loc)) {
		    if (bp->ignore_counter)
		    	bp->ignore_counter--;
		    else {
		    	bp->ignore_counter = bp->ignore_reset;
			if (bp->temporary)
			    *lbp = bp->next;
			return 1;
		    }
		}
	return 0;
}

/* Parse "[filename:]lineno" into nloc. Returns true if valid */
static int
location_parse(interp, debug, loc, nloc, argp)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc, *nloc;
	char **argp;
{
	char *p = *argp;
	struct SEE_string *filename = loc ? loc->filename : NULL;
	int lineno = 0, ok = 0;

	if (*p && isdigit(*p) && filename) {
		/* pure line number */
		while (*p && isdigit(*p)) {
			lineno = lineno * 10 + *p - '0';
			p++;
		}
		nloc->filename = filename;
		nloc->lineno = lineno;
		ok = 1;
	} else if (*p && *p != ':') {
		char *start = p;
		while (*p && !isspace(*p) && *p != ':') p++;
		if (*p == ':' && p[1] && isdigit(p[1]))  {
			filename = SEE_string_sprintf(interp, "%.*s", 
				p - start, start);
			p++;
			while (*p && isdigit(*p)) {
				lineno = lineno * 10 + *p - '0';
				p++;
			}
			nloc->filename = filename;
			nloc->lineno = lineno;
			ok = 1;
		} else
			fprintf(stderr, 
				"missing ':<lineno>' after filename\n");
	} else 
	    fprintf(stderr, "expected <filename>:<lineno>\n");
	if (ok) {
		while (*p && isspace(*p)) p++;
		*argp = p;
	}
        return ok;
}

static int
cmd_where(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	SEE_PrintTraceback(interp, stderr);

	fprintf(stderr, " @ ");
	loc_print(stderr, loc);
	fprintf(stderr, "\n");

	return 0;
}

static int
cmd_step(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	debug->break_immediately = 1;
	return 1;
}

static int
cmd_cont(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	return 1;
}

static int
cmd_break(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	struct SEE_throw_location bloc;
	struct breakpoint *bp;

	if (location_parse(interp, debug, loc, &bloc, &arg)) {
		bp = bp_add(interp, debug, &bloc, 0, 0);
		fprintf(stderr, "added breakpoint: ");
		bp_print(interp, bp, stderr);
		fprintf(stderr, "\n");
	}
	return 0;
}

void
debug_add_bp(interp, debug, filename, lineno)
	struct SEE_interpreter *interp;
	struct debug *debug;
        const char *filename;
	int lineno;
{
	struct SEE_throw_location loc;
	struct breakpoint *bp;

	loc.filename = SEE_string_sprintf(interp, "%s", filename);
	loc.lineno = lineno;
	bp = bp_add(interp, debug, &loc, 0, 0);
	fprintf(stderr, "added breakpoint: ");
	bp_print(interp, bp, stderr);
	fprintf(stderr, "\n");
}

/* Show breakpoints in reverse order */
static void
bp_show(interp, bp)
	struct SEE_interpreter *interp;
	struct breakpoint *bp;
{
	if (bp) {
	    bp_show(interp, bp->next);
	    fprintf(stderr, "  ");
	    bp_print(interp, bp, stderr);
	    fprintf(stderr, "\n");
	}
}


static int
cmd_show(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	if (!debug->breakpoints)
		fprintf(stderr, "no breakpoints\n");
	else {
		fprintf(stderr, "current breakpoints:\n");
		bp_show(interp, debug->breakpoints);
	}
	return 0;
}

static int
cmd_delete(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	char *p;
	int id;

	p = arg;
	if (!*p || !isdigit(*p)) {
		fprintf(stderr, "expected number\n");
		return 0;
	}
	id = 0;
	while (*p && isdigit(*p))
		id = 10 *id + (*p++ - '0');
	if (bp_delete(interp, debug, id))
		fprintf(stderr, "breakpoint #%d deleted\n", id);
	else
		fprintf(stderr, "error: unknown breakpoint #%d\n", id);
	return 0;
}

static int
cmd_eval(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	SEE_try_context_t ctxt;
	struct SEE_value res;
	struct SEE_string *str;

	void (*save_trace)(struct SEE_interpreter *, 
		struct SEE_throw_location *, struct SEE_context *);

	if (!*arg) {
		fprintf(stderr, "expected expression text\n");
		return 0;
	}
	str = SEE_string_sprintf(interp, "%s", arg);
	save_trace = interp->trace;
	interp->trace = NULL;
	SEE_TRY(interp, ctxt) {
		SEE_context_eval(context, str, &res);
	}
	interp->trace = save_trace;
	if (SEE_CAUGHT(ctxt)) {
		fprintf(stderr, " exception: ");
		SEE_PrintValue(interp, SEE_CAUGHT(ctxt), stderr);
		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, " = ");
		SEE_PrintValue(interp, &res, stderr);
		fprintf(stderr, "\n");
	}
	return 0;
}


static int
cmd_throw(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	SEE_try_context_t ctxt;
	struct SEE_value res;
	struct SEE_string *str;

	void (*save_trace)(struct SEE_interpreter *, 
		struct SEE_throw_location *, struct SEE_context *);

	if (!*arg) {
		fprintf(stderr, "expected expression text\n");
		return 0;
	}
	str = SEE_string_sprintf(interp, "%s", arg);
	save_trace = interp->trace;
	interp->trace = NULL;
	SEE_TRY(interp, ctxt) {
		SEE_context_eval(context, str, &res);
	}
	interp->trace = save_trace;
	if (SEE_CAUGHT(ctxt)) {
		char *yn;

		fprintf(stderr, "An exception occured while evaluating the expression\n  exception: ");
		SEE_PrintValue(interp, SEE_CAUGHT(ctxt), stderr);
		fprintf(stderr, "\n");
		yn = readline("Throw this exception instead? [n]: ");
		if (yn && (*yn == 'y' || *yn == 'Y'))
			SEE_DEFAULT_CATCH(interp, ctxt);
	} else {
		fprintf(stderr, "throwing ");
		SEE_PrintValue(interp, &res, stderr);
		fprintf(stderr, " ...\n");
		SEE_THROW(interp, &res);
	}
	return 0;
}


static int
cmd_help(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	int i;
	for (i = 0; cmdtab[i].name; i++)
		fprintf(stderr, "   %-20s%s\n",
			cmdtab[i].name, cmdtab[i].doc);
	return 0;
}

static int
cmd_info(interp, debug, loc, context, arg)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
	char *arg;
{
	fprintf(stderr, "context info:\n");
	fprintf(stderr, "  activation = ");
	SEE_PrintObject(interp, context->activation, stderr);
	fprintf(stderr, "\n");
	fprintf(stderr, "  variable = ");
	SEE_PrintObject(interp, context->variable, stderr);
	fprintf(stderr, "\n");
	fprintf(stderr, "  varattr = < %s%s%s%s>\n",
		context->varattr & SEE_ATTR_READONLY ? "readonly " : "",
		context->varattr & SEE_ATTR_DONTENUM ? "dontenum " : "",
		context->varattr & SEE_ATTR_DONTDELETE ? "dontdelete " : "",
		context->varattr & SEE_ATTR_INTERNAL ? "internal " : "");
	fprintf(stderr, "  this = ");
	SEE_PrintObject(interp, context->thisobj, stderr);
	fprintf(stderr, "\n");
	return 0;
}


/* Prompts user for a command, and carry it out. 
 * Returns true if the command is to resume, false if not.
 */
static int
user_command(interp, debug, loc, context)
	struct SEE_interpreter *interp;
	struct debug *debug;
	struct SEE_throw_location *loc;
	struct SEE_context *context;
{
	char *line, *p, *cmd;
	int i, result;

	loc_print(stderr, loc);
	line = readline(" % ");
	if (!line) {
		fprintf(stderr, "end-of-file in debugger; terminating\n");
		exit(1);
	}
	p = line;
	while (*p && isspace(*p)) p++;	/* skip leading whitespace */

	cmd = p;			/* extract initial command word */
	while (*p && isalnum(*p)) p++;
	if (*p) *p++ = '\0';
	while (*p && isspace(*p)) p++;	/* skip following whitespace */

	result = 0;
	if (*cmd) {
	    for (i = 0; cmdtab[i].name; i++)
		if (strcmp(cmdtab[i].name, cmd) == 0) {
		    result = (*cmdtab[i].fn)(interp, debug, loc, context, p);
		    break;
		}
	    if (!cmdtab[i].name)
	    	fprintf(stderr, "unknown command '%s'\n", cmd);
	}
	free(line);
	return result;
}
