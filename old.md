# Itai's programming language

## Contents
- [Runtime system](#runtime-system)
- [Basic syntax](#basic-syntax)
  * [Literals](#literals)
  * [Operators](#operators)
- [Comments](#comments)
- [Libraries](#libraries)
  * [Importing](#importing)
  * [Namespaces](#namespaces) 
  * [Creating a library](#creating-a-library)
- [Variables](#variables)
  * [Constants](#constants)
  * [Arrays](#arrays)
- [Types](#types)
  * [Numbers](#numbers)
  * [Text](#text)
  * [Other](#other)
  * [Custom types](#custom-types)
 * [Casting](#casting)
 * [Pointers and References](#pointers-and-references)
- [Loops](#loops)
  * [The `while` loop](#the-while-loop)
  * [The `for` loop](#the-for-loop)
  * [Iterators](#iterators)
- [`if`/`else`](#ifelse)
- [`switch`](#switch)
- [Functions](#functions)
  * [Variable argument functions and methods](#variable-argument-functions-and-methods)
- [Objects](#objects)
  * [Members](#members-variables-in-the-object)
  * [Methods](#methods)
  * [Access modifiers](#access-modifiers)
  * [Inheritance](#inheritance)
  * [Polymorphism](#polymorphism)
- [Generics](#generics)
- [Dynamic memory allocation](#dynamic-memory-allocation)
- [Standard library](#standard-library)
  * [Available without importing](#available-without-importing)

## Runtime system
### Errors
Function and line number are printed followed by a stack trace, for example:
```
Cause: Segmentation fault in "path/to/file":2
Stack trace:
makeSegFault()
	"path/to/file":1
func1(1, 2)
	"path/to/file":2
main()
	"path/to/file":3
```
First the cause is printed: the type of error and the file followed by a colon and the line number. Then the stack trace is printed.<br>
**reading the stack trace:**<br>
The function name is printed (with the arguments it was passed if applicable) followed by `<path/to/file>:<line number>`.<br>
The last function called is printed first.<br>
Methods are printed like this:
```
className.methodName(): instanceName
	"path/to/file":1
```

### Memory management
In this section, the term 'object' means any variable or instance of a class.<br>
**Where are objects created?**<br>
- Objects created inside a function with a known size at compile time are created on the stack, so they are automatically freed when the function returns.<br>
- Global objects are created on the heap and freed automatically when the program exits.<br>
- Objects with an unknown size at compile time are created on the heap and automatically freed.<br>
- Objects allocated with the `new` keyword are created in the heap.<br>
  * Manually allocated objects are NOT freed, you have to free them yourself.<br>
- There is no garbage collector, but if an object allocated on the heap goes out of scope and has no references or pointers to it, it's freed.<br>
- Objects known to be in the heap can be freed with the `delete` keyword. You can't free objects automatically allocated on the heap. doing so will cause a runtime error.

## Basic syntax
Statements are followed by a semicolon (`;`).<br>
Expressions can be grouped inside parentheses (`()`) for higher precedence.<br>

### Literals
**Booleans:** - `true` or `false`. any number other than 0 is `true`, and 0 is `false`.<br>
**Integers:** `<number>` for example: `12`.<br>
**Floats:** `<integer>.<integer>` for example: `3.14`. if no decimal point is provided, `.0` is added. so `12` becomes `12.0`.<br>
**characters:** `'<character>'` for example: `'A'`. only ASCII characters are supported.<br>
**strings:** `"<string>"` for example: `"Hello, World!"`. only ASCII characters are supported.<br>

Any number can be postfixed with a type, so `10usize` means 10 is a number literal of type `usize`.<br>

## Operators
**Math**: addition: `+`, substraction: `-`, multiplication: `*`, division: `/`, modulo (remainder): `%`.<br>
**Bitwise:** and: `&`, or: `|`, not: `!`, xor: `^`, left shift: `<<`, right shift: `>>` // TODO: add the other ones.<br>
**Asignment:** `=`.<br>
**Comparison:** `==`, `!=`, `<`, `>`, `<=`, `>=`.<br>
**Control flow:** and: `&&`, or: `||`<br>
### Assignemt + math and bitwise operators (excluding `!`)
```golang
var int a = 10;

// math operators
a += 10; // a = a + 10
a -= 10; // a = a - 10
a *= 2; // a = a * 2
a /= 2; // a = a / 2

// bitwise operators
a &= 2; // a = a & 2
a |= 2; // a = a | 2
// != is a comparison operator, so it doesn't work like this.
a <<= 2; // a = a << 2
a >>= 2; // a = a >> 2
```
### prefix and postfix `+` and `-`
```golang
var int a=10;
// '+' operator
var b = a++; // 10, a is 11
var c = ++a; // 12, a is 12

// '-' operator
var d = a--; // 12, a is 11
var e = --a; // 10, a is 10
```

## Comments
* Start with `//` until end of line.
* Between `/*` and `*/` (doesn't work as part of an expression, for example: `*x/*y` means dereference `x` and `y` and divide `x` by `y`).

## Libraries
### Creating
TODO

### Importing
```golang
// import a whole library
import library;

// import a single thing from a library
from library import thing;

// import multiple things from a library
from library import thing1, thing2;

// using 'add(a int, b int)' from 'library'
library::add(1, 2);
```

## Namespaces
TODO

## Variables
```golang
var <type> <name>;
var <type> <name> = <value>;
```
**The type can be detected by the compiler automatically:**
```golang
var <name> = <value>;
```
### Constants
```golang
const <type> <name> = <value>;
```
**The type doesn't have to be specified:**
```golang
const <name> = <value>;
```
#### **Examples**
```golang
// regular variables
var int num = 10;
var num2 = 5;

// constants
const int NUM = 10;
const NUM2 = 5;
```
### Arrays
#### **Initializing:**<br>
If all the array is filled at declaration, there is no need to specify the size.
```golang
var <type> <name>[<size>];
var type <name>[<size>] = {<comma separated elements>};
var <name>[<size>] = {<comma separated elements>};
```
#### **accessing elements:**<br>
```golang
// with type declaration:
// var int array[] = {1, 2, 3, 4, 5};
var array[] = {1, 2, 3, 4, 5};
// accesing elements
array[0]; // 1
array[1]; // 2

// writing to elements
array[0]=0;
array[0]; // 0
```
#### **slices:**<br>
You can access parts of the array using **Slices**.<br>
**Syntax:**<br>
```golang
array[<from>:<to>];
```
**Example:**<br>
```golang
// make a new array (of type char)
var array[] = {'a', 'b', 'c', 'd', 'e'};

array[1:3]; // ['b', 'c', 'd']
```
**The following are valid slices:**<br>
- `array[:]` - The entire array.<br>
- `array[2:]` - The entire array except the first 2 elements.<br>
- `array[:2]` - The first 2 elements.<br>

## Types
**`type`** - Used to save types. **usage:** assignment: `var type type_int = type(int)`, comparison: `type_int == type(int)`. you have to explicitly cast the type.<br>

### Boolean
**`bool`** - 1 byte, holds `true` or `false` (which are 1 or 0 respectively).<br>

### Numbers
**`int`** - 4 byte integer.<br>
**`float`** - 32 bit floating point number.<br>
**`size`** - 32 or 64 bit depending on the architecture.<br>

#### **Larger sizes are available by appending the size to the end:**
**`int` sizes:**<br>
**`2`, `4`, `8`, `16`** (bytes).// TODO: change to bits instead of bytes<br>
`int` is an alias for `int4`.<br>
**`float` sizes:**<br>
**`32`, `64`** (bits).<br>
`float` is an alias for `float32`.<br>
#### **unsigned numbers**
**`int` and `size` are signed by default. you can make them unsigned by prefixing them with `u`:**<br>
unsigned int: `uint`.<br>
unsigned size: `usize`.<br>
**You use `u` in combination with `int` sizes:**<br>
unsigned 2 byte int: `uint2`.<br>
unsigned 8 byte int: `uint8`.<br>
unsigned 16 byte int: `uint16`.<br>

### Text
**`byte`** - 1 byte.<br>
**`char`** - 1 unsigned byte, can hold only ASCII characters.<br>
**`str`** - Constant size string, alias for a fixed size `char` array.<br>
### Other
**`any`** - big enough to hold a pointer to any value. it's size depends on the largest type. works only for pointers, `var any a = 12` is invalid.<br>
**`struct`** - A Structure can hold any fixed size type inside. used to group variables that belong to the same thing together. Variables defined inside a `struct` don't need to be declared with the `var` keyword.<br>
**`enum`** - An Enumeration (enum) is a bunch of constants in a single place. they can only be numbers. the first number is 0 by default.
#### `struct` and `enum` example
```c
// a struct
struct name {
	int a;
	float b;
	str name;
};

// an enum
enum name {
	A, // 0
	B, // 1
	C=10, // 10
	D // 11
};

// accesing enum values
name::A; // 0
name::B; // 1
name::C; // 10
name::D; // 11
```
### Custom types
**You can create a custom type with the `deftype` (define type) keyword:**
```
deftype oldtype newtype;
```
**for example:**<br>
```rust
deftype usize unsigned_size;
```
**You can also use `structs` and `enums`:**
```rust
deftype enum {
	CAT,
	DOG
} AnimalType;
// AnimalType is now a type that can only be CAT or DOG (0 or 1 respectively).

deftype struct {
	String name;
	uint age;
	AnimalType type;
} Animal;
// 'Animal' is now a type.

// usage
var Animal dog;
dog.name.from("doggy");
dog.age = 2;
dog.type = AnimalType::DOG;
```

### Casting
Casting between types is done by enclosing the value you want to cast in parentheses prefixed by the type you want to cast to.
```golang
var int a=10;
var float b=float(a); // a is converted to a float, so it's now 10.0
// a more elegant way
var c=float(a); // no type name duplication
```
### Pointers and references
Pointers and references are supported.<br>
They work for basic types, functions and objects (classes).<br>
It's recommended to only use references unless you need pointer arithmetic.<br>
#### **Pointers:**
```golang
var a = 10;
var *ptr_to_a = &a;
// to access the value in 'a'
var b = *ptr_to_a; // 10

// pointer arithmetic
var array[] = {1, 2, 3, 4, 5};
var *ptr_array = array[0];
*ptr_array; // 1
*ptr_array+1; // 2
ptr_array += 1;
*ptr_array; // 2
// same for subtraction, multipliaction, and division.
```
Pointer arithmetic is safe because it has runtime checks to make sure that memory that shouldn't be accesed isn't accessed.<br>

#### **References:**
```golang
var a = 10;
var &ref_to_a = a;
// to access 'a'
var b = ref_to_a; // exactly the same as 'var b = a'
```

## Loops
There are two loop types: `while` and `for`.<br>
You can exit a loop with the `break` keyword, and jump to it's start with the `continue` keyword.
The output of all the following programs will be: `|1|2|3|4|5|`.<br>

### The `while` loop
The while loop keeps running until the condition provided is false.
```golang
// count to 10
var a = 1;
while a <= 10 {
	print("|{}|", i)
	a=a+1;
}
```
### The `for` loop
The `for` loop has two versions:<br>
**Counter:**<br>
```golang
// count to 10
for var i = 1; i <= 10; i++ {
	print("|{}|", i);
}
```
**Iterator:**<br>
```golang
var array = {1, 2, 3, 4, 5};
// iterate over every element in the array
for element in array {
	print("|{}|", element);
}
```

## Iterators
TODO

## `if`/`else`
The condition needs to evaluate to a `bool`.
```golang
var a = 10;
if a == 10 {
	print("'a' is 10\n");
} else {
	print("'a' isn't 10\n");
}

// multiple tests
if a == 10 {
	print("'a' is 10\n");
} else if a == 5 {
	print("'a' is 5\n");
} else {
	print("'a' isn't 10 or 5\n");
}
```

## `switch`
```golang
var a = 10;
switch a {
	10 => print("'a' is 10\n");
	5 => print("'a' is 5");
	// _ catches anything that isn't handled by the other cases.
	_ => print("'a' isn't 10 or 5\n");
}

// for declaring variables inside the switch or having more than one line, you can create a scope
switch a {
	10 => {
		print("'a' is 10\n");
		var b = 5;
		print("'b' is 5\n");
	}
	_ => print("'a' isn't 10\n");
}

// you can add labels to the cases and jump to them from a different case
a = 5;
switch a {
	10 : ten => print("'a' is 10\n");
	_ => {
		print("'a' isn't 10, jumping to 10 case...\n");
		break ten;
	}
}
// output of above switch will be:
//
// 'a' isn't 10, jumping to 10 case...
// 'a' is 10

// fallthrough
a = 10;
switch(a) {
	10 => fallthrough;
	5 => print("'a' is 5 or 10\n");
	_ => print("'a' isn't 5 or 10\n");
}
// output of above switch will be:
//
// 'a' is 5 or 10
```

## functions
```rust
fn name(<parameters>) -> <return_type> {
	<body>
}
```
**`parameters`** are declared as following: `type name, type2 name2`. if multiple parameters have the same type, you can declare them as following: `type name, name2`.<br>
**`body`** is the body of the function.<br>
**`return_type`** is the return type of the function. if not provided, the function doesn't return anything.
### example
```rust
fn add(int a, int b) -> int {
	return a+b;
}
```
### Variable argument functions and methods
Variable argument functions work by adding `name...type` (name can be any valid variable name, and type can be any valid type) as the last parameter in a function or method. `name` is a `Vector<type>` that contains the arguments. to get each argument, you can use the `Vector<T>` `peek_*` and `pop_*` methods to get the values.<br>
The last argument can be accessed using the `peek_front()` Vector method, and the first one using the `peek_back()` Vector method.

The type is optional, and can be removed to get variable type variable arguments. You can access them with the `*_as<TP>()` `Vector<T>` methods.
#### **Example**
```rust
fn variable_args(args...int) {
	var arg1 = args.peek_back();
	var last_arg = args.peek_front();

	// you can get a regular array of the arguments
	var args_as_array[] = args.to_array();
	// iterate over the array
	for arg in args_as_array {
		print("|{}|", arg);
	}
	// you can also do this
	for arg in args.to_array() {
		print("|{}|", arg);
	}
}

fn variable_type_args(args...) {
	var arg1 = args.peek_back_as<int>();
	var last_arg = args.peek_front_as<char>();
}

```


## Objects
TODO: decide if class based or binding methods to structs based.
```cpp
class name {
<access modifier>:
	variables, methods
};
```
### Members (variables in the object)
#### **Syntax**
Same as variables but without the `var` keyword:
```cpp
class Animal {
	int i;
	float f;
	String s
};
```
### Methods
#### **Syntax**
Same as functions but without the `fn` keyword:
```cpp
class Animal {
	print_sound() {
		// print the sound
	}
	get_name() -> str {
		// return the name
	}
};
```
#### **`this` and `super`**
* `this` is a reference to the current instance of the class, it has to be used to access anything inside the class from inside the class.<br>
* `super` is a reference to the super-class. when used like a function (`super()`) it calls the super-class constructor.

#### **Special methods**
* **constructors** - Have to have the same name as the class.<br>
called when a new instance is created.<br>
* **destructors** - Have to have the same name as the class prefixed with a `~`.<br>
called when an instance is destroyed (goes out of scope, it's memory freed etc.).

The constructor and destructor can be `private` and `public`.
#### **Example**
```cpp
class Animal {
public:
	// constructor
	Animal() {
		// do stuff to initialize
	}
	// destructor
	~Animal() {
		// do stuff to uninitialize
	}
};
```

### Access modifiers
- `public`: available to everyone.
- `private`: avilable only to the class.
The default is `private`.

**They can be used for a "section", or for single variables/methods:**
```java
class Animal {
// "section"
private:
	String name;
	String sound;
public:
	// single variable
	private int age;

	Animal(str name, str sound, int age) {
		this.name.from(name);
		this.sound.from(sound);
		this.age = age;
	}
};
```
### Inheritance
Inheritance is done by using `<`.
The class inheriting from is called the **super class** or the **parent class**, and the class inheriting from the superclass is called the **child class**.<br>
You can access all the methods and variables in the super class (including private ones) using the `super` keyword.
#### Example
```java
// the parent class
class Animal {
private:
	String name, sound;
public:
	Animal(str name, str sound) {
		this.name.from(name);
		this.sound.from(sound);
	}
	get_name() -> String {
		return this.name;
	}
	get_sound() -> String {
		return this.sound;
	}
};

// the child class
class Dog < Animal {
public:
	private String color;
	Dog(str name, str color) {
		// calling the constructor of the superclass
		super(name, "Woof!");
		this.color.from(color);
	}
	get_color() -> String {
		return this.color;
	}
	to_String() -> String {
		String s;
		s.from(super.name.to_str()); // can also use super.get_name().to_str()
		s.append(" is a dog of color ");
		s.append(this.color.to_str());
		s.append(" that makes the sound ");
		s.append(super.sound.to_str())
		// s is now "<name> is a dog of color <color> that makes the sound <sound>"
		// sound will always be "Woof!" in the Dog class
		// because that's what we passed to the superclass constructor in the Dog constructor

		return s;
	}
};
```
The child class `Dog` has access to everything in the super class `Animal` using the `super` keyword.

### Polymorphism
TODO

## Generics
Generics make writing one function for many different types possible, they work with functions, classes and methods.<br>
The compiler creates a version of the function for every provided type.
```rust
// declaring a function with generics
// multiple types can be declared:
// fn add<A, B, C>
fn add<T> (T a, T b) -> T {
	return a+b;
}

// calling the generic function
add<int>(1, 2); // 3
add<float>(1.5, 3.5); // 5
// the following function is generated by the compiler for the above call:
//
// fn add(float a, float b) -> float {
//     return a+b;
// }
```
You can make a generic type work only with specific types by putting the types in parentheses after the template type declaration:
```rust
// again, you can declare multiple types:
// fn add<N(int, float), S(str, String), U(uint, usize)>
fn add<T(int, float)> (T a, T b) -> T {
	return a+b;
}

add<int>(1, 4); // works.
add<char>('a', 'b'); // doesn't work, compilation error.
```
Generics in classes:
```cpp
// declaring a template class
class any_type<T> {
	T value;
public:
	any_number(T value) {
		this.value = value;
	}
	set(T value) {
		this.value = value;
	}
	get() -> T {
		return this.value;
	}
};

// creating instances of generic classes
any_type<int> integer(12);
any_type<uint> unsigned_int(3);
var int i = integer.get() // 12
var uint u = unsigned_int.get(); // 3
```

## Dynamic memory allocation
New objects can be created in the heap using the `new` keyword and freed using the `delete` keyword:
```cpp
// create a new String in the stack
var String *s = new String;
// you have to dereference 's' to use it, but you don't have to as the compiler does it for you
*s.from("Hello, World!");
// free it
delete s;
// s is still available for use again.

// You don't have to specify the type and make it a pointer as the compiler does it for you.
var str_in_heap = new String;
// The compiler dereferences for you.
str_in_heap.from("Hello, World!");
delete str_in_heap;

// For a class that has arguments in it's constructor
// (Object(int, int) is some class)
var obj = new Object(1, 2);
delete obj;
```
`new` returns a pointer to the memory address where the allocated memory starts.
Arrays in the heap can be created using `make<T>(usize size)`:
```golang
// make an int array of size 10
var array = make<int>(10);
```
A heap allocated object will be automatically freed when it goes out of scope and has no pointers or references.

## Standard library
### Available without importing
#### Functions
**`print(str fmt, args...)`** - Print `fmt` to standard out (`stdout`).<br>
* Suports formatting: `print("variable 'i' is: {}", i);`.
#### Smart types
**`String`** - A wrapper around a dynamic array of chars. this is what you have to use for mutable strings.<br>
**Methods for `String`:**<br>
* `from(str s)` - add the contents of `s` into the `String`.
* `to_str() -> str` - convert the contents of the `String` into a `str` and return it.
* `len() -> usize` return the length of the string in the `String`.
* `substr(usize start, end) -> String` - return a new `String` containing the contents between `start` and `end` in the original `String`.
* `append(str s)` - Append `s` to the `String`.
* `clone() -> String` - Create a copy of the `String`. equivalent of `String.substr(0, String.len())`.
**Example:**
```java
var String s;
s.from("Hello, World!");
s.len(); // 13
var hello = s.substr(0, 5); // Hello
hello.append(s.substr(8, 12).to_str()); // append 'World'
// hello is now 'Hello World'
var s2 = s.clone(); // s2 is 'Hello, World!'
```
**`Vector<T>`** - A dynamic array that can be any type.<br>
**Methods for `Vector<T>`:**<br>
* `push_front(T data)` - Put 'data' in the front of the array.
* `push_back(T data)` - Put 'data' at the back of the array.
* `push_to(uint index, T data)` - Put 'data' at 'index'.
* `pop_front() -> T` - Remove the front value and return it.
* `pop_front_as<TP>() -> TP` - Remove the front value and return it as `TP`.
* `pop_back() -> T` - Remove the back value and return it.
* `pop_back_as<TP>() -> TP` - Remove the back value and return it as `TP`.
* `pop_from(uint index) -> T` - Remove the value at 'index' and return it.
* `pop_from_as<TP>(uint index) -> TP`
* `peek_front() -> T` - Get the front value.
* `peek_front_as<TP>() -> TP` - Get the front value as `TP`.
* `peek_back() -> T` - Get the back value.
* `peek_back_as<TP>() -> TP` - Get the back value as `TP`.
* `peek_at(uint index) -> T` - Get the value at 'index'
* `peek_at_as<TP>(uint index) -> TP` - Get the value at 'index' as `TP`.
* `ensure(usize elements)` - Ensure that at least 'elements' space is available.
* `size() -> usize` - Get the size of the array.
* `is_empty() -> bool` - Check if the `Vector` is empty.
* `to_array() -> T` - Convert the array into a regular array of type 'T' and return it.
* `dump()` - Print the entire contents of the array.

**NOTE:** non `*_as<TP>()` methods call `*_as<TP>()` with `T` as the type. The data is stored as a `any` pointer to a heap allocated buffer, so it can be any type.

**Example:**
```java
Vector<int> v;
v.push_front(1);
v.dump(); // front [1] back

v.push_back(2);
v.dump(); // [1,2]

v.pop_back(); // 2
v.push_front(2);
v.dump(); // [2,1]

var int_array = v.to_array(); // [2,1]
```

### `stdio` (standard I/O)
Functions for printing text (to stdout, stderr and files), reading text (from stdin and files), file I/O.

