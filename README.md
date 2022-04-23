# Itai-lang
A custom programming language based on C, Rust, and Go.

## Current progress
An example of everything the compiler can currently do
```rust
/***
 * itai-lang example
 * 22/04/2022 (dd/mm/yy)
 ***/
var global; // initialized by default to 0
var global2 = 20;

fn answer() {
	return 42;
}

fn main() {
	var local = 0;
	for var i = 0; i < 10; i=i+1 {
		local=local+1;
	}
	print local; // 10
	
	if local == 10 { // true
		global = 1;
	} else { // false
		global = 0;
	}

	while local > 0 {
		local=local-1;
	}
	print local; // 0

	{
		// new scope.
		// local variables declared here
		// are only available inside this scope.
		var inner = global2;
		print inner; // 20
	}

	var return_value = answer();
	return return_value;

}
```
### Implemented features (so far)
#### Comments
Single and multi-line. comments are ignored by the scanner (lexer).
#### Operators:
**infix:** `+ - * / %  == != > >= < <= | ^ &  << >>`
**prefix:** `- +`
#### Literals:
Only number literals are supported.
#### variables
Global and local, no types, only 32bit signed integers.
#### scopes
Global, function, and block scopes are supported.
#### functions
Zero arity (zero parameters) functions can be declared and called.
Functions can return a number.
#### loops
There are 2 loops: `for` and `while`.
#### if statements
Only if and if-else statements are supprted. if-else if isn't supported.

## Compiling
```bash
cd compiler
mkdir build
cd build
cmake ..
make
````
### Running
The compiler currently recieves code as the first argument, and prints the resulting assembly to standard out (stdout).
An example of a typical usage:
```bash
./ilc 'fn main() { print 40 + 2; }' > print42.s
gcc print42.s -o print42
./print42
````
The output of the above will be `42`.

## Full language spec
The full spec for the language is [here](SPEC.md).
It isn't final yet.

## TODO
- [ ] types.
- [ ] references.
- [ ] if-else if statements.
- [ ] switch statement.
- [ ] constants (`const name = value;`).
