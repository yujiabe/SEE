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
    if (v === NO_EXCEPTION) return "NO_EXCEPTION";
    if (v === ANY_EXCEPTION) return "ANY_EXCEPTION";
    try {
	switch (typeof v) {
	case "string":
		return '"' + v.replace(/[\\'"]/g, "\\$&") + '"';
	default:
		return String(v);
	}
    } catch (e) {
	return "<cannot represent " + typeof v + " value as string>";
    }
}

/*
 * instances of ExceptionClass are treated specially by the test() function.
 * They only match values that have been thrown.
 */
function Exception(value) { 
    switch (value) {
    /* For error classes, we match by constructor only */
    case Error:	case EvalError: case RangeError:
    case ReferenceError: case SyntaxError: case TypeError: case URIError:
	return new ExceptionInstance(value);
    /* Everything else we match exactly */
    default:
	return new ExceptionValue(value); 
    }
}

/* Exception base class */
function ExceptionBase() {}
ExceptionBase.prototype = {}

/* Exceptions that match a particular value (usually not an object) */
function ExceptionValue(value) { 
    this.value = value; }
ExceptionValue.prototype = new ExceptionBase();
ExceptionValue.prototype.matches = function(v) { 
    return this.value == v
};
ExceptionValue.prototype.toString = function() { 
    return "throw " + literal(this.value);
};

/* Exceptions that match when they are an instance of a error class */
function ExceptionInstance(base) { 
    this.base = base; }
ExceptionInstance.prototype = new ExceptionBase();
ExceptionInstance.prototype.toString = function() { 
    return "throw " + this.base.prototype.name + "(...)";
};
ExceptionInstance.prototype.matches = function(v) { 
    /* Strict ECMA forbids Error.[[HasInstance]] so we have to hack */
    return (typeof v == "object") && v && v.constructor == this.base;
};

var NO_EXCEPTION = {}
var ANY_EXCEPTION = {}


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

	var result, msg, ok, result_str;

	try {
		result = eval(expr);
		if (expected instanceof ExceptionBase)
		    ok = false;
		else
		    ok = (result === expected || expected === NO_EXCEPTION);
		result_str = literal(result);
	} catch (e) {
		if (expected === ANY_EXCEPTION)
		    ok = true;
		else if (expected instanceof ExceptionBase)
		    ok = expected.matches(e);
		else
		    ok = false;
		result_str = "throw " + literal(e);
	}

	msg = expr + ' = ' + result_str;
	if (ok) {
		pass(msg);
	} else {
		fail(msg, "expected " + literal(expected));
	}
}

function finish() {

	print();
	print((total - failures) + " out of " + total + " sub-tests passed.");

	/* Throw an error on failure */
	if (failures > 0)
		throw new Error("tests failure");
}

