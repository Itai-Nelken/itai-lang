# Itai's programming language

## basic syntax
statements are followed by a semicolon (;).<br>
### Literals
**Integers:** `<number>` for example: `12`.<br>
**Floats:** `<integer>.<integer>` for example: `3.14`. if no decimal point is provided, `.0` is added. so `12` becomes `12.0`.<br>
**characters:** `'<character>'` for example: `'A'`. only ASCII characters are supported.<br>
**strings:** `"<string>"` for example: `"Hello, World!"`. only ASCII characters are supported.<br>

## Operators
**Math**: addition: `+`, substraction: `-`, multiplication: `*`, division: `/`, modulo (remainder): `%`.<br>
**Bitwise:** and: `&`, or: `|`, not: `!`, xor: `^`.<br>
**Asignment:** `=`.<br>
**Comparison:** `==`, `!=`, `<`, `>`, `<=`, `>=`.<br>
**Control flow:** and: `&&`, or: `||`<br>

## Comments
* Start with `//` until end of line.
* Between `/*` and `*/`.

## Importing libraries
```py
import library
from library import thing
```

## Variables
```py
type name;
type name = value;
```
**The type can be detected by the compiler automatically:**
```go
var name = value;
```
### Constants
```go
const type name = value;
```
**The type doesn't have to be specified:**
```go
const name = value;
```
### Arrays
#### **Initializing:**<br>
If all the array is filled in declaration, there is no need to specify the size.
```go
type name[size];
type name[size] = {elements};
var name[size] = {elements};
```
#### **accessing elements:**<br>
```go
var array[] = {1, 2, 3, 4, 5};
// accesing elements
array[0]; // 1
array[1]; // 2

// writing to elements
array[0]=0;
array[0]; // 0
```

## Types
### Numbers
**`int`** - 4 byte integer.<br>
**`float`** - 32 bit floating point number.<br>
**`size`** - 32 or 64 bit depending on the architecture.<br>

#### **Larger sizes are available by appending the size to the end:**
**`int` sizes:**<br>
**`2`, `4`, `8`, `16`** (bytes).<br>
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
**`char`** - 1 byte, can hold only ASCII characters.<br>
**`str`** - Constant string, alias for a fixed size constant `char` array.<br>
### Other
**`struct`** - A Structure can hold any fixed size type inside. used to group variables that belong to the same thing together.<br>
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
	C=10 // 10
};
```
### Custom types
**You can create a custom type with the `deftype` keyword:**
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
Animal dog;
dog.name="doggy";
dog.age=2;
dog.type=DOG;
```

### Casting
Casting between types is done by enclosing the value you want to cast in parentheses prefixed by the type you want to cast to.
```go
int a=10;
float b=float(a); // a is converted to a float, so it's now 10.0
// a more elegant way
var c=float(a); // no type duplication
```
### Pointers and references
Pointers and references are supported.<br>
They work for basic types, functions and objects (classes).
#### **Pointers:**
```go
var a = 10;
var *ptr_to_a = &a;
// to access the value in a
var b = *ptr_to_a; // 10
```
#### **References:**
```go
var a = 10;
var &ref_to_a = a;
// to access 'a'
var b = ref_to_a; // exactly the same as 'var b = a'
```

## Loops
There are two loop types: `while` and `for`.<br>
The output of all the following programs will be: `|1|2|3|4|5|`.<br>
### The `while` loop
The while loop keeps running until the condition provided is false.
```go
// count to 10
var a = 1;
while(a <= 10) {
	print("|{}|", i)
	a=a+1;
}
```
### The `for` loop
The `for` loop has two versions:
**Counter:**<br>
```go
// count to 10
for var i=1; i<=10; i=i+1 {
	print("|{}|", i);
}
```
**Iterator:**<br>
```go
var array = {1, 2, 3, 4, 5};
// iterate over every element in the array
for element in array {
	print("|{}|", element);
}
```

## functions
```rust
fn name(type parameter) return_type {
	body
}
```
**`body`** is the body of the function.<br>
**`return_type`** is the return type of the function. if not provided, the function doesn't return anything.
### example
```rust
fn add(int a, int b) int {
	return a+b;
}
```

## Objects
```cpp
class name {
<access modifier>:
	variables, methods
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
	get_name() str {
		// return the name
	}
};
```
#### **`this` and `super`**
* `this` is the current instance of the class, it has to be used to access anything inside the class from inside the class.<br>
* `super` is used to access things in the superclass. when used like a function (`super()`) it calls the superclass constructor.

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
		this.age=age;
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
	get_name() String {
		return this.name; // calling this method makes a copy of this.name for the return address. like calling String.clone()
	}
	get_sound() String {
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
	get_color() String {
		return this.color;
	}
	to_String() String {
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

## Templates
Templates make writing one function for many different types possible. templates work with functions, classes and methods.<br>
The compiler creates a version of the function for every provided type.
```rust
// declaring a template function
// multiple types can be declared:
// fn add<type A, type B, type C>
fn add<type T> (T a, T b) T {
	return a+b;
}

// calling a template function
add<int>(1, 2); // 3
add<float>(1.5, 3.5); // 5
// the following function is generated by the compiler for the above call:
//
// fn add(float a, float b) float {
//     return a+b;
// }
```
You can make a template type only work with specific types by putting the types in parentheses after the template type declaration:
```rust
// again, you can declare multiple types:
// template<type N(int, float), type S(str, String), type U(uint, usize)>
fn add<type T(int, float)> (T a, T b) T {
	return a+b;
}

add<int>(1, 4); // works.
add<char>('a', 'b'); // doesn't work, compilation error.
```
Templates in a class:
```cpp
// declaring a template class
class any_type<type T> {
	T value;
public:
	any_number(T value) {
		this.value=value;
	}
	set(T value) {
		this.value=value;
	}
	get() T {
		return this.value;
	}
};

// creating instances of a template class
any_type<int> integer(12);
any_type<uint> unsigned_int(3);
int i = integer.get() // 12
uint u = unsigned_int.get(); // 3
```

## Dynamic memory allocation
New objects can be created in the heap using the `new` keyword and freed using the `delete` keyword:
```go
// create a new String in the stack
String *s = new String;
// you have to dereference 's' to use it.
*s.from("Hello, World!");
// free it
delete s;

// You can use 'var' to create the variable and then you don't have to make it a pointer
// because the compiler handles it for you
var str_in_heap = new String;
// because 'var' was used, the compiler dereferences for you.
str_in_heap.from("Hello, World!");
delete str_in_heap;
```
`new` returns a pointer to the memory address where the allocated memory starts.
Arrays can be created using `make<type T>(usize size)`:
```go
// make an int array of size 10
var array = make<int>(10);
```
A heap allocated object will be automatically freed when it goes out of scope and has no pointers or references.

## Standard library
### Available without importing
#### Smart types
**`String`** - A wrapper around a dynamic array of chars. this is what you have to use for mutable strings.<br>
**Methods for `String`:**<br>
* `from(str s)` - add the contents of `s` into the `String`.
* `to_str() str` - convert the contents of the `String` into a `str` and return it.
* `len() usize` return the length of the string in the `String`.
* `substr(usize start, usize end) String` - return a new `String` containing the contents between `start` and `end` in the original `String`.
* `append(str s)` - Append `s` to the `String`.
* `clone() String` - Create a copy of the `String`. equivalent of `String.substr(0, String.len())`.
**Example:**
```java
String s;
s.from("Hello, World!");
s.len(); // 13
var hello = s.substr(0, 5); // Hello
hello.append(s.substr(8, 12).to_str()); // append 'World'
// hello is now 'Hello World'
var s2 = s.clone(); // s2 is 'Hello, World!'
```
**`Vec<type T>`** - A dynamic array that can be any type.<br>
**Methods for `Vec<type T>`:**<br>
* `push_front(T data)` - Put 'data' in the front of the array.
* `push_back(T data)` - Put 'data' at the back of the array.
* `push_to(uint index, T data)` - Put 'data' at 'index'.
* `pop_front() T` - Remove the front value.
* `pop_back() T` - Remove the back value.
* `pop_from(uint index) T` - Get the value at 'inedx' and return it.
* `size() usize` - Get the size of the array.
* `to_array() T` - Convert the array into a regular array of type 'T' and return it.
* `dump()` - Print the entire contents of the array.
**Example:**
```java
Vec<int> v;
v.push_front(1);
v.dump(); // front [1] back

v.push_back(2);
v.dump(); // [1,2]

v.pop_back(); // 2
v.push_front(2);
v.dump(); // [2,1]

var int_array = v.to_array(); // [2,1]
```
