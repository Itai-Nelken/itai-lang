#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "Vec.h"
#include "OpCode.h"


/* Program - the result outputed by the bytecode compiler (for a module or a whole program?) */

typedef struct program {
    Vec(uint8_t) bytecode;
} Program;

void programInit(Program *prog) {
    prog->bytecode = VEC_NEW(uint8_t);
}

void programFree(Program *prog) {
    VEC_FREE(prog->bytecode);
}

void programAppendByte(Program *prog, uint8_t byte) {
    VEC_PUSH(prog->bytecode, byte);
}

void programAppendBytes(Program *prog, uint8_t byte1, uint8_t byte2) {
    programAppendByte(prog, byte1);
    programAppendByte(prog, byte2);
}

void disassemble(Program *prog) {
    VEC_FOREACH(i, prog->bytecode) {
        switch(prog->bytecode[i]) {
            case OP_START_FUNCTION: printf("OP_START_FUNCTION %u", prog->bytecode[++i]); break;
            case OP_END_FUNCTION: printf("OP_END_FUNCTION"); break;
            case OP_RETURN: {
                uint8_t arg = prog->bytecode[++i];
                printf("OP_RETURN ");
                switch(arg) {
                    case ARG_VALUE: printf("ARG_VALUE"); break;
                    case ARG_NONE: printf("ARG_NONE"); break;
                    default:
                        assert(0 && "Invalid argument");
                }
                break;
            }
            case OP_CALL: printf("OP_CALL %u", prog->bytecode[++i]); break;
            case OP_STORE_GLOBAL: {
                uint8_t global = prog->bytecode[++i];
                uint8_t offset = prog->bytecode[++i];
                printf("OP_STORE_GLOBAL %u, %u", global, offset);
                break;
            }
            case OP_LOAD_GLOBAL: {
                uint8_t global = prog->bytecode[++i];
                uint8_t offset = prog->bytecode[++i];
                printf("OP_LOAD_GLOBAL %u, %u", global, offset);
                break;
            }
            case OP_STORE_LOCAL: {
                uint8_t local = prog->bytecode[++i];
                uint8_t offset = prog->bytecode[++i];
                printf("OP_STORE_LOCAL %u, %u", local, offset);
                break;
            }
            case OP_LOAD_LOCAL: {
                uint8_t local = prog->bytecode[++i];
                uint8_t offset = prog->bytecode[++i];
                printf("OP_LOAD_LOCAL %u, %u", local, offset);
                break;
            }
            case OP_ADD: printf("OP_ADD"); break;
            case OP_IMM: printf("OP_IMM %u", prog->bytecode[++i]); break;
            default:
                assert(0 && "Invalid opcode");
        }
        putchar('\n');
    }
}

/* Vm */

// TODO: support other types (i32, str, ptrs etc.).
typedef struct value {
    uint32_t v;
} Value;

typedef struct variable {
    //ASTObj *obj;
    Value value;
} Variable;

typedef struct vm {
    Vec(uint32_t) stack; // TODO: change to Vec(Value)?
    Vec(Variable) globals;
} Vm;

void vm_init(Vm *vm) {
    vm->stack = VEC_NEW(uint32_t);
    vm->globals = VEC_NEW(Variable);
}

void vm_free(Vm *vm) {
    VEC_FREE(vm->stack);
    VEC_FREE(vm->globals);
}

void vm_execute(Vm *vm, Program *prog) {
    VEC_FOREACH(i, prog->bytecode) {
#define NEXT() (prog->bytecode[++i])
        switch(prog->bytecode[i]) {
            case OP_START_FUNCTION: { // arg1: <function>
                uint8_t function_id = NEXT();
                VEC_PUSH(vm->stack, function_id);
                // Function *fn = get_function(function_id);
                // stack.push_n(fn.locals.length());
                break;
            }
            case OP_END_FUNCTION: // no args
                // Function *fn = get_function(current_function_id);
                // stack.pop_n(fn.locals.length());

                // cast to void because result is unused.
                (void)VEC_POP(vm->stack);
                break;
            case OP_RETURN: { // arg1: OpArg, pops 1 temp
                switch((OpArg)NEXT()) {
                    case ARG_VALUE: {
                        __attribute__((unused)) uint32_t return_value = VEC_POP(vm->stack);
                        // TODO: return value.
                        break;
                    }
                    case ARG_NONE:
                        break;
                    default:
                        assert(0 && "Invalid argument to return");
                }
                // Function *fn = get_function(current_function_id);
                // goto fn.end_function_instruction;
                break;
            }
            case OP_CALL: {
                __attribute__((unused)) uint8_t callee_function_id = NEXT();
                // Function *fn = get_function(callee_function_id);
                // args are in stack.
                // goto fn;
                break;
            }
            case OP_STORE_GLOBAL: { // arg1: <global>, arg2: <offset>, pops 1 temp
                uint8_t global_id = NEXT();
                __attribute__((unused))  uint8_t offset = NEXT(); // for structs and arrays
                assert(global_id < VEC_LENGTH(vm->globals));
                vm->globals[global_id].value.v = VEC_POP(vm->stack);
                break;
            }
            case OP_LOAD_GLOBAL: { // arg1: <global>, arg2: <offset>, pushes 1 temp
                uint8_t global_id = NEXT();
                __attribute__((unused)) uint8_t offset = NEXT(); // for structs and arrays.
                assert(global_id < VEC_LENGTH(vm->globals));
                VEC_PUSH(vm->stack, vm->globals[global_id].value.v);
                break;
            }
            case OP_STORE_LOCAL: // arg1: <local>, arg2: <offset>, pops 1 temp
            case OP_LOAD_LOCAL: // arg1: <local>, arg2: <offset>, pushes 1 temp
                assert(0 && "Locals are not implemented yet");
            case OP_ADD: // no args, pops 2 temp pushes 1 temp
                VEC_PUSH(vm->stack, VEC_POP(vm->stack) + VEC_POP(vm->stack));
                break;
            case OP_IMM:
                VEC_PUSH(vm->stack, NEXT());
                break;
            default:
                assert(0 && "Invalid opcode");
        }
#undef NEXT
    }
}

// TODO: add objects (ASTObj from compiler) for vars, fns, structs etc.
int main(void) {
    Program prog;
    programInit(&prog);
    programAppendBytes(&prog, OP_START_FUNCTION, 123);
    programAppendBytes(&prog, OP_IMM, 42);
    programAppendBytes(&prog, OP_RETURN, ARG_VALUE);
    programAppendByte(&prog, OP_END_FUNCTION);
    disassemble(&prog);
    Vm vm;
    vm_init(&vm);
    vm_execute(&vm, &prog);
    vm_free(&vm);
    programFree(&prog);
    return 0;
}
