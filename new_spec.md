# Itai-Lang

## Destructors
A destructor is a bound function that is automatically called
by the compiler when an object goes out of scope.<br>
**Example:**
```rust
import "std/memory";

struct Ptr<T> {
	p: &T

	fn new(T t) -> This {
		return This{p: std::memory::new<T>(t)};
	}

	#[destructor]
	fn free(&this) {
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

## Error handling (Optional types)
Error handling is achieved using the `Error` base class and Optionals.<br>
Optionals are declared by appending a `?` (question mark) to a type (e.g `i64?` is an optional `i64`). values are automatically wrapped in An optional when returned in a function returning an optional or assigned to a variable that is an optional type.<br>

An empty optional contains an error (`Error` or derived structs), empty errors are represented by `Error::new("")` or the shorthand `None`.<br>
To get the state of an optional, the `is_error() -> bool` bound function is used.

An optional must be unwrapped to access the value stored in it, there are three ways to do so:
1) Force unwrap: panic if there is no value.
2) `or` blocks: executed on error, and provided with the error which is accessible using the parameter. `or` blocks must return.
3) Propagate the error using the `?` operator: used to let caller handle the error.
4) Using the `value_or(T) -> T` bound function: for default values.

```rust
import "std/errors";

using errors::Error;

struct NotDivisibleByTwoError < Error {
    value: i32;

    fn new(value: i32) -> This {
        return This{value};
    }

    public fn what(&this) override -> String {
        return String::format("'%d' is not divisible by two.", this);
    }
}

fn do_stuff(values: &[i32]) -> i64? {
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


fn test() -> i64? {
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

fn main() {
    // '&err' is syntactic sugar for 'err: &Error'.
    var value = test() or &err {
        eprintln("Error: %s", err.what());
        return 1;
    }
    println("result: %d", value);
}
```
The base struct `Error` stores a string:
```rust
fn error() -> i32? {
    return Error::new("error message");
}
```

If a function need to return nothing or an error, the special `void?` type is used. It represents nothing or an error.
```rust
import "std/io";

fn error_or_nothing() -> void? {
    if !something_that_might_fail() {
        return Error::new("The thing failed");
    }
    // No need to return any value, after all void? means "error or nothing".
    // The explicit return statement isn't needed either. Unless an error is returned,
    // functions returning void? are the same as functions returning no value if an error is not returned.
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
    fn print(&this);
}

trait Add<Rhs, Output> {
    fn add(&this, rhs: Rhs) -> Output;
}

struct NumberList implements Printable, Add<[i32], This> {
    public numbers: [i32];

    public fn print(&this) {
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
    virtual fn print() {
        println("Base");
    }
}

struct Derived < Base {
    public fn print() override {
        println("Derived");
    }
}

fn main() {
    var base = Base{};
    var derived = Derived{};

    base.print(); // "Base"
    derived.print(); // "Derived"

    var derived_as_base = as<Base>(derived);
    derived_as_base.print(); // still "Derived".
}
```

To check the actual type of a polymorphic type, the `is` operator is used:
```rust
// Using the same types as the previous example:

fn test(a: Base) {
	if a is Base {
		println("The type of 'a' is Base");
	} else if a is Derived {
		println("The type of 'a' is Derived");
	} else {
		println("The type of 'a' is unknown");
	}
}
```


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

* There are no implicit type conversions the only exception being untyped number literals which are cast according to their context (e.g. when assigning to a variable with an unsigned type).
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
#[source(add.o)]
extern fn add(i32, i32) -> i32;

fn main() -> i32 {
	return add(40, 2);
}
```
The C file has to be compiled to an object file (`.o`), and then the itai-lang compiler will link it to the program.

