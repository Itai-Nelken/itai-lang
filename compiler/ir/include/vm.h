#ifndef VM_H
#define VM_H

#include "OpCode.h"

int execute(OpCode program[], int length, int entry_point);

#endif // VM_H
