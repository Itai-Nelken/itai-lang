# Itai-lang

A custom programming language aiming at the simplicity of C with the power of C++, Rust and Go.

The compiler currently parses only a small subset of the language and does some basic typechecking on it.\
The code generator currently generates C.

## Usage

```
Usage: ./ilc [options] file
Options:
	--help,        -h    Print this help.
	--dump-ast,    -d    Dump the parsed AST.
	--dump-tokens, -t    Dump the scanned tokens.
```
The compiler currently compiles by default a file called `test.ilc` in the current directory.

## Ideas being worked on

There are a few programs in the `compiler` folder that are ideas being tested. They might be removed in the future.\
Currently these programs are:
* The `tester`: An experimental compiler-output based testing framework.
* The `ir`: An experiment with a bytecode IR that might be used in the compiler.


## Full language spec

The full spec for the language is [here](SPEC.md), it isn't final yet.\
A new and improved spec is being written, it can be found [here](new_spec.md).

