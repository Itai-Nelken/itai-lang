# IR

## Data structures
* Temporary values are loaded into a stack (for example `IMM`).
* Globals are stored in an indexed collection (An array for example).
* Functions are stored in the bytecode array, and are indexed by theur first instruction (`ENT`).

## Function definition
To begin a function, `ENT` must be executed with the number of bytes to reserve for locals and parameters provided.
Locals and arguments can be accesed with `LDL`/`STL`, Locals with positive indices, and parameters with negative indices (`0` is the first local, and `-1` is the first argument).
To leave a function, `LEV` or `RET` must be executed (they pop the amount of bytes provided to `ENT` and `RET` saves the current stack top as the return value).

## Function arguments
* Arguments are passed by pushing them on to the stack before calling the function.
* The caller must pop the arguments when the calle returns.

```rust
var a = 41;

> IMM 41 // [41]
> ST 0 // []

fn test() -> i32 { // idx: 2
    return 42;
}

> ENT 0 // [<return address>]
> IMM 42 // [..., 42]
> LEV // [42]

fn do_stuff() { // idx: 5
    a = a + 1;
}

> ENT 0 // [<return address>]
> LD 0 // [..., <a>]
> IMM 1 // [..., <a>, 1]
> ADD // [..., <a> + 1]
> LEV // [<a> + 1]

fn add(value: i32) { // idx: 10
    a += value;
}

> ENT 0 // [<value>, <return address>]
> LDL -1 // [..., <value>]
> LD 0 // [..., <value>, <a>]
> ADD // [..., <value> + <a>]
> ST 0 // [...]
> LEV // []

fn main() -> i32 { // idx: 16
    add(-1);
    do_stuff();
    var b: i32 = test();
    return a - b;
}

> ENT 4 // [<return address>]
> IMM -1 // [..., -1]
> CALL 10 // [..., -1]
> POP // [...]
> CALL 5 // [...]
> CALL 2 // [..., 42]
> STL 0 // [...]
> LD 0 // [..., 42]
> LDL 0 // [..., 42, 42]
> SUB // [..., 0]
> LEV // [0]
```
