/* Copyright David Leonard, 2003. */
/* $Id$ */

/*
 * Generate Unicode tables.
 * This is a HORRIBLE program that does two things: First, it computes the
 * best compression result for a depth=2 splay tree for various
 * properties of the Unicode data tables. Its second function is to
 * spew out the C code that will implement those tables. Do not attempt
 * to understand this.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>

#define _UNICODE_MAX		0x110000

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *f;
	char *s, *start;
	size_t len;
	unsigned int codepoint;
	unsigned char *bit;
	char class[2];
	int i, group, used;
	int bitcount;
	int groupinc, groupstart, ngroups;
	char *usedtab, *ut, *name;
	char *datapath;

	if (argc < 4) {
		fprintf(stderr, "usage: %s path [-|bits name] class ...\n",
			argv[0]);
		exit(1);
	}

	datapath = argv[1];
	argv++; argc--;

	if (argv[1][0] == '-' && argv[1][1] == '\0')
		group = 0;
	else {
		group = atoi(argv[1]);
		name = argv[2];
		argv++; argc--;
	}

	/* First load the file */
	f = fopen(datapath, "r");
	if (!f) err(1, "%s", datapath);

	/* allocate space for the bit array */
	bit = (unsigned char *)malloc(_UNICODE_MAX >> 3);
	if (!bit) errx(1, "malloc");

	bitcount = 0;
	memset(bit, 0, _UNICODE_MAX >> 3);

	while ((start = fgetln(f, &len))) {
		s = start;
		if (s[len-1] == '\n')
			len--;

		codepoint = 0;
		for (; *s != ';'; s++) {
		    unsigned int d = (*s >= 'A') ? *s - 'A' + 10 : *s - '0';
		    if (d < 0 || d > 15)
			errx(1, "bad codepoint char %c", *s);
		    codepoint = (codepoint << 4) | d;
		}
		s++;
		if (codepoint >= _UNICODE_MAX)
			errx(1, "codepoint too big 0x%x", codepoint);

		/* Skip name field */
		for (; *s != ';'; s++);
		s++;

		class[0] = *s++;
		class[1] = *s++;

		/* printf("%x = %c%c\n", codepoint, class[0], class[1]); */

		/* scan through argv[] to see if we match */
		if (argv[2][0] == '$' && (codepoint == '$' || codepoint == '_'))
			; /* keep */
		else {
			for (i = 2; i < argc; i++)
			    if (argv[i][0] == class[0] &&
				argv[i][1] == class[1])
				    break;
			if (i == argc)
				continue;
		}

		bit[codepoint >> 3] = 1 << (codepoint & 7);
		bitcount++;
	}
	if (ferror(f)) err(1, "%s", datapath);
	fclose(f);

	fprintf(stderr, "Found %d codepoints\n", bitcount);

	if (group == 0) {
		int best = -1;
		int bestgroup = -1;
		for (group = 3; group < 21; group++) {
			int indexsz, tabsz, total;
			groupinc = 1 << group;

			used = 0;
			for (groupstart = 0; 
				groupstart < _UNICODE_MAX; 
				groupstart += groupinc)
			    for (i = groupstart >> 3; 
				 i < (groupstart + groupinc) >> 3
				 && i < (_UNICODE_MAX >> 3);
				 i++)
				if (bit[i]) {
				    used++;
				    break;
				}

			/* Size of the master lookup table */
			ngroups = groupstart / groupinc;
			indexsz = ngroups * sizeof (void *);
			tabsz = (sizeof (char) * (groupinc >> 3)) * used;
			total = indexsz + tabsz;

			printf("%d -> %d/%d groups used (%d+%d = %d bytes)\n", 
			    group, used, ngroups,
			    indexsz, tabsz, total);
			if (best == -1 || total < best) {
				best = total;
				bestgroup = group;
			}
		}
		printf("-- best group is %d (%d bytes)\n", bestgroup, best);
		exit(0);
	}

	printf("\n\n/* Unicode tables for %d-bit groups:", group);
	for (i = 2; i < argc; i++)
		printf(" %s", argv[i]);
	printf(" */\n/* %d codepoints in this table */\n", bitcount);

	groupinc = 1 << group;

	for (groupstart = 0; 
		groupstart < _UNICODE_MAX; 
		groupstart += groupinc)
	    ;
	ngroups = groupstart / groupinc;
	usedtab = (unsigned char *)malloc(ngroups);
	memset(usedtab, 0, ngroups);

	for (groupstart = 0; 
		groupstart < _UNICODE_MAX; 
		groupstart += groupinc)
	    for (i = groupstart >> 3; 
		 i < (groupstart + groupinc) >> 3
		 && i < (_UNICODE_MAX >> 3);
		 i++)
		if (bit[i]) {
		    usedtab[groupstart / groupinc] = 1;
		    break;
		}

	for (groupstart = 0; 
		groupstart < _UNICODE_MAX; 
		groupstart += groupinc)
	{
	    int gn = groupstart / groupinc;
	    if (!usedtab[gn])
		continue;
	    printf("static unsigned char %s_%d[] = {", name, gn);
	    for (i = groupstart >> 3; 
		 i < (groupstart + groupinc) >> 3
		 && i < (_UNICODE_MAX >> 3);
		 i++)
	    {
		if (i != groupstart >> 3) putchar(',');
		if ((i & 0x0f) == 0) printf("\n\t");
		/* printf("0x%02x", bit[i]); */
		printf("%u", bit[i]);
	    }
	    printf(" };\n");
	}

	printf("unsigned char *SEE_unicode_%s[] = {", name);
	for (groupstart = 0; 
		groupstart < _UNICODE_MAX; 
		groupstart += groupinc)
	{
	    int gn = groupstart / groupinc;
	    if (groupstart != 0) putchar(',');
	    if ((gn & 0xf) == 0) printf("\n\t");
	    if (usedtab[gn])
		printf("%s_%d", name, gn);
	    else
		printf("0");
	}
	printf(" };\n\n");
	exit(0);
}
