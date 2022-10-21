# Itai-lang

A custom programming language based on C, Rust, and Go.

The compiler currently parses only a small subset of the language and does some basic typechecking on it.\
There is no code generator yet, but the parsed and typechecked AST can be dumped to `stdout` with the `-d` flag.

## Usage

```
Usage: ./ilc [options] file
Options:
	--help,     -h    Print this help.
	--dump-ast, -d    Dump the parsed AST.
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

