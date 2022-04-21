== DONE ==
variables (global)
==================
- declaration
- declaration + assignment
- assignment
- use (a+b works)

operators
=========
infix:
+ - * / %
| ^ & >> <<
== != > >= < <=
unary:
+ -

literals
========
numbers: decimal (XXX), hexadecimal (0xXXX), binary (0bXXX), octal (0oXXX)

== TODO ==
- validator
- logical operators: ||, && (or, and)
- parser: optimization: turn commutative (+ *) binary expressions
  from:
  2 + (3 + (4 + 5))
  to:
  ((2 + 3) + 4) + 5
- codegen: optimization: print the code to spill registers in allocate_register()
  that way a register won't be unspilled and then spilled right away.
