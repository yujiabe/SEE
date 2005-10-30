/* (c) David Leonard, 2003 */
/* $Id$ */

/*
 * Experimental code to generate compressed unicode case translation tables
 *
 * Case conversion is used in ECMAScript in these places
 *	String.prototype.toUpperCase()
 *	String.prototype.toLocaleUpperCase()
 *	String.prototype.toLowerCase()
 *	String.prototype.toLocaleLowerCase()
 *	Regex internal Canonicalize() enabled by case-insensitive matching
 *
 * In all places, case conversion happens character-by-character.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#define MAX		(0x10ffff + 1)
#define MAXTRANS	32

#define FLAG_FINAL_SIGMA	0x0001
#define FLAG_AFTER_SOFT_DOTTED	0x0002
#define FLAG_MORE_ABOVE		0x0004
#define FLAG_BEFORE_DOT		0x0008
#define FLAG_AFTER_I		0x0010

static void printtrans(int *);
unsigned char transid(int, int *);

/*
 * A translation is a tuple of three targets and two flags, written 
 * <u,l,t,mask,match>
 * Each of u,l,f is a sequence of signed unicode offsets. The flags
 * are constructed such that mask is the union of flags we are interested
 * in, and match is the union of flags that should be set.
 */

unsigned char *transmap[MAX];
int *transp[256];
int ntrans;

static struct {
	const char *name;
	int flag;
} flagname[] = {
	{ "Final_Sigma",		FLAG_FINAL_SIGMA },
	{ "After_Soft_Dotted",		FLAG_AFTER_SOFT_DOTTED },
	{ "More_Above",			FLAG_MORE_ABOVE },
	{ "Before_Dot",			FLAG_BEFORE_DOT },
	{ "After_I",			FLAG_AFTER_I },
	{ NULL, 0 }
};

static int
flagval(s)
	char *s;
{
	int i, len;

	for (i = 0; flagname[i].name; i++) {
		len = strlen(flagname[i].name);
		if (memcmp(s, flagname[i].name, len) == 0)
			return flagname[i].flag;
	}
	errx(1, "bad flag");
}

static void
printtrans(trans)
	int *trans;
{
	int i, *t, j;
	printf("<");
	for (t = trans, i = 0; i < 3; i++) {
		if (i) printf(",");
		j = *t++;
		while (j--) {
			printf(" %d", *t);
			t++;
		}
	}
	printf(" >");
}

unsigned char
transid(len, trans)
	int len, *trans;
{
	unsigned int i;
	int *t;

	/* printf("Adding "); printtrans(trans); printf("\n"); */

	for (i = 0; i < ntrans; i++)
	    if (memcmp(trans, transp[i], len * sizeof (int)) == 0)
		return i;
	if (ntrans == 256)
		errx(1, "too many different case translations");
	t = (int *)malloc(len * sizeof (int));
	memcpy(t, trans, len * sizeof (int));
	transp[ntrans] = t;
	return ntrans++;
}

void
skipspace(s, len)
	char **s;
	int *len;
{
	while (*len && (**s == ' ' || **s == '\t'))
		--*len, ++*s;
}

unsigned int
hex(s, len, optional)
	char **s;
	int *len;
	int optional;
{
	int digits = 0, digitval;
	unsigned int val = 0;

	skipspace(s, len);
	while (*len) {
		if (**s >= '0' && **s <= '9')
			digitval = **s - '0';
		else if (**s >= 'A' && **s <= 'F')
			digitval = **s - 'A' + 10;
		else if (**s >= 'a' && **s <= 'f')
			digitval = **s - 'A' + 10;
		else
			break;
		val = 16 * val + digitval;
		digits++;
		--*len;
		++*s;
	}
	if (!digits && !optional)
		errx(1, "bad hex value");
	return val;
}

int
expect(s, len, ch)
	char **s;
	int *len;
	char ch;
{
	skipspace(s, len);
	if (!*len)
		errx(1, "expected '%c' at end of line", ch);
	if (**s != ch) 
		errx(1, "expected '%c' at '%c'", ch, **s);
	--*len;
	++*s;
}

int
main()
{
	FILE *f;
	int len, i;
	char *s, *d, *e;
	int codepoint, c, transpoint[3];
	static int zero[] = { 1, 0, 1, 0, 1, 0 };
	unsigned int transbuf[64], translen, id;

	ntrans = 0;
	(void)transid(6, zero);	/* Add zero first */


goto zz;

	/* Load the normal unicode case translations */
	if ((f = fopen("UnicodeData.txt", "r")) == NULL)
		errx(1, "UnicodeData.txt");
	while ((s = fgetln(f, &len)) != NULL) {

/* 0    1                    2  3 4 5 6 7 8 9 0 1 2    3 4 */
/* 0061;LATIN SMALL LETTER A;Ll;0;L; ; ; ; ;N; ; ;0041; ;0041 */

		/* Decode the leading hex number */
		codepoint = hex(&s, &len, 0);

		/* Skip fields 1 thru 11 */
		for (i = 1; i < 12; i++) {
			expect(&s, &len, ';');
			while (len && *s != ';') len--, s++;
		}

		/* Decode the upper, title, lower code points */
		for (i = 12; i < 15; i++) {
			expect(&s, &len, ';');
			c = hex(&s, &len, 1);
			transpoint[i-12] = c == 0 ? 0 : c - codepoint;
		}

		transbuf[0] = 1;
		transbuf[1] = transpoint[1];	/* lower */
		transbuf[2] = 1;
		transbuf[3] = transpoint[2];	/* title */
		transbuf[4] = 1;
		transbuf[5] = transpoint[0];	/* upper */
		translen = 6;
		id = transid(translen, transbuf);
		printf("%x: %d\n", codepoint, id);
	}
	fclose(f);

zz:	
	if ((f = fopen("SpecialCasing.txt", "r")) == NULL)
		err(1, "SpecialCasing.txt");
	while ((s = fgetln(f, &len)) != NULL) {
		unsigned int *l, *ll;

		/* Skip blanks and comment lines */
		while (len && (*s == ' ' || *s == '\t')) len--, s++;
		if (*s == '#') len = 0;
		if (!len || *s == '\n' || *s == '\r') continue;

		/* Decode the codepoint ID */
		codepoint = hex(&s, &len, 0);

		l = &transbuf[0];
		for (i = 0; i < 3; i++) {
			expect(&s, &len, ';');
			skipspace(&s, &len);
			*(ll = l++) = 0;
			while (len && *s != ';' && *s != '\n' && *s != '\r') {
			    c = hex(&s, &len, 1);
			    *l++ = c - codepoint;
			    ++*ll;
			    skipspace(&s, &len);
			}
		}
		translen = l - &transbuf[0];
		id = transid(translen, transbuf);
		printf("%x: %d\n", codepoint, id);
	}
	fclose(f);
}
