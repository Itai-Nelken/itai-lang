# Itai-Lang

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
Optionals are declared by appending a `?` (question mark) to a type (e.g `i64?` is an optional `i64`). values are automatically wrapped in An optional when returned in a function returning an optional.<br>

An empty optional contains an error (`Error` or derived structs), empty errors are represented by `Error::new("")` or the shorthand `None`.<br>
To get the state of an optional, the `is_error() -> bool` bound function is used.

An optional must be unwraped to access the value stored in it, there are three ways to do so:
1) Force unwrap: panic if there is no value.
2) `or` blocks: have a special variable `err` containing the error and they must return.
3) Propagate the error using the `?` operator: used to let caller handle the error.
4) Using the `value_or(T) -> T` bound function: for default values.

```rust
import "std/errors";

using errors::Error;

struct NotDivisibleByTwoError < Error {
    fn new(value: i32) -> This {
        return as<This>(value);
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

    // 'sum' is automatically wrapped in a Option<i64>.
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
    var value = test() or {
        eprintln("Error: %s", err.what());
        return 1;
    }
    println("result: %d", value);
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

To fix the above example, we will use the `std::traits::operators::Add<T, Out>` trait:

```rust
import "std/traits/operators" as ops;

fn add<T(ops::Add<T, T>)>add(a: T, b: T) -> T {
    return a + b;
}
```
Now only types that implement `std::traits::operators::Add<T, Out>` will be accepted as arguments.

Multiple traits can be required:
```rust
fn make_table<K(Equality, Hashable), V>() -> Table<K, V> {
    var table = Table<K, V>::new();
    // stuff...
    return table;
}
```

It is also possible to limit generic types to a list of types:
```rust
fn to_i32<T(str, String)>(string: T) -> i32 {
    return string.covert_to<i32>();
}
```
**Traits:**<br>

A trait defines functions the implementing type must implement.
Traits are used for generic type constraints.<br>
A trait can have generic types, but they can't be limited.<br>
The `This` type is an alias for the implementing type.

```rust
trait Printable {
    fn print(&this);
}

trait Add<T, Out> {
    fn add(&this, b: B) -> Out;
}

struct NumberList implements Printable, Add<[i32], This> {
    public numbers: [i32];

    public fn print(&this) {
        println("%a", .numbers);
    }

    public fn add(&this, b: This) -> This {
        var new_list = NumberList{numbers: []};
        new_list.numbers.append_array(.numbers);
        new_list.numbers.append_array(b);

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

A virtual bound function is a function that can be redaclared in derived structs. A virtual function must have an implementation (pure virtual functions are implemented with traits).<br>
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
