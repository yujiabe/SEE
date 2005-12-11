
describe("Exercises every production in the grammar");

/* FutureReservedWord */
test("abstract", "exception");
test("boolean", "exception");
test("byte", "exception");
test("char", "exception");
test("class", "exception");
test("const", "exception");
test("debugger", "exception");
test("enum", "exception");
test("export", "exception");
test("extends", "exception");
test("final", "exception");
test("float", "exception");
test("goto", "exception");
test("implements", "exception");
test("int", "exception");
test("interface", "exception");
test("long", "exception");
test("native", "exception");
test("package", "exception");
test("private", "exception");
test("protected", "exception");
test("short", "exception");
test("static", "exception");
test("super", "exception");
test("synchronized", "exception");
test("throws", "exception");
test("transient", "exception");
test("volatile", "exception");
test("double", "exception");
test("import", "exception");
test("public", "exception");

test("1 + +1", 2);
test("1 - +1", 0);
test("1 + -1", 0);
test("1 + +1", 2);

test("this", this);
test("null", null);
test("undefined", undefined);
test("'\\u0041'", "A");
test('"\\x1b\\x5bm"', "[m");
test('"\\33\\133m"', "[m");
ident = "ident"; test("ident", "ident");
test("[1,2,3].length", 3);
test("({'foo':5}.foo)", 5);
test("((((3))))", 3);

function constr() {
	return constr;
}
constr.prototype = Object.prototype

test("new new new constr()", constr);
test("(1,2,3)", 3);
test("i = 3; i++", 3); test("i", 4);
test("i = 3; ++i", 4); test("i", 4);
test("i = 3; i--", 3); test("i", 2);
test("i = 3; --i", 2); test("i", 2);
test("i = 3; i ++ ++", "exception");
test("i = 3; --i++", "exception");	/* only inc/decrement lvalues */
test("i = 3; ++i--", "exception");	/* only inc/decrement lvalues */

test("!true", false);
test("~0", -1);
test("void 'hi'", undefined);
test("i = 3; delete i", true); test("i", "exception");

test("3 * 6 + 1", 19);
test("1 + 3 * 6", 19);
test("17 % 11 * 5", 30);
test("30 / 3 / 5", 2);
test("30 / 3 * 5", 50);

test("1 - 1 - 1", -1);

test("i=3;j=5; i*=j+=i", 24);

/* instanceof only supports objects */
test("1 instanceof 1", "exception");
test("1 instanceof Number.prototype", "exception");

/* Only function objects should support HasInstance: */
test("new Number(1) instanceof Number.prototype", "exception");	


/* Test the instanceof keyword and the new operator applied to functions. */
function Employee(age, name) {
	this.age = age;
	this.name = name;
}
Employee.prototype = new Object()
Employee.prototype.constructor = Employee
Employee.prototype.toString = function() {
	return "Name: " + this.name + ", age: " + this.age;
}
Employee.prototype.retireable = function() { return this.age > 55; }

function Manager(age, name, group) {
	this.age = age;
	this.name = name;
	this.group = group;
}
Manager.prototype = new Employee();
Manager.prototype.toString = function() {
	return "Name: " + this.name + ", age: " + this.age
	       + ", group: " + this.group;
}

e = new Employee(24, "Tony");
m = new Manager(62, "Paul", "Finance");
test("m.retireable()", true);
test("m instanceof Employee", true);
test("e instanceof Manager", false);

test("{true;}", true);
test(";", undefined);
test("{}", undefined);
test("i=0; do { i++; } while(i<10); i", 10);
test("i=0; while (i<10) { i++; }; i", 10);
test("for (i = 0; i < 10; i++); i", 10);
test("i=0; for (; i < 10; i++); i", 10);
test("i=0; for (; i < 10; ) i++; i", 10);
test("i=0; for (; ;i++) if (i==10) break; i", 10);
test("a=[1,2,3,4]; c=0; for (var v in a) c+=a[v]; c", 10);
test("delete t; t", "exception");
test("{var t;} t", undefined);
test("continue", "exception");
test("return", "exception");
test("break", "exception");
test("x = 0; outer: for (;;) { for (;;) break outer; x++; }; x", 0);
test("x = 0; for (i = 0; i < 3; i++) { continue; x++; } x", 0);
test("x = 0; it:for (i = 0; i < 3; i++) { for (;;) continue it; x++; } x", 0);
test("c = 9; o = { a:'a', b: { c: 'c' }, c:7 }; with (o.b) x = c; x", 'c');
test("x = ''; for (i = 0; i < 8; i++) switch (i) {" +
     "case 0: x+='a'; case 1: x+='b'; break;" +
     "case 2: x+='c'; break; case 3: x+='d'; default: x+='e';" +
     "case 4: x+='f'; break; case 5: x+='g'; case 6: x+='h';}; x",
     "abbcdeffghhef");
test("foo:bar:baz:;", undefined);
test("throw {}", "exception");
test("x=0;try{throw {a:1}} catch(e){x=e.a};x", 1);
test("x=y=0;try{" +
     " try { throw {a:1} } finally {x=2}; " +
     "} catch(e) {y=e.a}; x+y", 3);
test("x=y=0; try{throw {a:2};y=1;} catch(e){x=e.a;y=-7;} finally{y=3}; x+y", 5);


finish()
