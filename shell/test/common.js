/*
 * Common test code
 * $Id: grammar.js 561 2003-12-04 10:39:04Z d $
 */

/*
 * Tests scripts should first call describe(), then test() for each test, 
 * and then call finish() at the end.
 */

var failures = 0;
var total = 0;

function describe(desc) {
	print()
	print("===============================")
	print("Test: " + desc)
	print()
}

/* Literalise a string for printing purposes */
function literal(v) {
	switch (typeof v) {
	case "string":
		return '"' + v.replace(/[\\'"]/, "\\$1") + '"';
	default:
		return String(v);
	}
}

function pass(msg) {
	print(msg + " - [32mPASS[m");
	total++;
}

function fail(msg, extra) {
	var s = msg + " - [31;7mFAIL[m";
	if (extra) s += "\n\t\t(" + extra + ")";
	print(s);
	failures++;
	total++;
}

/* Run a test, and check that the result is that expected */
function test(expr, expected) {

	var result, msg, ok, result_str, expected_str;

	try {
		result = eval(expr);
		ok = (result === expected || expected == "no exception");
	} catch (e) {
		ok = (expected == "exception");
		result = e.name;
		// result = String(e);
	}

	try { result_str = literal(result); }
	catch (e) { result_str = "<cannot represent as string>"; }


	msg = expr + ' = ' + result_str;
	if (ok) {
		pass(msg);
	} else {
		try { expected_str = literal(expected); }
		catch (e) { expected_str = "<cannot represent as string>"; }
		fail(msg, "expected " + expected_str);
	}
}

function finish() {

	print();
	print((total - failures) + " out of " + total + " sub-tests passed.");

	/* Throw an error on failure */
	if (failures > 0)
		throw new Error("tests failure");
}

