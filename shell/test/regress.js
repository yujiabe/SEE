
describe("Regression tests from bugzilla.")

/* bug 9 */
function foo () { 1; }
test(foo.prototype.constructor, foo)

finish()
