# Itai-lang

A custom programming language aiming at the simplicity of C with the power of C++, Rust and Go.

The compiler currently parses only a small subset of the language and does some basic typechecking on it.\
The code generator currently generates C.

## Usage

```
Usage: ./ilc [options] file
Options:
	--help,        -h    Print this help.
	--dump-ast,    -d    Dump the parsed and validated AST.
	--dump-tokens, -t    Dump the scanned tokens.
```
The compiler currently compiles by default a file called `test.ilc` in the current directory.

## Tests

To make sure no regressions occur, there are tests for every major feature & almost all errors.\
The tests are compiled and their output compared to the expected one by the `tester` program.\
Note that the tests do not cover the whole language yet. Also note that some tests were written
for fixed bugs in older iterations of the compiler, and so will seem weird and unnecessary.

### Running the tests

To run the tests, the `tester` program must first be compiled.\
To do so, execute the following from the root of the repository:
```bash
cd compiler/tester
g++ tester.cpp -Wall -Wextra -Werror -o tester
```
Now the tests can be run by executing the `tester` program in the same folder like this:
```bash
./tester
```

### Notes about the `tester` program

* The `tester` program is currently written in (not very good) C++.\
  The plan is to rewrite it in itai-lang as soon as the compiler is able to
  compile enough of the language for it to be usable.

* The `tester` program expects the compiler executable to be named `ilc` and to be
  present in the `<repo_root>/compiler/build` folder (`<repo_root>` represents the path to the repository).\
  This also means that the `tester` program can only be run from the `compiler`, `compiler/build`, & `compiler/tester` folders.

* The `tester` program runs all files in the current folder with the extension `.ilc`.\
  Each test must have the following as the first line: `/// expect` followed by either `success`, or `error:` followed by the expected error message.

## Full language spec

The full spec for the language is [here](SPEC.md), it isn't final yet.\
A new and improved spec is being written, it can be found [here](new_spec.md).

