
describe("Regression tests from bugzilla.")
compat('ext1')

function foo () { 1; }
test(foo.prototype.constructor, foo)			/* bug 9 */

var lower = "abcdefghij0123xyz\uff5a";
var upper = "ABCDEFGHIJ0123XYZ\uff3a";
test(literal(lower)+".toUpperCase()", upper)
test(literal(upper)+".toLowerCase()", lower)
test("'foo'.lastIndexOf('o', 0)", -1)
test("Function().__proto__", Function.prototype)
test("typeof asjlkhadlsh", "undefined")

function f() { return this; } 
function g() { var h = f; return h(); } 
test("g().toString()", this.toString())			/* bug 32 */

test("(new String()).indexOf()", -1)			/* bug 33 */
test("-\"\\u20001234\\u2001\"", -1234)			/* r960 */
test("/m/i.ignoreCase == true", true)			/* bug 34 */
test("a: { do { break a } while (true) }",  void 0);	/* bug 35 */


finish()
