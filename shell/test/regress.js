
describe("Regression tests from bugzilla.")

function foo () { 1; }
test(foo.prototype.constructor, foo)			/* bug 9 */

var lower = "abcdefghij0123xyz\uff5a";
var upper = "ABCDEFGHIJ0123XYZ\uff3a";
test(literal(lower)+".toUpperCase()", upper)		/* bug 4 */
test(literal(upper)+".toLowerCase()", lower)
test("'foo'.lastIndexOf('o', 0)", -1)			/* bug 24 */
test("typeof asjlkhadlsh", "undefined")

compat('js12')
test("Function().__proto__", Function.prototype)
compat('')

function f() { return this; } 
function g() { var h = f; return h(); } 
test("g()", this)					/* bug 32 */

test("(new String()).indexOf()", -1)			/* bug 33 */
test("-\"\\u20001234\\u2001\"", -1234)			/* r960 */
test("/m/i.ignoreCase == true", true)			/* bug 34 */
test("a: { do { break a } while (true) }",  void 0);	/* bug 35 */
test("/foo/.test('foo')", true);			/* bug 40 */
test("/foo/.test('bar')", false);			/* bug 40 */
test("'foo'.concat('bar')", "foobar");			/* bug 42 */
test("('x'+'y').indexOf('longer')", -1);		/* bug 46 */

/* bug 36 */
function h(x,x) { return arguments[0]+","+arguments[1]; }
test("h(1,2)", "1,2")
function h1(x) { x = 1;                      arguments[0] = 2; return x; }
function h2(x) { x = 1; delete arguments[0]; arguments[0] = 2; return x; }
test("h1()", 1);
test("h1(0)", 2);
test("h2()", 1);
test("h2(0)", 1);

test("var a = 10; a -= NaN; isNaN(a)", true);		/* bug 61 */
test("var b = new Array(1,2); delete b[1];0", 0);	/* bug 62 */

finish()
