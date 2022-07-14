# Itai-lang
A custom programming language based on C, Rust, and Go.

## Current progress
An example of everything the compiler can currently do
```rust
/***
 * itai-lang example
 * 14/07/2022 (dd/mm/yy)
 ***/
var global; // initialized by default to 0
var global2 = 20;

fn answer() -> i32 {
	return 42;
}

fn main() -> i32 {
	var local = 10;
	if local == 10 { // true
		global = 1;
	} else if local > 20 { // false
		global = -1;
	} else { // false
		global = 0;
	}

	while local > 0 {
		local=local-1;
	}

	{
		// new scope.
		// local variables declared here
		// are only available inside this scope.
		var inner = global2;
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
Only number literals are implemented.
#### variables
Global and local, only 32 bit signed integers.
#### types
Types are parsed in variable and function declaration, but no typechecking is done.
#### scopes
Global, function, and block scopes are supported.
#### functions
Zero arity (zero parameters) functions can be declared and called.
Functions can return values.
#### loops
only `while`.
#### if statements
`if/else-if/else`.

## Compiling
```bash
cd compiler
mkdir build
cd build
cmake ..
make
````
### Running
The compiler currently recieves code as the first argument, and prints the resulting AST to standard out (stdout).
An example of a typical usage:
```bash
./ilc 'fn main() -> i32 { return 40 + 2; }'
```
The output of the above will be:
```
globals:
=======
functions:
=========
main:
name id: 0
return type: {type: TY_I32, type_id: -1}
locals:
body:
ASTBlockNode {type: ND_BLOCK, body: [ASTUnaryNode {type: ND_RETURN, child: ASTBinaryNode {type: ND_ADD, left: ASTLiteralNode {type: ND_NUM, as.int32: 40}, right: ASTLiteralNode {type: ND_NUM, as.int32: 2} } } ] }
```

## Full language spec
The full spec for the language is [here](SPEC.md).
It isn't final yet.

## TODO
- [x] types.
- [ ] typechecking.
- [ ] references.
- [x] if-else-if statements.
- [ ] switch statement.
- [ ] constants (`const name = value;`).
