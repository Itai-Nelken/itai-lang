# IR

## Data structures
* Temporary values are loaded into a stack (for example `IMM`).
* Globals are stored in an indexed collection (An array for example).
* Functions are stored in the bytecode array. The index of a function is the index of its first instruction (`ENT`).

## Function definition
To begin a function, `ENT` must be executed with the number of bytes to reserve for locals provided.
To leave a function, `LEV` or `RET` must be executed (they rewind the stack to the way it was before `ENT` was executed. `RET` also saves the current stack top as the return value.



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
> ADD // [..., <a> + 1]
> ST 0 // [...]
> LEV // []

fn main() -> i32 { // idx: 10
    do_stuff();
    a = test();
    return a;
}

> ENT 4 // [<return address>]
> CALL 5 // [...]
> CALL 2 // [..., 42]
> ST 0 // [...]
> LD 0 /// [..., 42]
> LEV // [42]
```
