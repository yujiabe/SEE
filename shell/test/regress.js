
describe("Regression tests from bugzilla.")

/* bug 9 */
function foo () { 1; }
test(foo.prototype.constructor, foo)

var lower = "abcdefghij0123xyz\uff5a";
var upper = "ABCDEFGHIJ0123XYZ\uff3a";
test(literal(lower)+".toUpperCase()", upper)
test(literal(upper)+".toLowerCase()", lower)
test("'foo'.lastIndexOf('o', 0)", -1)
test("Function().__proto__", Function.prototype)
test("typeof asjlkhadlsh", "undefined")

function f() { return this; } 
function g() { var h = f; return h(); } 
test("g().toString()", this.toString())

finish()
