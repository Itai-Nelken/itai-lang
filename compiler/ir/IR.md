# IR

## Data structures
* Temporary values along with stack (call) frames are loaded into a stack.
* Global data (global variables, strings etc.) is stored in an indexed collection (An array for example).
* Functions are stored in the bytecode array. The index of a function is the index of its first instruction (`ENT`).

## Registers
There are 3 registers:
- `sp`  - The stack pointer, not accesible.
- `bp`  - The base pointer, not accesible.
- `r` - The general purpose register, read/write.

## Function definition
To begin a function, `ENT` must be executed with the number of bytes to reserve for locals provided.\
To leave a function, `LEV` must be executed.\
Return values are saved in the `r` register using the `SR` instruction. To load the returned value the `LR` instruction is used.

### Function arguments
The arguments to a function have to be pushed to the stack before calling the function.\
After the function returns, the arguments must be removed. That is achieved using the `ADJ <num params to pop>` instruction.

## Instructions
|  Name  |     Usage      |  Notes  |
| :----: |     :---:      |  :---:  |
| `IMM`  |  `imm <num>`   | The argument is 12 bits. only positive numbers are supproted. |
|  `ST`  |   `st <idx>`   | `<idx>` is an index into the data section. |
|  `LD`  |   `ld <idx>`   | `<idx>` is an index into the data section. |
| `ARG`  |  `arg <n>`     | `<n>` is the argument to get (0 -> last arg, 1 -> one before last arg etc.). |
| `ADJ`  |  `adj <count>` | `<count>` is the number of arguments to remove from the stack. |
| `ADD`  |  `add`         | The 2 operands are poped from the stack. |
| `ENT`  | `ent <locals>` | `<locals>` is the number of locals so space can be reserved in the call frame. |
| `LEV`  |  `lev`         | - |
|  `SR`  |   `sr`         | - |
|  `LR`  |   `lr`         | - |
| `CALL` |  `call <idx>`  | `<idx>` is the index of the `ent` instruction of the function to call. |

```rust
var a = 41;

> IMM 41 // [41]
> ST 0 // []

fn test() -> i32 { // idx: 2
    return 42;
}

> ENT 0 // [<return address>]
> IMM 42 // [..., 42]
> SR // [...]
> LEV // []

fn do_stuff() { // idx: 6
    a = a + 1;
}

> ENT 0 // [<return address>]
> LD 0 // [..., <a>]
> ADD // [..., <a> + 1]
> ST 0 // [...]
> LEV // []

fn main() -> i32 { // idx: 11
    do_stuff();
    a = test();
    return a;
}

> ENT 4 // [<return address>]
> CALL 6 // [...]
> CALL 2 // [..., 42]
> ST 0 // [...]
> LD 0 // [..., 42]
> SR // [...]
> LEV // []
```
