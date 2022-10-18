# IR

## Data structures
* Temporary values are loaded into a stack (for example `IMM`).
* Globals are stored in an indexed collection (An array for example).
* Functions are stored in the bytecode array. The index of a function is the index of its first instruction (`ENT`).

## Registers
There are 3 registers:
- `sp`  - The stack pointer, not accesible.
- `bp`  - The base pointer, not accesible.
- `reg` - The general purpose register, read-only.

## Function definition
To begin a function, `ENT` must be executed with the number of bytes to reserve for locals provided.\
To leave a function, `LEV` or `RET` must be executed (they rewind the stack to the way it was before `ENT` was executed. `RET` also saves the current stack top in `reg`.

### Function arguments
The arguments to a function have to be pushed to the stack before calling the function.\
After the function returns, the arguments must be removed. That is achieved using the `ADJ <num params to pop>` instruction.

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
