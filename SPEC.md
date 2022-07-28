# Itai-lang

### Example

```rust
import "std/io";
import "std/os";
import "std/strings";
import "std/types";

module Collections {

struct ListItem<T> {
    value: T?;
    next: Box<ListItem<T>>;
    prev: Box<ListIem<T>>
}

public struct List<T> {
    head: Box<ListItem<T>>?;
    item_count: usize;

    public fn new() -> This<T> {
        return This<T>{
            data: null,
            item_count: 0
        };
    }

    public fn isEmpty(&const this) -> bool {
        return .item_count == 0;
    }

    public fn length(&const this) -> usize {
        return .item_count;
    }

    public fn push(&this, value: T) {
        var item = new ListItem{value, null};
        if !head.has_value() {
            .head = item;
        } else {
            .head!.next = item;
            .head!.prev = head;
            .head = item;
        }
        .item_count++;
    }

    public fn pop(&this) -> T? {
        if item_count == 0 {
            return null;
        }
        var value = (*.head!).value;
        .head = .head!.prev;
        return value;
    }
} // struct List<T>
} // module Collections

using os::Args;
using Collections::Stack;
fn main() {
    if Args::len() < 2 {
        io::printerrln("USAGE: %s [numbers]", os::args[0]);
        return 1;
    }
    var args = Args::args[1:];
    var list = List<i32>::new(16);
    for arg in args {
        if types::isdigit(arg) {
            list.push(strings::atoi(arg));
        } else {
           io::printerrln("WARNING: %s is not a number!", arg);
        }
    }

    while !list.isEmpty() {
        io::println("%d", list.pop());
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
#[warn_uninitialized(true)] // turn on warnings for uninitialized variables
```

## Types

### Primitives

**NOTE:** `str` isn't a primitive, but it's an alias to one.

| Type | Equivalent in C |
| --- | --- |
| `i8` | `int8_t` (`signed char`) |
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
| `str` (pointer to string literal) | `const char *` |

### Advanced (from the standard library)

These aren't primitive types, but they are pretty basic in modern programming languages, so they are automatically imported and put in the global scope.

| Type | Description |
| ---  |     ---     |
| `String` | Mutable string with advanced features. |
| `Vector<T>` | A vector for any type (using generics). has advanced features. |
| `Optional<T>` | A type that either holds a value of type `T` or no value. |
| `Box<T>` | Allocates an instance of the type `T` on the heap. |

### `Box<T>` (store data on the heap)

The `Box<T>` type is integrated into the language for easier usage.<br>
Note that `Box<T>` doesn't handle deallocating automatically, instead the compiler will reference count instances of it, and call its `free()` bound function when no references to the instance are left. If you need to explicitly free a boxed value, you can manually call the `free()` bound function.<br>
To create a new boxed type, the `new` keyword is used:
```rust
struct Data {
    value: u32;
    // ...
}

var boxed_data = new Data{0, /* ... */};
```
For builtin types like integers, you simply use the literal/value casting it if neccesary:
```rust
var boxed_i32 = new 42i32; // Box<i32> infered
var boxed_usize = new 2usize; // Box<usize> infered
var boxed_str = new "Hello!"; // Box<str> infered
var five = 5; // i64 infered
var boxed_five = new five; // Box<i64> infered.
```

To access a boxed value, simply dereference it, but unlike references boxed structs don't auto-dereference.<br>
Note that assigning a boxed value to another boxed value with the same underlying type doesn't require a dereference.
```go
var value = (*boxed_data).value;
var unboxed_data = *boxed_data;
var value_again = unboxed_data.value;
var boxed_value_again = boxed_value;
```

### Optional types

The `Optional<T>` type is integrated into the language and its type system for easier usage.<br>
To make a type optional, add a `?` (question mark) after it:
```rust
i32? // Optional<i32>
usize? // Optional<usize>
str? // Optional<str>
String? // Optional<String>
Vector<i32>? // Optional<Vector<i32>>
```
As long as the type of the optional is explicitly defined, you can assign the value to be stored in the optional directly:
```rust
var maybe_a_number: i32? = 4;
```
To create an empty optional, assign `null` to it:
```rust
var not_a_number: i32? = null;
```
When assigning `null` to an optional, the type can be ommited as long as it can be infered by a later assignment or by returning it from a function with a defined optional return type:
```rust
fn maybe_number(value: i32) -> i32? {
    var maybe = null;
    maybe = value; // i32? is infered from the type of 'value'.
    return maybe;
}

fn not_a_number() -> i32? {
    var nothing = null;
    return nothing; // i32? is infered from the function return type.
}
```

To force unwrap an optional, you can use the postfix `!` operator:
```rust
var a: i32? = 42;
a!; // 42 or panic if there is no value.
```
The none-coalescing operator is supported:
```rust
var a: i32? = null;
var b: i32? = 4;

// the type to the left of the operator must be either an optional with the same underlying type, or the underlying type.
// the following is invalid:
// var a: i32? = 3;
// var b: str? = "three";
// a ?? b; // invalid
a ?? 3; // 3
a ?? b; // 4
// chaining also works
a ?? b ?? 3; // 4
```

### Function type

The function type is the type of function literals. it can also be used to store references to regular functions.

```rust
fn(<param types>) -> T
```
`<param types>` is the parameter types, and it can be empty.<br>
`T` is the return type (its optional along with the `->` (arrow) if nothing is returned).

### Example

```rust
fn function() { io::println("function()"); }
var func = null;
var func2 = function; // the type of 'func2' is infered as 'fn()'
func = nullfn; // the type of 'func' is infered as fn()?

// all calls bellow call function()
function();
func!();
func2();
```

### Reference and Array types

Each type has reference and array variants of itself.

In the table bellow, one type from each group (primitives, advanced, fn types) is shown.<br>
Note that you cannot reference a function type as function types are already a reference to a function.

| Type | Reference variant | Array variant |
| ---  |        ---        |      ---      |
| `u8` | `&u8`, `&const u8` | `[u8]` |
| `Vector<T>` | `&Vector<T>`, `&const Vector<T>` | `[Vector<T>]` |
| `fn(...) -> T` | N/A | `[fn<T>(...)]` |

The array variant can be mixed with the reference variant, for example:

```rust
&[u8] // reference to an array of type u8 and unknown size
&const[u8; 4] // read-only reference to an array of type u8, size 4
```

### Defining custom types

Custom types are defined using the `type` keyword:

```go
type Number: i32;
var num: Number = 12;
Number typeof num; // true
Number typeof 12; // false
```
Functions can be bound to custom types by adding them inside a pair of opening and closing braces (`{}`):
```rust
type Number: i32 {
    public fn from_i32(value: i32) -> This {
        return This{value};
    }

    public fn equal(&const this, other: &const This) -> bool {
        return this == other;
    }
}
```

### Type casting

Type casting is done using the `as` operator.<br>
Note that it returns an optional which will be empty in the case of a failed cast.
```rust
as<type>(value) // return value as 'type'.
// === examples ===
var n = as<i32>(12)!; // 12i32
var f = as<f32>(1)!; // 1.0f32
var d = as<f64>(f)!; // 1.0f64
```

## Literals

### Numbers

For decimal numbers, simply the number.<br>
Note that the default integer type is `i64`, but the default float type is `f32`.

The `_` (underscore) character may be used to make the numbers more readable for humans (it's ignored by the compiler).

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

Number literals can be postfixed with an integer type to cast them to that type:
```rust
42i32
12i64
5f32 // 5.0
7u8
```

### Characters and Strings

Characters are put in between `'` (single quotes) and strings in between `"` (double quotes).<br>

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

Array elements are a comma separated list inside a pair of opening and closing brackets (`[]`).

```rust
[1, 2, 3, 4, 42] // [i32; 5]
['a', 'b', 'c'] // [char; 3]
["s1", "s2", "s3"] // [str; 3]
// nested arrays
[[1, 2, 3], [4, 5, 6]] // [[i32; 3]; 2]
```

### Structs

Struct literals are simply the name of the struct followed by a list of the field names, a colon, and the value in between a pair of opening and closing braces (`{}`).<br>
Note that if a field is asssigned a value stored in a variable with the same name, the field name can be omitted.
```rust
struct Foo {
    i32 a;
    i32 b;
}

var instance = Foo{a: 20, b: 2};
var a = 42;
var instance2 = Foo{a, b: 0}; // instance2.a == 42
```

### Function literals

Function literals are simply `fn` followed by the argument list inside parentheses, `->` and the return type.<br>
Note that the parameter and return types can be ommited for generic function literals, for the details read the generics section.
```go
var add = fn(a: i32, b: i32) -> i32 { return a + b; }; // type is fn<i32>(i32, i32)
add(1, 2); // returns 3
var split = fn(s: String) -> String {
    var len = s.length()/2;
    return s.substring(0, len);
}; // type is fn(String) -> String
split("abcd"); // "ab"
```

## Operators

### Math

| Operator |     Description     |  Type  |
|    ---   |         ---         |  ---   |
| `+`      | addition            | infix, prefix |
| `-`      | subtraction         | infix, prefix |
| `*`      | multiplication      | infix |
| `/`      | division            | infix |
| `%`      | modulus (remainder) | infix |

### Bitwise

| Operator |        Description      |  Type  |
|   ---    |           ---           |  ---   |
| `&`      | AND                     | infix  |
| `\|`     | OR                      | infix  |
| `^`      | XOR                     | infix  |
| `~`      | Binary One's Complement | prefix |
| `<<`     | left shift              | infix  |
| `>>`     | right shift             | infix  |

### Conditional

| Operator |           Description           | Type  |
|   ---    |              ---                |  ---  |
| `&&`     | conditional AND                 | infix |
| `\|\|`   | conditional OR                  | infix |
| `??`     | none-coalescing (for optionals) | infix |

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
| `\|=` | OR+assignment |
| `^=` | XOR+assignment | infix |
| `<<=` | left shift+assignment | infix |
| `>>=` | right shift+assignment | infix |

### Equality & Comparison

| Operator | Description | Type |
| --- | --- | --- |
| `==` | equal | infix |
| `!=` | not equal | infix |
| `>` | larger than... | infix |
| `<` | smaller than... | infix |
| `>=` | larger or equal to... | infix |
| `<=` | smaller or equal to... | infix |

### Other

| Operator | Description | Type |
| --- | --- | --- |
| `sizeof` | returns the size of an object | prefix |
| `typeof` | returns the type of an object/compares an object with a type | prefix, infix |
| `!` | force unwrap (for optionals) | postfix |
| `?:` | conditional expression (ternary operator) | N/A |
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
    else => {
        io::println("Wrong!");
    }    
}
```

A switch can be exited with the `break` keyword.

The braces aren't needed for single expressions:

```go
var a = 42;
switch a {
    42 => io::println("The answer!");
    else => io::println("Wrong answer!");
}
```

Cases can't fallthrough, but each case can match multiple values:

```go
var a = 24;
switch a {
    24 | 42 => io::println("The answer (or not)");
    else => io::println("Wrong answer!");
}
```

### Loops

There are 2 types of loops: `while` and `for`.

```rust
var i = 0;
while i < 10 {
    i++;
    io::println("%d", i);
}
```

```rust
for var i = 0; i < 10; i++ {
    io::println("%d", i);
}
```
`for` loops also support iterators (see the iterators section for more information on iterators):
```rust
var nums = [1, 2, 3, 4];
for num: i32 in nums {
    io::println("%d", num)
}

// The type of the item variable can be infered.
for num in nums { // i32 is infered as the type of 'num'.
    io::println("%d", num);
}

// A Reference of each item can be taken as well.
for num: &i32 in nums {
    io::println("%d", *num);
}

// Shorthand for taking a reference.
for &num in nums { // &i32 is infered as the type of 'num'.
    io::println("%d", *num);
}

// The 'in' keyword is short for using the iterator manually (note the reset call after the loop).
for var num = nums.next(); !num.is_end(); num = nums.next() {
    // ...
}
nums.reset();
```

## Iterators
Iterators are used to iterate over iterable types mostly in `for` loops.<br>
for a type to be iterable, it has to implement the `Iterator` trait:

```rust
trait Iterator<T> {
    // return the next item.
    fn next(&this) -> T;
    // return the next item as a reference.
    fn next_ref(&this) -> &T;
    // return true if the end is reached.
    fn is_end(&const this) -> bool;
    // reset the iterator.
    fn reset(&this);
}
```

## Variables

Variables are declared using the `var` keyword followed by the name of the variable. A value can also be assigned by adding `=` followed by an expression that returns a value. if that is done, the compiler infers the type automatically, else the type has to be provided by adding a `:` followed by the type before the `=` (if used).<br>

Constants are the same but you have to assign a value to them at declaration (and you cannot reassign a value to them).

```go
// === Variables ===
var name = value; // the type of 'value' has to be possible to infer as there is no cast or explicit type.
// explicit typing
var name: type = value;

// === Constants ===
const name = value;
const name: type = value;
```

## Functions

Functions are declared with the `fn` keyword followed by a pair of opening and closing parentheses (`()`) containing the parameters. `->` followed by the return type can be added if the function returns something.<br>

A function is called by its name followed by opening and closing parentheses (`()`) containing the arguments if applicable.

```rust
// declaring
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// calling
add(40, 2); // 42
```

### Variable arguments

Variable argument functions (variadic functions) are declared by adding a last parameter of type `...`. Note that at least one parameter is required before the `...`.

The following api is available to access the arguments:

- `va_start(ap, lastarg)`
  
- `va_end(ap)`
  
- `va_arg<T>(ap) -> T`
  
- `va_copy(dest, src)`
  

The above functions are internal and not from the standard library.
Note that they are unsafe as variadic arguments are not type-safe, so use them with caution.

**example:**

```rust
fn printf(format: str, ap: ...) {
    var out: String;
    va_start(ap, format);
    defer va_end(ap);
    for var i = 0; format[i] != '\0'; i++ {
        if format[i] == '%' {
            i++;
            switch format[i] {
                'i' | 'd' => out.append(String::from(va_arg<i32>(ap)));
                'c' => out.append(va_arg<char>(ap));
                's' => out.append(va_arg<str>(ap));
                '%' => out.append('%');
                else => out.append(format[i-1 : i]);
            }
        } else {
            out.append(format[i]);
            i++;
        }
    }

    io::print(out.as_str());
}
```

## Arrays
Arrays are a fixed type fixed size list of items.

```go
var array = [1, 2, 3, 4];
[i32; 4] typeof array; // true
var a: [i32; 2];
a[0] = 1;
a[1] = 2;

// 2d arrays
var two_d = [
    [1, 2, 3, 4],
    [5, 6, 7, 8]
];
[[i32; 4]; 2] typeof two_d; // true
var b: [[i32; 2]; 2];
b[0] = [1, 2];
b[1] = [3, 4];

// accessing elements
array[0]; // 1
a[1]; // 2
two_d[0][1]; // 6
b[1][0]; // 3
```

## References

References also known as pointers allow you to "point" to a variable instead of copying it.

A reference of a variable can be taken by adding `&` before it, and the value of it can be accessed by dereferencing it using the prefix `*` operator.<br>
Note that references to arrays can ommit the array size to allow for an array of any size.

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

// references to arrays
var array = [1, 2, 3];
var aref: &[i32; 3] = array;
var bref: &[i32] = array;
```

Read only or constant references are references that can only be used to read a value, not change it.

A constant reference can be taken by adding `&const` before the variable, and it can be dereferenced the same way as a regular reference:

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

Structs are used to group a related variables (and functions) together.<br>
Fields are private by default meaning that only the enclosing module can access them. To make a field available to everyone, the `public` keyword can be added before the field declaration.

```cpp
struct name {
    field: type;
    field2: type2;
    public field3: type3;
}
```

When accessing the fields of a struct instance through a reference, the compiler auto-dereferences it.<br>
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
ref.a + ref.b; // 42 <==== this part is the example.
// the above is translated into the following by the compiler:
// (*ref).a + (*ref).b;
```

### Binding functions to structs, enums & custom types

There are 2 types of functions you can bind to a struct/enum/any custom types:
 1) associated functions "belong" to the struct itself, not instances of it.
 2) bound functions "belong" to instances of the struct, and cannot be called without an instance.

Both associated and bound functions have to be defined inside the struct/enum to which they belong. to be visible outside of the enclosing module, they have to be made public by adding the `public` keyword before defining them.

Bound functions most have `&this` or `&const this` as the first parameter. this is short for `this: &This` or `this: &const This` respectively.<br>
The `This` type is defined automatically and is an alias to the type of the enclosing struct, enum or custom type.

As bound functions are likely to access members/call other bound functions, the shorthand `.member` (instead of `this.member`) is allowed.

```rust
struct Person {
    name: String;
    age: i32;

    // associated function
    public fn new(name: String, age: i32) -> This {
        return This{name, age};
    }

    // bound functions
    public fn getName(&const this) -> String {
        // .name is the same as this.name
        return .name
    }

    public fn changeName(&this, name: String) {
        .name = name;
    }

    public fn getAge(&const this) -> i32 {
        return .age;
    }
}

// usage
var p = Person::new("Robert", 42);
p.getName(); // "Robert"
p.changeName("John");
p.getName(); // "John"
p.getAge(); // 42
```

## Enums

Enums are used to create a type that can only be one value out of a list.

Enums can also capture values.

```rust
// declaring
enum States {
    None,
    On,
    Off
}

enum Types {
    Int(i32),
    Float(f32),
    String(String),
    All(i32, f32, String)
}

enum MyOptional<T> {
    Some(T),
    None
}

// usage
var state = States::None;
var number = Types::Int(42);
var answer = number.0; // answer == 42
var all = Types::All(42, 3.14, "What are the numbers?");
all.0; // 42
all.1; // 3.14
all.2; // "What are the numbers?"

var maybe_number = MyOptional::Some(number);
var the_number = maybe_number.0;
var nothing_here = MyOptional::None;
```

## Generics

Generics allow a type in a function or struct to be any type the user provides.<br>
In a function, the generic type can be infered in the call.

```rust
// structs
struct Foo<T> {
    value: T;
}
var value = Foo<i32>{value: 12};
var string = Foo<String>{value: String::from_str("Hello, World!")};

// functions
fn add<T>(T a, T b) -> T {
    return a + b;
}
add<i32>(40, 2); // 42
add<f32>(41.5, 0.5); // 42

add(20, 22); // i32 inferred
add(21.5, 20.5) // f32 inferred
```

A generic type can be limited to be only a few types. this is useful for allowing only types that implement 1 or more specific traits (more about traits later):

```rust
fn add_num<T(i32, f32)>(T a, T b) -> T {
    return a + b;
}

add_num<i32>(40, 2); // compiles fine
add_num<char>('a', 'b'); // compilation error!
add_num("a", "b"); // str inferred, compilation error!
```
**Note:** in function literals, a generic function is defined by not giving a type to the arguments and/or return type.
If there are no calls to the generic function literal, the parameter and return types can't be infered and so a compilation error will occur.
```go
var add = fn(a, b) { return a + b; };

add(1, 2); // i32 inferred
add(1.5, 1.5); // f32 inferred
```
Note that generics in function literals are more limited (no type limiting, no generic parameters), that is because function literals should be used mostly for callbacks where the types are most likely known/easy to infer and so the verbosity of explicit typing isn't needed and omitting the types will make the code easier to read and understand.

### Traits
Traits are a compile-time utility that allows limiting generic parameters to only types that implement a trait.<br>
Traits are declared like structs, but unlike structs they can only contain function declarations:
```rust
trait Equality {
    fn equal(&const this, other: &const This) -> bool;
}
```
All function declarations inside a trait are public by default as making a trait require a private function is pointless because a trait defines an api that has many implementations (all the types that implement it), and an api is an application **_public_** interface.

Implementing a trait for a struct or enum is done by adding `implements <traitname>` after the name. multiple traits can be implemented by adding them after a `+`. Remember, functions required to implement a trait must be public:
```rust
trait Hashable {
    fn hash(&const this) -> u64;
}

// Equality is the trait declared in the code snippet above.
struct Number implements Equality + Hashable {
    value: i64;

    public fn get_value(&const this) -> i64 {
        return .value;
    }

    public fn equal(&const this, other: &const This) -> bool {
        return .value == other.get_value();
    }

    public fn hash(&const this) -> u64 {
        return (.value >> 2) * 31;
    }
}
```
Custom types can also implement traits:
```rust
type MyNumber: i32 implements Equality {
    public fn equal(&const this, other: &const This) -> bool {
        return this == other;
    }
}
```

Using traits to limit generic parameters is done by limiting the generic parameter using the trait name as a type. Again, if multiple traits have to be implemented, they can be put as a list with '+' as a separator:
```rust
fn do_something<T(Equality + Hashable)>(value: &const T) {
    // ...
    var hash = value.hash();
    //...
    var other: T = get_something();
    if value.equal(other) {
        // ...
    }
    // ...
}
```
Providing a type that doesn't implement the `Equality` and `Hashable` traits as a generic paramteter to the function above will result in a compile-time error.

## The `defer` statement

The `defer` statement defers the execution of a function call until the parent (surrounding) function returns.

The deferred call's arguments are evaluated immediately, but the function call is not executed until the parent function returns.

## Modules

A module is used to group related together types, variables, constants, enums, structs and functions. Each module has its own namespace.<br>

Anything in a module is private by default - that means the code importing it can't see anything inside the module. To make something public, add the `public` keyword before declaring it.

```cpp
// declaring a module
module The {
    public const answer = 42;
}

// using the module
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
public fn TheAnswer() -> i32 {
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

A scope generally is whatever is inside a pair of opening and closing braces (`{}`) (a block). More specifically, each function has a scope and each for loop has a scope (so the initializer clause can be a variable declaration).
further scopes can be added with blocks (`{ ... }`).

Object names in the current scope "shadow" objects with the same name in the parent scopes.<br>
In other words, if a child scope and its parent scope have an object with the same name, referencing the object in the child scope will use the one defined in the child scope. to use the object from the parent scope, the scope resolution operator (`::`) is used in its prefix variant.<br>
**Example:**<br>
```rust
fn main() {
    var a = 42;
    {
        var a = 50;
        a; // 50
        ::a; // 42
    }
    a; // 42
}

```

### Namespaces

A namespace is like a scope but for identifiers.<br>
Function and global variable names, enum names, and struct names live in the global namespace (unless defined inside a module).

Custom namespaces cannot be declared, but each module has its own namespace.

## Keywords

| Keyword  | Description  |
|   ---    |     ---      |
| `var`    | Declare a variable. |
| `const`  | Declare a constant/make a reference constant |
| `fn`     | Declaring functions |
| `return` | Return from a function |
| `new`    | Create a new boxed value. |
| `null`   | The value of an empty optional. |
| `enum`   | Declare an enum. |
| `struct` | Declare a struct. |
| `if`     | `if` statement. |
| `else`   | `if` statement. |
| `switch` | `switch` statement. |
| `public` | Make a variable/struct and enum field/function/enum/struct accessible from outside the current module. |
| `module` | Declare a module. |
| `import` | Import a module (or parts of it). |
| `as`     | Cast/Change the name of an imported module. |
| `using`  | Bring the namespace of a module into the current scope. |
| `while`  | `while` loop statement. |
| `for`    | `for` loop statement. |
| `in`     | Use an iterator in a for loop. |
| `type`   | Define a custom type. |
| `typeof` | Get the type of an object. |
