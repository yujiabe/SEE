#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This is a simple, generic test framework.
 * The main program should provide a void function called test(), 
 * which calls the following macros.
 */

#define TEST_NOT_NULL(p)    _TEST((p) != 0, #p " != NULL")
#define TEST_NULL(p)	    _TEST(!(p), #p " == NULL")
#define TEST_EQ(a,b)	    _TEST((a)==(b), #a " == " #b)
#define TEST(expr)	    _TEST(expr, #expr)
#define TESTX(expr, msg)    _TEST(expr, #expr " (" msg ")")
#define TEST_FALSE(expr)    _TEST(!(expr), "!(" #expr ")")
#define FAIL(msg)	    _TEST(0, msg)
#define PASS(msg)	    _TEST(1, msg)

/* If an entire test should be ignored, call exit(EXIT_IGNORE); */
#define EXIT_IGNORE	    77

int _test_count, _test_failures, _test_verbose;
const char *_test_program;

void test(void);
static void _test(int, const char *, const char *, int);
static const char * _test_basename(const char *);

static const char *
_test_basename(const char *name)
{
    const char *n;
    const char *res = name;

    for (n = name; *n; n++)
	if (*n == '/')
	    res = n + 1;
    return res;
}

static void
_test(int cond, const char *desc, const char *file, int line)
{
	_test_count++;
	if (cond) {
		if (_test_verbose)
		    printf("[32mPASS[m line %3d: %s\n", line, desc);
	} else {
		printf("[41mFAIL[m line %3d: %s\n", line, desc);
		_test_failures++;
	}
}

int
main(int argc, char **argv)
{
	_test_program = _test_basename(argv[0]);
	test();
	if (_test_verbose) 
	    printf("%d of %d passed (%.2f%%)\n",
		_test_count - _test_failures, _test_count,
		100.0 * (_test_count - _test_failures) / (double) _test_count);
	exit(_test_failures ? 1 : 0);
}

#define _TEST(cond, desc) _test((cond), desc, __FILE__, __LINE__)
