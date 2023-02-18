#ifndef VM_H
#define VM_H

#include <stdbool.h>
#include "OpCode.h"

int execute(OpCode program[], int length, int entry_point, bool debug_dump);

#endif // VM_H
