# itai-lang spec for version 0.1 (initial version)

## Syntax
Statments end with a semicolon (`;`).<br>
You can give expressions higher precedence with parentheses.

## Literals
**Integers:** `<number>` for example: `14`.<br>
**Booleans:** `true` or `false` (1 and 0 respectively). any number other than 0 is `true`, 0 is `false`.<br>
**Characters:** ASCII characters only, `'<character>'` (a-zA-Z and number characters, 0 to 255).

## Variables
```golang
var <type> <name> = <value>;
```
### Types
**bool** - 1 byte, `true` or `false` (1 or 0 respectively).<br>
**int** - 4 byte integer.<br>
**char** - 1 unsigned byte, can hold all ASCII characters.<br>
**byte** - 1 byte.

## Comments
`//` until end of line.

## Pointers
regular pointers. no pointer arithmetic. no references.

## Loops
`while` only.

## `if`/`else`
`if`, `else`, `else if`.

## Functions
```rust
// name|parameters|return type
fn add(int a, int b) int {
    return a+b;
}
```

## Standard library
`putchar(char c)` from the C standard library.
`getchar() char` from the C standard library.

## Operators in this version
**Math:** `+`, `-`, `*`, `/`.<br>
**Asignment:** `=`.<br>
**Comparison:** `==`, `!=`, `<`, `>`, `<=`, `=>`.

## Keywords in this version
`var`<br>
`while`<br>
`if`<br>
`else`<br>
`true`<br>
`false`<br>
`fn`<br>
`return`<br>
### The types
`int`<br>
`char`<br>
`byte`<br>
`bool`
