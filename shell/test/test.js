

function factorial(x) {
	if (x <= 1) return x
	return x * factorial(x-1) }

write(factorial(10) + 
"\n")

write( String(factorial) + "\n");
