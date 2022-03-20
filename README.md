# Itai-lang

### Example

```rust
import "io";
import "arrays";
import "os";
import "strings";
import "types";

module Stack {
using arrays::{make, grow};

export struct Stack<T> {
    data: T[];
    sp: i32;
    sp: usize;
}

const INITIAL_SIZE = 16;
export fn [static Stack]new<T>(initial_size: usize) -> Stack<T> {
    if initial_size == 0 {
        initial_size = INITIAL_SIZE;
    }
    return Stack<T>{data: make<T>(initial_size), sp: 0, size: initial_size};
}

fn [this &const Stack]isFull() -> bool {
    return this.sp == this.size-1 ? true : false;
}

export fn [this &const Stack]isEmpty() -> bool {
    return this.sp == 0 ? true : false;
}

fn [this &Stack]grow() {
    this.size *= 2;
    ::grow(&this.data, this.size);
}

export fn [this &Stack]push(data: T) {
    if this.isFull() {
        this.grow();
    }
    this.data[this.sp] = data;
    this.sp++;
}

export fn [this &Stack]pop() -> T {
    if this.isEmpty() {
        io::printerrln("ERROR: stack is empty!");
    }
    this.sp--;
    return this.data[this.sp];
}

export fn [this &const Stack]peek() -> T {
    return this.data[this.sp-1];
}
} // module Stack

fn main() {
    if arrays::len(os::args) < 2 {
        io::printerrfln("USAGE: %s [numbers]", os::args[0]);
        return 1;
    }
    var args = os::args[1:];
    var len = arrays::len(args);
    var stack = Stack::new<i32>(16);
    for var i = 0; i < len; i++ {
        if types::isdigit(args[i]) {
            stack.push(strings::atoi(args[i]));
        } else {
           io::printerrfln("WARNING: %s is not a number!", args[i]);
        }
    }

    while !stack.isEmpty() {
        io::printfln("%d", stack.pop());
    }
}
```

## Comments

Comments start with `//` until end of line or between `/*` and `*/`.

```c
// single line comment

/*
multiline
comment.
*/
```

### Preprocessor and compiler directives

The Preprocessor and Compiler can be passed certain options using directives:

```rust
#[directive(value)]
// examples
#[wuninitialized(true)] // turn on warnings for uninitialized variables
```

## Types

### Primitives

**NOTE:** `str` isn't a primitive, but it's an alias to one.

| Type | Equivalent in C |
| --- | --- |
| `i8` | `int8_t` (`char`) |
| `i16` | `int16_t` |
| `i32` | `int32_t` (`int`) |
| `i64` | `int64_t` (`long`) |
| `i128` | `__int128_t` |
| `u8` | `uint8_t` (`unsigned char`) |
| `u16` | `uint16_t` |
| `u32` | `uint32_t` (`unsigned`) |
| `u64` | `uint64_t` (`unsigned long`) |
| `u128` | `__uint128_t` |
| `f32` | `float` |
| `f64` | `double` |
| `isize` | `ssize_t` |
| `usize` | `size_t` |
| `char` | `char` |
| `str` (pointer to string literal) | `char*` or `char*` |

### Advanced (from the standard library)

Those aren't primitive types, but they are pretty basic in modern programming languages.

They are automatically imported and put in the global scope.

| Type | Description |
| --- | --- |
| `String` | Mutable string with advanced features (using bound functions) |
| `Vector<T>` | A vector for any type (using generics). has advanced features (using bound functions) |

### `null`

`null` isn't really a type. it is a placeholder for the empty state of each type.

for example: for integers, `null` is `0`, and for strings it's `""` (empty string).

for references, it's an invalid address (0x0) and dereferencing a null reference is a runtime error and where possible, a compile time error.

The table bellow shows what `null` is for every built-in type:

| **type** | **value of `null`** |
| --- | --- |
| all integers (signed and unsigned) | 0   |
| all floats | 0.0 |
| `isize`, `usize` | 0   |
| `char` | nul character (ascii character 0) |
| `str`, `String` | empty string |
| function types | `panic()` built in function (with appropriate error message) |
| All references | 0x0 (an invalid address) |

### Function types

Function types are the type of function literals. they can also be used to store pointers/references to functions.

```rust
fn(<types>) -> T
```

The `<types>` is the argument types, and it can be empty.

`T` is the return type. it also can be multiple types in parentheses or none (if none, than the `->` isn't needed).

### Example

```rust
fn nullfn() { io::println("nullfn()"); }
var func: fn() = null;
var func2 = nullfn;
func = nullfn;
// all calls bellow call nullfn()
nullfn();
func();
func2();
```

The following function type is used when referencing a function type in this document:

```rust
fn(...) -> T
```

### Reference and Array types

Each type has reference and array variants of itself.

In the table bellow, one type from each group is shown.

| Type | Reference variant | Array variant |
| --- | --- | --- |
| `u8` | `&u8`, `&const u8` | `u8[]` |
| `Vector<T>` | `&Vector<T>`, `&const Vector<T>` | `Vector<T>[]` |
| `fn(...)T` | `&fn(...) -> T`, `&const fn(...) -> T` | `fn(...)T[]` |

The array variant can be mixed with the reference ones, for example:

```rust
&u8[] // reference to an array of type u8 and unknown size
&const u8[4] // read-only reference to an array of type u8, size 4
```

### Defining custom types

Custom types are defined using the `type` keyword:

```go
type Number = i32;
var num: Number = 12;
Number typeof num; // true
Number typeof 12; // false
```

### Type casting

Type casting is done by using the type as a function:

```go
type(value) // return value as 'type'.
// === examples ===
var n = i32(12);
var f = f32(0.5);
var d = f64(f);
```

## Literals

### Numbers

For decimal numbers, simply the number.

The character `_` may be used to make the numbers more readable for humans (it's ignored by the compiler).

Hexadecimal numbers have to be prefixed with `0x` , binary numbers with `0b`, and octal numbers with `0o` (0 followed by lowercase 'o').

```rust
// numbers
42 // decimal
0x64 // hexadecimal
0b1010 // binary
0o666 // octal

12.5
1_000
0b1001_0000_1000
0xab_6d_4a
```

### Characters and Strings

Characters are put in between `'` (single quotes) and strings in between `"` (double quotes).<br>

Multiline strings can be put in between `"""` (3 double quotes).

```rust
// character
'c'
// string
"string"
// multiline string
"multiline
string"
```

### Arrays

Array elements are put as a comma separated list inside `[]` (brackets).

```rust
[1, 2, 3, 4, 42]
['a', 'b', 'c']
["s1", "s2", "s3"]
// nested arrays
[1, [2, 3, [4, 5, 6], 7], 8]
```

### Structs

Struct literals are simply the name of the struct followed by a list of the field names, a colon, and the value in between braces.

```rust
struct Foo {
    i32 a;
    i32 b;
}

var instance = Foo{a: 20, b: 2};
```

### Functions

Function literals are simply `fn` followed by the argument list inside parentheses, `->` and the return types (inside parentheses if necessary).

```go
var add = fn(a: i32, b: i32) -> i32 { return a + b; };
add(1, 2); // returns 3
var split = fn(s: String) -> (String, String) {
    var len = s.length()/2;
    return s.substring(0, len), s.substring(len, s.length());
}
split("abcd"); // "ab", "cd"
```

## Operators

### Math

| Operator | Description | Type |
| --- | --- | --- |
| `+` | addition | infix, prestfix |
| `-` | subtraction | infix, prestfix |
| `*` | multiplication | infix |
| `/` | division | infix |
| `%` | modulus (remainder) | infix |

### Bitwise

| Operator | Description | Type |
| ---  | --- | --- |
| `&`  | AND | infix |
| `|`  | OR  |
| `^`  | XOR | infix |
| `~`  | Binary One's Complement | prefix |
| `<<` | left shift | infix |
| `>>` | right shift | infix |

### Assignment

| Operator | Description | Type |
| --- | --- | --- |
| `=` | assignment | infix |
| `+=` | addition+assignment | infix |
| `-=` | subtraction+assignment | infix |
| `*=` | multiplication+assignment | infix |
| `/=` | division+assignment | infix |
| `%=` | modulus+assignment | infix |
| `&=` | AND+assignment | infix |
| `   | =`  | OR+assignment |
| `^=` | XOR+assignment | infix |
| `<<=` | left shift+assignment | infix |
| `>>=` | right shift+assignment | infix |

### Comparison

| Operator | Description | Type |
| --- | --- | --- |
| `==` | equal | infix |
| `!=` | not equal | infix |
| `>` | larger than... | infix |
| `<` | smaller than... | infix |
| `>=` | larger or equal to... | infix |
| `<=` | smaller or equal to... | infix |

### Other

| Operator | Description |     |
| --- | --- | --- |
| `sizeof` | returns the size of an object | prefix |
| `typeof` | returns the type of an object/compares an object with a type | prefix, infix |
| `?:` | conditional expression | N/A |
| `++` | increment | postfix |
| `--` | decrement | postfix |
| `&`, `&const` | reference and constant reference | prefix |
| `*` | dereference | prefix |
| `[]` | subscript (for arrays) | postfix |
| `::` | Scope resolution | prefix, infix |

## Constructs

### if/else

```go
var a = 42;
if a == 42 {
    io::println("The answer");
} else if a == 24 {
    io::println("Almost!");
} else {
    io::println("Wrong!");
}
```

### Switch

```go
var a = 42;
switch a {
    // catch everything between 0 and 23 (including 0 and 23)
    0 ... 23 => {
        io::println("Close to almost!");
    }
    24 => {
        io::println("Almost!");
    }
    42 => {
        io::println("The answer");
    }
    // will catch anything that isn't handled
    _ => {
        io::println("Wrong!");
    }    
}
```

A switch can be exited with the `break` keyword. it is also possible to add labels to cases and `break` to them:

```go
var a = 42;
switch a {
    42 : answer => {
        io::println("The answer!");
    }
    _ => {
       io::println("Wrong!\nGuess you deserve the prize after all...");
        break answer;
    }
}
```

The braces aren't needed for single expressions:

```go
var a = 42;
switch a {
    42 => io::println("The answer!");
    _ => io::println("Wrong answer!");
}
```

Cases can fallthrough using the `fallthrough` keyword:

```go
var a = 24;
switch a {
    24 => fallthrough;
    42 => io::println("The answer (or not)");
}
```

### Loops

There are 2 types of loops: `while` and `for`.

```rust
var i = 0;
while i < 10 {
    i++;
    io::printfln("%d", i);
}
```

```go
for(var i = 0; i < 10; i++) {
    io::printfln("%d", i);
}
```

## Variables

Variables are declared using the `var` keyword followed by the name of the variable. a value can also be assigned by adding `=` followed by an expression that returns a value. if that is done, the compiler infers the type automatically, else you'll have to provided by adding a `:` followed by the type before the `=` (if used).<br>

Constants are the same but you have to assign a value to them at declaration (and you cannot reassign a value to them).

```go
// === Variables ===
var name = value; // the type of 'value' has to be possible to infer as there is no cast or explicit type.
var n1, n2 = v1, v2; // n1 == v1, n2 == v2
// explicit typing
var name: type = value;

// === Constants ===
const name = value;
const n1, n2 = v1, v2; // n1 == v1, n2 == v2
const name: type = value;
```

## Functions

Functions are declared with the `fn` keyword followed by a pair of opening and closing parentheses (`()`) containing the paraneters.`->` followed by the return type(s) can be added if the function returns something.<br>

A function is called by its name followed by opening and closing parentheses (`()`) containing the arguments if applicable.

```rust
// declaring
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// calling
add(40, 2); // 42
```

A function can return more than one value, this is done by putting the return types in a comma separated list inside parentheses after the `->`:

```rust
fn divide(a: i32, b: i32) -> (i32, i32) {
    var remainder = a % b;
    return a / b, remainder;
}

var result, remainder = divide(10, 2); // 5, 0
```

### Variable arguments

Variable argument functions (variadic functions) are declared by adding a last parameter of type `...`. Note that at least one parameter is required before the `...`.

The following api is available to access the arguments:

- `va_start(ap, lastarg)`
  
- `va_end(ap)`
  
- `va_arg<T>() -> T`
  
- `va_copy(from, to)`
  

The above functions are internal and not from the standard library.

**example:**

```rust
fn printf(format: str, ap: ...) {
    var out: String;
    va_start(ap, format);
    defer va_end(ap);
    for(var i = 0; format[i] != '\0'; i++) {
        if format[i] == '%' {
            i++;
            switch format[i] {
                'i' => fallthrough;
                'd' => out.append_char(va_arg<i32>(ap));
                'c' => out.append_char(va_arg<char>(ap));
                '%' => out.append_char('%');
                _ => out.append(format[i-1 : i]);
            }
        } else {
            out.append(format[i : i+1]);
            i++;
        }
    }

    io::print(out.as_str());
}
```

## Arrays

```go
var array = [1, 2, 3, 4];
i32[4] typeof array; // true
var a: i32[2];
a[0] = 1;
a[1] = 2;

// 2d arrays
var two_d = [
    [1, 2, 3, 4],
    [5, 6, 7, 8]
];
i32[2][4] typeof two_d; // true
var b: i32[2][2];
b[0] = [1, 2];
b[1] = [3, 4];

// accessing
array[0]; // 1
a[1]; // 2
two_d[0][1]; // 6
b[1][0]; // 3
```

## References/Pointers

References also known as pointers allow you to "point" to a variable instead of copying it.

A reference of a variable can be taken by adding `&` before it, and the value of it can be accessed using `*`:

```go
// a number (i32)
var number = 42;

// take a reference of the variable 'number',
// and store it in the variable 'ref'
var ref = &number;
// access the address of 'number'
ref;
// access the value stored in 'number'
*ref; // 42
// change 'number'
*ref = 12;
// now number is 12
number; // 12

// read-only reference
var ro_ref = &const number;
*ro_ref; // 42
// *ro_ref = 12 is a compile-time error
```

Read only or constant references are references that can only be used to read a value, not change it.

A constant reference can be taken by adding `&const` before the variable, and it can be accessed the same way as a regular reference:

```go
// a number (i32)
var number = 42;

// take a constant reference to the variable 'number'
// and store it in the variable 'ref'
var ref = &const number;
// access the value stored in 'number'
*ref; // 42

// *ref = 12; is a compile time error.
```

## Structs

Structs are used to group a bunch of related variables (and functions) together.

```cpp
struct name {
    field: type;
    field2: type2;
}
```

When accessing the fields of a struct instance through a reference, the compiler auto-dereferences it.

so the following is valid code:

```rust
struct Foo {
    a: i32;
    b: i32;
}

var instance = Foo{a: 40, b: 2};
var ref = &instance;

// var bar = ref; // makes 'bar' a reference to 'instance'
var bar = *ref; // same as var bar = foo
ref.a + ref.b; // 42
// the above is translated into the following by the compiler:
// *(ref).a + *(ref).b;
```

### Binding functions to structs

There are associated functions and bound functions.

associated functions "belong" to the struct itself, not instances of it.

bound functions "belong" to instances of the struct, and cannot be called without an instance.

```rust
struct Person {
    name: String;
    age: i32;
}

// associated function
fn [static Person]new(String name, i32 age) -> Person {
    return Person{name: name, age: age};
}

// bound functions
fn [this &const Person]getName() -> String {
    return this.name;
}

fn [this &Person]changeName(String name) {
    this.name = name;
}

fn [this &const Person]getAge() -> i32 {
    return this.age;
}

// usage
var p = Person::new("Robert", 42);
p.getName(); // "Robert"
p.changeName("John");
p.getName(); // "John"
p.getAge(); // 42
```

## Enums

Enums are used to create a type that can only be one of a list.

Enums can also capture values.

```rust
// declaring
enum states {
    NONE,
    ON,
    OFF
}

enum types {
    INT(i32),
    FLOAT(f32),
    STRING(String),
    ALL(i32, f32, String)
}

// using
var state = states::NONE;
var number = types::INT(42);
var answer = number.0; // answer == 42
var all = types::ALL(42, 3.14, "What are the numbers?");
all.0; // 42
all.1; // 3.14
all.2; // "What are the numbers?"
```

## Generics

Generics allow a type in a function or struct to be any type the user provides.

```rust
// structs
struct Foo<T> {
    value: T;
}
var value = Foo<i32>{value: 12};
var str = Foo<String>{value: String::from_str("Hello, World!")};

// functions
fn add<T>(T a, T b) -> T {
    return a + b;
}
add<i32>(40, 2); // 42
add<f32>(41.5, 0.5); // 42
```

A generic type can be limited to be only a few types:

```rust
fn add_num<T(i32, f32)>(T a, T b) -> T {
    return a + b;
}

add_num<i32>(40, 2); // compiles fine
add_num<char>('a', 'b'); // compilation error!
```

## The `defer` statement

The `defer` statement defers the execution of a function call until the parent (surrounding) function returns.

The deferred call's arguments are evaluated immediately, but the function call is not executed until the parent function returns.

## Modules

A module is used to group together a bunch of types, variables, constants, enums, structs, and functions that do a single thing. each module has its own namespace.<br>

Anything in a module is private by default - that means the code importing it can't see anything inside the module. stuff can be made public by adding the `export` keyword before declaring it.

```cpp
// declaring
module The {
    export const answer = 42;
}

// using
The::answer; // 42
using The::answer;
answer; // 42
```

A whole file can be made a module by adding `module <name>;` at the top.

for example:

**file1:**

```rust
module answers;

const answer = 42;
export fn TheAnswer() -> i32 {
    return answer;
}
```

**file2:**

```rust
import "file1/";
// above can also be:
// import "answers";
// if the name of the file is "answers.<ext>"

file1::answers::TheAnswer(); // 42;

// the namespace of the 'file1::answers' module can be brought
// into the current scope like this:
using file1::answers;
TheAnswer(); // 42
```

### Importing files and modules

A file can be imported by `import "<filename>";`, `<filename>` being the path to the file (absolute or relative to the provided import paths).

If a file contains a single module and its name is the same as the module, it can be imported with `import "<modulename>";`, `<modulename>` being the name of the module.

If a file contains multiple modules, single modules can be imported with `import "<filename>/<modulename>";`.

A module can be imported using a different name with the `as` keyword:

```rust
import "file/mod" as m;
```

When a file is imported, it gets it's own namespace. this is useful when multiple modules are defined in a single file.

this doesn't apply when a module from a file (`import "<file>/<module>";` ) or when a module with the same name as its file (`import "<module>";`) is imported.

## Scopes

A scope is whatever is inside a pair of opening and closing braces (`{}`) (a block).

A scope has it's own namespace that has higher precedence than it's parent scope. that means that if a scope and it's parent scope have an object with the same name, referencing it in the child scope will use the one defined in it. to use the one from the parent scope, the `::` (scope resolution) operator is used in it's prefix variant.

### Namespaces

Each function, enum, struct, module, and scope has it's own namespace.

The global namespace holds module names and global function and variable (and constant) names.

## Keywords

| Keyword | Description |
| --- | --- |
| `var` | declare a variable |
| `const` | declare a constant/make a reference constant |
| `static` |     |
| `fn` | declaring functions |
| `return` | return from a function |
| all the default types | N/A |
| `null` | empty value for each type |
| `enum` | declare an enum |
| `struct` | declare a struct |
| `if` | if statement |
| `else` | if statement |
| `switch` | switch statement |
| `module` | declare a module |
| `export` | export an object in a module |
| `import` | import a module (or parts of it) |
| `as` | change the name of an imported module |
| `using` | bring the namespace of a module into the current scope. |
| `while` | declare a while loop |
| `for` | declare a for loop |
| `type` | define a custom type |
| `typeof` | get the type of an object |
