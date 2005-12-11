
describe("Exercises function instances.")

function factorial(n) {
	if (n <= 1) return 1;
	return n * factorial(n - 1);
}
test("factorial(5)", 1*2*3*4*5)


function foo() { 
	this.getValue = function () { return this.value; }
	this.setValue = function (value) { this.value = value; }
}

i1 = new foo();
i1.setValue(123);
i2 = new foo();
i2.setValue(456);
i3 = new foo();
foo.prototype.value = 789;
test("i1.getValue()", 123)
test("i2.getValue()", 456)
test("i3.getValue()", 789)

test("foo.prototype.constructor", foo)
test("foo.constructor", foo)

finish()
