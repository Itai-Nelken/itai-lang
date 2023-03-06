# IR

```rust
var a = 42;
> IMM 42 <- temporarily store the immediate value 42.
> STORE_GLOBAL <a>, 0 <- store the temporarily stored value in the global `a` (0 is an offset used for structs).

fn do_nothing() {
    return;
}
> START_FUNCTION, <do_nothing>
> RETURN NONE
> END_FUNCTION

fn test() -> i32 {
    var l = a;
    return l;
}
> START_FUNCTION <test> <- start the function `test` (gives a reader all available info on the function).
> LOAD_GLOBAL <a>, 0
> STORE_LOCAL <l>, 0
> LOAD_LOCAL <l>, 0
> RETURN VAL <- set the return value & return.
> END_FUNCTION

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
> START_FUNCTION <add>
> LOAD_LOCAL <a>, 0 <- arguments are treated as locals.
> LOAD_LOCAL <b>, 0
> ADD <- stores the result as a temporary.
> RETURN
> END_FUNCTION

struct T {
    a: i32;
    b: i32;
}
fn sum_T(t: &T) -> i32 {
    return t.a + t.b;
}
> START_FUNCTION <sum_T>
> LOAD_LOCAL <t>, 0 <- load t.a
> LOAD_LOCAL <t>, 4 <- load t.b (`t` + 4 bytes).
> ADD
> RETURN
> END_FUNCTION
```

# Function related instructions #
START_FUNCTION <fn> - start function <fn>
END_FUNCTION - end a function
RETURN <VAL/NONE> - return from a function, if argument is VAL the temporary value is returned.
CALL <fn> - call a function, gets arguments from temporary stack.

# Variable related instructions #
STORE_GLOBAL <global>, <offset> - store the temporary value at <global> + <offset>.
LOAD_GLOBAL <global>, <offset> - load the global at <global> + <offset>.
STORE_LOCAL <local>, <offset> - store the temporary value at <local> + <offset>.
LOAD_LOCAL <local>, <offset> - load the local at <local> + <offset>.

# Operators #
ADD
SUBTRACT
MULTIPLY
DIVIDE
MODULO
...

DEREF - dereference the address in the temporary value.

# Other #
IMM <number literal>
