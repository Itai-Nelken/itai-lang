# Itai-lang
A custom programming language based on C, Rust, and Go.

An example of what the compiler can currently compile:
```rust
// A comment
var a = 0;

fn main() {
	var b = 1;
	var c: u32 = 1;
	if -1 {
		1;
	} else if +2 {
		2;
	} else {
		3;
	}

	a = test();
	while a {
		a = a - 1;
	}
}

fn test() -> i32 {
	return 1 + 2 * 4 / 2 + (2 + 3);
}
```

The compiler currently transpiles the code to C that can be compiled with any modern C compiler.

## Full language spec
The full spec for the language is [here](SPEC.md).
It isn't final yet.

