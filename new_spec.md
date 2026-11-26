# Itai-Lang

Itai-Lang aims to be a systems programming language that combines the simplicity and power of C
with modern features such as object visibility (in modules and structs), memory safety,
a strict type system, and support for object oriented programming (while not forcing it.)
<br>
This document attempts to cover the most important parts of the language, the parts that
make it different than other similar languages, starting with the toplevel
(code organization and visibility) and going all the way down to primitive types and basic operations.


## Code organization and visibility
### Modules
The main unit of visibility in itai-lang is a module. A module represents a single file, and is
declared by adding `module "<modulename>";` as the first line in a file.<br>
The name of a module must match the actual filename (minus the file extension.)<br>
#### Importing modules
To import a module from within another module, use `import "/path/to/module/I/want/to/import"`.
An example of a good way to organize code is as follows:
```
MyProject/
└── src
    ├── Helpers.ilc
    ├── MyProject.ilc
    └── Types
        ├── HashTable.ilc
        └── StringTable.ilc
```
To import `Helpers.ilc` from `MyProject.ilc`, `import "Helpers";` would be used.<br>
To import `Types/StringTable.ilc` from `MyProject.ilc`, `import "Types/StringTable";` would be used.
#### Visibility in modules
By default, all types, structs, and functions in a module are private. To mark any one of those things
as public, add the keyword `public` before the declarations.
**Example:**
```rust
module "ValueOps";

public struct Value<T> { // set as public (visible outside this module)
    value: T; // private (only visible inside this struct)

    public fn setValue(&this, val: T) { // set as public (visible wherever Value<T> is visible)
        this.value = val;
    }
    public fn getValue(&readonly this) -> T { // set as public (visible wherever Value<T> is visible)
        return this.value;
    }
}

fn doSomethingInternalToThisMoudle() { // private (only visible inside this module)
    // stuff...
}

public fn makeIntValue(val: i32) -> Value<i32> { // set as public (visible outside this module)
    var v = Value{};
    v.setValue(val);
    return v;
}
```

## Destructors
A destructor is a bound function that is automatically called
by the compiler when an object goes out of scope.<br>
Destructors are implemented using the `Destrucatble` trait, which is defined as following:
```rust
trait Destrucatble {
	fn destruct(&this);
}
```
**Example:**
```rust
import "std/memory";
import "std/traits";

struct Ptr<T> implements traits::Destructable {
	p: &T

	fn new(t: T) -> This {
		return This{p: std::memory::new<T>(t)};
	}

	// This is the destructor
	fn destruct(&this) {
		std::memory::free(this.p);
	}
}

// Example usage:
fn main() {
	var ptr = Ptr<i32>::new(42);
	// ptr goes out of scope, so its destructor is called.
}
```

## Memory managagement

The recommended way to manage heap allocated memory is with the `Box<T>` type.<br>
`Box<T>` frees the memory when the object is destructed so memory leaks are prevented.

### Example usage of `Box<T>`
```rust
{
    var i: Box<i32> = Box<i32>::new(42);
    println("value: %d", i.get());
    // stuff...
    // 'i' goes out of scope, so the destructor is called: 'i.free()'
}
```

Raw pointers to heap memory are also supported but not recommended.<br>
They don't have destructors, so they have to be manually freed. 

### Example usage of raw pointers
```rust
import "std/memory";
{
    using memory::{new, free};
    var i: &i32 = new(42); // fn new<T>(T) -> &T;
    println("value: %d", *i);
    // stuff...
    free(i);
}
```

## Error handling (Optional & Result types)
Error handling is achieved using the `Error` base class and Optional & Result types.<br>

### Optional types
A type is declared optional by appending a `?` (question mark) to it (e.g `i64?` is an optional `i64`). Values are automatically wrapped in an optional when returned in a function returning an optional type or assigned to a variable of an optional type.<br>

An empty optional is represented by the value `None`.<br>
To get the state of an optional, the `has_value() -> bool` bound function is used.

### Result types
A type is declared as a result type by appending a `!` (exclamantion mark) to it (e.g. `i64!` is a result `i64`). As with optionals, values are automatically wrapped in a result type when returned from a function returning a result type or assigned to a variable of a result type.

An empty result type contains an instance of the error type (`Error` or derived structs).<br>
To get the state of a result type, the `is_error() -> bool` bound function is used.

### Extracting the value from Optional & Result types
Optional & Result types must be unwrapped to extract the value stored in them. There are three ways to do so:
1) Force unwrap using the `!` operator: panic if there is no value.
2) `or` blocks: executed if the optional is empty/the result contains an error. If the type is a result type, the error is accessible using the parameter. `or` blocks must return.
3) Propagate the error using the `?` operator: used to let caller handle the error.
4) Using the `value_or(T) -> T` bound function: for default values.

```rust
import "std/io";

struct NotDivisibleByTwoError < Error {
    value: i32;

    fn new(value: i32) -> This {
        return This{value};
    }

    public fn what(&this) override -> String {
        return String::format("'%d' is not divisible by two.", this);
    }
}

fn do_stuff(values: &[i32]) -> i64! {
    var sum = 0i64;
    for value in values {
        if value % 2 != 0 {
            return NotDivisibleByTwoError::new(value);
        }
        sum += value / 2;
    }

    // 'sum' is automatically wrapped in an optional.
    return sum;
}


fn test() -> i64! {
    var values = [2, 4, 6, 8];

    // use a default value:
    // var result = do_stuff(&values).value_or(42);

    // force unwrap the result (panic if an error):
    //var result = do_stuff(&values)!;

    // return the error if it exists (so it "bubbles" up to main() or until it's handled),
    // or unwrap the value:
    var result = do_stuff(&values)?;

    return result;
}

fn test_optional() -> i64? {
	// '&err' is syntactic sugar for 'err: &Error'.
	var value = test() or &err {
		io::eprintln("Error: %s", err.what());
		return None;
	};
	return value;
}

fn main() {
    var value = test_optional() or {
		// The error is already outputted by 'test_optional()'.
        return 1;
    }
    io::println("result: %d", value);
}
```
The base struct `Error` stores a string:
```rust
fn error() -> i32? {
    return Error::new("error message");
}
```

If a function needs to return nothing or an error, the special `void!` type is used.
```rust
import "std/io";

fn error_or_nothing() -> void! {
    if !something_that_might_fail() {
        return Error::new("The thing failed");
    }
    // No need to return any value because void! means "error or nothing".
    // The explicit return statement isn't needed either. Unless an error is returned,
    // functions returning void! are the same as functions returning no value if an error is not returned.
    return;
}

// Usage:
fn main() {
    error_or_nothing() or &err {
        io::eprintln("error: {}", err.what());
    }
}
```

## Polymorphism

### Compile-time polymorphism

Compile-time polymorphism is achieved using generic types and traits.<br>

**Generic types:**<br>

Generic types allow writing a single function for multiple types:
```rust
fn add<T>(a: T, b: T) -> T {
    return a + b;
}
```
The above example won't compile as `T` doesn't implement the `+` operator.<br>
Type limiting also known as type constraints is a way to limit generic types so only certain types that are known to work are accepted.<br>

To fix the above example, we will use the `std::traits::operators::Add<Rhs, Output>` trait:

```rust
import "std/traits/operators" as ops;

fn add<T(ops::Add<T, T>)>add(a: T, b: T) -> T {
    return a + b;
}
```
Now only types that implement `std::traits::operators::Add<T, T>` will be accepted as arguments.

Multiple traits can be required:
```rust
fn make_table<K(Equality, Hashable), V>() -> Table<K, V> {
    var table = Table<K, V>::new();
    // stuff...
    return table;
}
```

It is also possible to limit generic types to one of a list of types:
```rust
fn to_i32<T(str, String)>(string: T) -> i32 {
    return string.convert_to<i32>();
}
```

**Traits:**<br>

A trait defines functions that implementing types must implement.
Traits are used for generic type constraints. \
A trait can have generic types. \
The `This` type is an alias for the implementing type. \
Functions defined in traits must not be private, so they are automatically marked as public.

```rust
trait Printable {
    fn print(&readonly this);
}

trait Add<Rhs, Output> {
    fn add(&this, rhs: Rhs) -> Output;
}

struct NumberList implements Printable, Add<[i32], This> {
    public numbers: [i32];

    public fn print(&readonly this) {
        println("%a", .numbers);
    }

    public fn add(&this, rhs: [i32]) -> This {
        var new_list = NumberList{numbers: []};
        new_list.numbers.append_array(.numbers);
        new_list.numbers.append_array(rhs);

        return new_list;
    }
}

fn main() {
    var numbers = NumberList{numbers: [1, 2, 3]};
    numbers = numbers + [4, 5, 6];

    println("%a", numbers.numbers); // "[1, 2, 3, 4, 5, 6]"
}
```

### Runtime polymorphism

Runtime polymorphism is achieved using bound function overriding.

A virtual bound function is a function that can be re-declared in derived structs. A virtual function must have an implementation (pure virtual functions are implemented with traits).\
A virtual function can't be private, and is marked as public automatically.

```rust
struct Base {
    virtual fn print(&readonly this) {
        println("Base");
    }
}

struct Derived < Base {
    public fn print(&readonly this) override {
        println("Derived");
    }
}

fn main() {
    var base = Base{};
    var derived = Derived{};

    base.print(); // "Base"
    derived.print(); // "Derived"

    var derived_as_base = as<&Base>(&derived);
    derived_as_base.print(); // still "Derived".
}
```

To check the actual type of a polymorphic type, the `is` operator is used:
```rust
// Using the same types as the previous example:

fn test(a: &Base) {
	if a is &Base {
		println("The type of 'a' is Base");
	} else if a is &Derived {
		println("The type of 'a' is Derived");
	} else {
		println("The type of 'a' is unknown");
	}
}
```

Note that runtime polymorphism only works through pointers, usually to instances stored in the heap using one of the smart pointer types such as `Box<T>`.

## Expect statement
The `expect` statement is used to make sure a condition is true and if not that the error is handled.
The `expect` statement is very similar to the `if` statement, and in a way is the equivalent of `if !<condition>`.
```rust
fn i32_to_i8(number: i32) -> i8? {
	expect number < 256 else {
		return None;
	}
	return as<i8>(number);
}
```

The `else` block must return from the function, exit the program (using any function marked as #[attribute(noreturn)]), or if in a loop `break` or `continue`.
The `expect` statement can also be used as an assertion if the `else` block is omitted:
```rust
fn do_something() {
	expect in_correct_place(); // Panic the program if the condition is false.
}
```

## Global/Function/Struct/Custom type definition order

Functions, global variables, structs and custom types can be used before they are defined although this should be avoided unless necessary because it makes code unreadable.\
The only times this feature should be used are:
1) In indirectly recursive structs (e.g. `struct T { inner: Box<T>; }`).
2) In recursive functions.
3) In variables/struct fields that depend on each other (e.g `struct A { b: B; } struct B { a: Box<A>; }`).

## Type conversions/casts

* There are no implicit type conversions with the only exception being untyped number literals which are cast according to their context (e.g. when assigning to a variable with an unsigned type).
* To cast a value to another type, the `as<T>(T)` operator is used. It will panic if the conversion fails (overflow, underflow, incompatible types etc.). Some casts can be and are typechecked at compile-time.

## Calling C functions (`extern` functions)
It is possible to call C functions by declaring them as `extern`.
For example, to call the following C function (assuming `int` is 32 bit):
```c
int add(int a, int b) {
	return a + b;
}
```
The following code is used:
```rust
#[source("add.o")]
extern fn add(i32, i32) -> i32;

fn main() -> i32 {
	return add(40, 2);
}
```
The C file has to be compiled to an object file (`.o`), and then the itai-lang compiler will link it to the program.

## Struct destructuring
It is possible to assign values stored in a `struct` to variables in a single declaration:
```rust
struct Values {
	a: i32;
	b: &str;
}

fn main() {
	var values = Values{a: 42, b: "The Answer to the Ultimate Question of Life, the Universe, and Everything"};
	var Values{a, b} = values;
	// 'a' is 'values.a', and 'b' is 'values.b'
}
```

The line `var Values{a, b} = values;` is equivalent to the following:
```go
var a = values.a;
var b = values.b;
```

## Variables
There are three types of variables:
1. Rebindable (i.e. they can be reassigned).
2. Non-rebindable (i.e. they cannot be reassigned).
3. Constants (compile-time constants).

An exmaple is in order:
```rust
// 1. Rebindable variables:
var myNumber: i32 = 123;
// The following is ok:
myNumber = 456;
myNumber -= 56;

// 2. Non-rebindable variables:
let myDouble: f64 = 123.456;
// The following cause compile-time errors:
myDouble = 654.321; // BAD
myDouble += 45.679; // BAD

// 3. Compile time constants:
const myConstant: i32 = 321;
// The following cause compile-time errors:
myConstant = 123; // BAD
myConstant -= 23; // BAD
```
Non-rebindable deserves a clarification: it does NOT make the variable immutable. It can still be mutated using a pointer to it:
```rust
// 1. Rebindable variables
var greeting: String = String::new("Hello");
// The following are all ok:
greeting = String::new("Hello, World");
greeting.append("!"); // Note that append mutates the string itself.

// 2. Non-rebindable variables
let myString: String = String::new("my string");
// This is ok:
myString.append(" is very cool!"); // Note that 'myString' is being mutated here.
// This is a compile-time error:
myString = String::new("No longer cool"); // BAD
```
Note that the typename is not needed as long as it is possible to infer it from the initial value.

Constants can NOT be of complex mutable types, meaning a struct that has bound functions that mutate it is NOT allowed (i.e. it has any bound function that takes `&this` rather than `&readonly this`).
Arrays (see below) are allowed, and simply become truly immutable (unlike arrays bound to a `let` variable which can be mutated through pointers.)

## Pointers/references (terms used interchangeably)
A pointer is declared as such: `&<typename>` where `<typename>` is any type (e.g. `i32`, `String`, `f64` etc.).
A pointer can be declared `readonly`. This means that the value it points to can NOT be modified through the pointer.
To get a reference to a variable the prefix `&` operator is used, and to dereference a pointer the prefix `*` operator is used. The `&` operator can be modified with the `readonly` modifier to create a readonly reference whose type is `&readonly <typename>`, see the example below.
```rust
var x: i32 = 123;

// Declare a variable of type "pointer to i32"
// and assign to it a "refrence to the variable x"
let myPtr: &i32 = &x;

print(x); // "123"
// dereference myPtr
print(*myPtr); // "123"

x = 456;
print(x); // "456"
print(*myPtr); // "456"

*myPtr = 789;
print(x); // "789"
print(*myPtr); // "789"

let myReadonlyPtr = &readonly x; // type inferred as &readonly i32
// This is a compile time error:
*myReadonlyPtr = 987; // BAD
```
When accessing struct fields through a pointer to the struct, said pointer is automatically dereferenced:
```rust
struct MyStruct {
    value: i32
}

let x: MyStruct = MyStruct{value: 123};
let ptr: &MyStruct = &x;

// Technically, to access `value` through `ptr` you would need to do this:
(*ptr).value += 1;

// However for convenienve, the following is allowed:
ptr.value -= 1; // The compiler automatically expands this to (*ptr).value -= 1;
```
When combined with the different variable declarations:
```rust
let value: i32 = 123;
let otherValue: i32 = 456;

// 1. rebindable read/write pointer
var rebindableReadWritePtr: &i32 = &value;
// The following is all ok:
*rebindableReadWritePtr += 321;
rebindableReadWritePtr = &otherValue;

// 2. rebindable readonly pointer
var rebindableReadonlyPtr: &readonly i32 = &value;
// The following is ok:
rebindableReadonlyPtr = &otherValue;
// The following is a compile-time error:
*rebindableReadonlyPtr += 654;

// 3. non-rebindable read/write pointer
let nonrebindableReadWritePtr: &i32 = &value;
// This is ok:
*nonrebindableReadWritePtr += 321;
// This is a compile-time error:
nonrebindableReadWritePtr = &otherValue;

// 4. non-rebindable readonly pointer
let nonrebindableReadonlyPtr: &readonly i32 = &value;
// The following all cause a compile-time error:
*nonrebindableReadonlyPtr += 321;
nonrebindableReadonlyPtr = &otherValue;
```
**A very important clarification regarding `readonly`:** It ONLY prevents the variable pointed to from being changed through THIS pointer. The value itself might be changed directly or through other read/write pointers.

## Arrays and slices
An array is declared as such: `var array: [i32; 4] = [1,2,3,4];`, an array of i32 of length 4. In this case, the compiler is able to infer the size from the initializer.
More generally, an array type is `[<typename>; <size>]` where `<size>` MUST be a compile-time constant such as a number literal or a `const` variable.
Arrays cannot be resized once declared, and their length is stored as part of the type and fully tracked by the compiler. Additionally, they cannot be assigned to another variable of type array.
This has an important implication: `[i32; 4]` and `[i32; 6]` are NOT the same type and can NOT be assigned to each other, and you can't pass arrays by value.
This is where slices come in: a slice is basically a view into some backing array. Internally, it is represented as a pointer to the first element in it and a length field, both stored in runtime.

A slice is logically a (fat) pointer into an array, and as such the syntax is the same as for pointers: `&[i32]` is a read/write slice, and `&readonly [i32]` is a read-only slice (i.e. it does not allow mutating elements of the backing array).
Note that a `const` array (i.e. an array assigned to a `const` variable) can only have `&readonly` slices, since it cannot be mutated.
```rust
let myArray = [1,2,3,4]; // type inferred as [i32; 4]
let mySlice = &myArray; // Type inferred as &[i32]

mySlice[0] = 5; // myArray is now [5,2,3,4]

let readonlySlice: &readonly [i32] = &readonly myArray;
// This is a compile time error:
readonlySlice[0] = 1; // BAD
```
Slices can be taken from any range of an array's elements using the following syntax: `&<array>[from:to]`. Note that `to` is EXCLUSIVE, meaning the range is `[from:to)`!
Either `from` or `to` (but not both) can be ommited to mean "from the beginning" and "until the end" respectively.
```rust
let myArray = [1,2,3,4];

let firstTwo = &myArray[:2];
let lastTwo = &myArray[2:];

let middleTwo = &myArray[1:3];
```
