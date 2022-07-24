# Itai-lang
A custom programming language based on C, Rust, and Go.

## Current progress
An example of everything the compiler can currently do
```rust
/***
 * itai-lang example
 * 14/07/2022 (dd/mm/yy)
 ***/
var global: i32; // initialized by default to 0
var global2: i32 = 20;

fn answer() -> i32 {
	return 42;
}

fn main() -> i32 {
	var local: i32 = 10;
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

	var return_value: i32 = answer();
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
The compiler currently recieves code as the first argument, transpiles it to C, and prints the resulting C code to stdout.
An example of a typical usage:
```bash
./ilc 'fn main() -> i32 { return 40 + 2; }'
```
The output of the above will be:
```
// Generated code for itai-lang.
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef __int128_t i128;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef __uint128_t u128;
typedef ssize_t isize;
typedef size_t usize;
typedef float f32;
typedef double f64;
typedef const char *str;

i32 main() {
{
return 40 + 2;
}
}

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
