#include <stdio.h>
#include <string.h> // memset()
#include <stdarg.h>
#include <stdbool.h>
#include "common.h"
#include "Errors.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Token.h"
#include "ast.h"
#include "codegen.h"

static void free_all_registers(CodeGenerator *cg);

void initCodegen(CodeGenerator *cg, ASTProg *program, FILE *file) {
    cg->program = program;
    cg->out = file;
    cg->buffer = NULL;
    cg->buffer_size = 0;
    cg->buff = open_memstream(&cg->buffer, &cg->buffer_size);

    cg->print_stmt_used = false;
    cg->had_error = false;
    free_all_registers(cg);
    cg->spilled_regs = 0;
    initArray(&cg->globals);
}
void freeCodegen(CodeGenerator *cg) {
    // write the code to the output file
    // and free the buffer
    fclose(cg->buff);
    if(!cg->had_error) {
        fwrite(cg->buffer, sizeof(char), cg->buffer_size, cg->out);
    }
    free(cg->buffer);
    cg->buffer = NULL;
    cg->buffer_size = 0;

    for(size_t i = 0; i < cg->globals.used; ++i) {
        freeString(((ASTObj *)cg->globals.data[i])->name);
        FREE(cg->globals.data[i]);
    }
    freeArray(&cg->globals);
}

#define error(cg, location, format) do { printErrorF(ERR_ERROR, location, format); cg->had_error = true; } while(0)
#define errorf(cg, location, format, ...) do { printErrorF(ERR_ERROR, location, format, __VA_ARGS__); cg->had_error = true; } while(0)

static void println(CodeGenerator *cg, const char *format, ...) {
    fputs("\t", cg->buff);
    va_list ap;
    va_start(ap, format);
    vfprintf(cg->buff, format, ap);
    va_end(ap);
    fputs("\n", cg->buff);
}

static const char *registers[_REG_COUNT] = {"x0", "x1", "x2", "x3", "x4"};
static const char *reg_to_str(Register reg) {
    if(reg > _REG_COUNT) {
        UNREACHABLE();
    } else if(reg == NOREG) {
        return "NOREG";
    }
    return registers[reg];
}

static Register allocate_register(CodeGenerator *cg) {
    for(int i = 0; i < _REG_COUNT; ++i) {
        if(cg->free_regs[i]) {
            cg->free_regs[i] = false;
            return (Register)i;
        }
    }
    // no free registers left
    // so spill one into the stack
    Register reg = (Register)(cg->spilled_regs % (_REG_COUNT - 1));
    cg->spilled_regs++;
    println(cg, "// spilling register %s", reg_to_str(reg));
    println(cg, "str %s, [sp, -16]!", reg_to_str(reg));
    return reg;
}

static void free_register(CodeGenerator *cg, Register reg) {
    if(reg == NOREG) {
        return;
    }
    if(cg->spilled_regs > 0) {
        cg->spilled_regs--;
        Register reg = (cg->spilled_regs % (_REG_COUNT - 1));
        println(cg, "// unspilling register %s", reg_to_str(reg));
        println(cg, "ldr %s, [sp], 16", reg_to_str(reg));
    } else {
        if(cg->free_regs[reg]) {
            // double free
            UNREACHABLE();
        }
        cg->free_regs[reg] = true;
    }
}

static void free_all_registers(CodeGenerator *cg) {
    memset(cg->free_regs, true, _REG_COUNT-1);
}

static Register cgloadint(CodeGenerator *cg, i32 value) {
    Register r = allocate_register(cg);
    println(cg, "mov %s, %d", reg_to_str(r), value);
    return r;
}

static Register cgneg(CodeGenerator *cg, Register r) {
    println(cg, "neg %s, %s", reg_to_str(r), reg_to_str(r));
    return r;
}

static Register cgadd(CodeGenerator *cg, Register a, Register b) {
    println(cg, "add %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
    free_register(cg, b);
    return a;
}

static Register cgsub(CodeGenerator *cg, Register a, Register b) {
    println(cg, "sub %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
    free_register(cg, b);
    return a;
}

static Register cgmul(CodeGenerator *cg, Register a, Register b) {
    println(cg, "mul %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
    free_register(cg, b);
    return a;
}

static Register cgdiv(CodeGenerator *cg, Register a, Register b) {
    // 'udiv' for unsigned values
    println(cg, "sdiv %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
    free_register(cg, b);
    return a;
}

static Register cgmodulo(CodeGenerator *cg, Register a, Register b) {
    // tmp = a / b
    // a = tmp * b
    // result = a - b
    Register tmp = allocate_register(cg);
    println(cg, "sdiv %s, %s, %s", reg_to_str(tmp), reg_to_str(a), reg_to_str(b));
    println(cg, "mul %s, %s, %s", reg_to_str(b), reg_to_str(tmp), reg_to_str(b));
    println(cg, "sub %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
    free_register(cg, tmp);
    free_register(cg, b);
    return a;
}

static Register cgcompare(CodeGenerator *cg, Register a, Register b, ASTNodeType op) {
    println(cg, "cmp %s, %s", reg_to_str(a), reg_to_str(b));
    switch(op) {
        case ND_EQ:
            println(cg, "cset %s, eq", reg_to_str(a));
            break;
        case ND_NE:
            println(cg, "cset %s, eq", reg_to_str(a));
            break;
        case ND_GT:
            println(cg, "cset %s, eq", reg_to_str(a));
            break;
        case ND_GE:
            println(cg, "cset %s, eq", reg_to_str(a));
            break;
        case ND_LT:
            println(cg, "cset %s, eq", reg_to_str(a));
            break;
        case ND_LE:
            println(cg, "cset %s, eq", reg_to_str(a));
            break;
        default:
            UNREACHABLE();
    }
    free_register(cg, b);
    return a;
}

static Register cgbitop(CodeGenerator *cg, Register a, Register b, ASTNodeType op) {
    switch(op) {
        case ND_BIT_OR:
            println(cg, "orr %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
            break;
        case ND_XOR:
            println(cg, "eor %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
            break;
        case ND_BIT_AND:
            println(cg, "and %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
            break;
        case ND_BIT_RSHIFT:
            println(cg, "lsr %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
            break;
        case ND_BIT_LSHIFT:
            println(cg, "lsl %s, %s, %s", reg_to_str(a), reg_to_str(a), reg_to_str(b));
            break;
        default:
            UNREACHABLE();
    }
    free_register(cg, b);
    return a;
}

static void addGlobal(CodeGenerator *cg, char *name) {
    ASTObj *g = CALLOC(1, sizeof(*g));
    g->name = stringCopy(name);
    arrayPush(&cg->globals, g);
}

static ASTObj *getGlobal(CodeGenerator *cg, char *name) {
    for(size_t i = 0; i < cg->globals.used; ++i) {
        if(!strcmp(ARRAY_GET_AS(ASTObj *, &cg->globals, i)->name, name)) {
            return ARRAY_GET_AS(ASTObj *, &cg->globals, i);
        }
    }
    return NULL;
}

static Register load_glob_addr(CodeGenerator *cg, char *name, Location loc) {
    ASTObj *var = getGlobal(cg, name);
    if(var == NULL) {
        error(cg, loc, "Undeclared variable");
        return NOREG;
    }
    Register reg = allocate_register(cg);
    const char *r = reg_to_str(reg);
    println(cg, "adrp %s, %s", r, var->name);
    println(cg, "add %s, %s, :lo12:%s", r, r, var->name);
    return reg;
}

static Register load_glob(CodeGenerator *cg, char *name, Location loc) {
    Register address = load_glob_addr(cg, name, loc);
    if(address == NOREG) {
        return NOREG;
    }
    Register r = allocate_register(cg);
    println(cg, "ldr %s, [%s]", reg_to_str(r), reg_to_str(address));
    free_register(cg, address);
    return r;
}

// returns the register that will contain the result
static Register gen_expr(CodeGenerator *cg, ASTNode *node) {
    switch(node->type) {
        case ND_NUM:
            return cgloadint(cg, node->as.literal.int32);
        case ND_VAR:
            return load_glob(cg, node->as.var.name, node->loc);
        case ND_ASSIGN: {
            Register addr = load_glob_addr(cg, node->left->as.var.name, node->loc);
            if(addr == NOREG) {
                return NOREG;
            }
            Register rvalue = gen_expr(cg, node->right);
            println(cg, "str %s, [%s]", reg_to_str(rvalue), reg_to_str(addr));
            free_register(cg, addr);
            return rvalue; // assignment returns thr value assigned
        }
        case ND_NEG:
            return cgneg(cg, gen_expr(cg, node->left));
        default:
            break;
    }
    
    Register left = gen_expr(cg, node->left);
    Register right = gen_expr(cg, node->right);
    switch (node->type) {
        case ND_ADD:
            return cgadd(cg, left, right);
        case ND_SUB:
            return cgsub(cg, left, right);
        case ND_MUL:
            return cgmul(cg, left, right);
        case ND_DIV:
            return cgdiv(cg, left, right);
        case ND_REM:
            return cgmodulo(cg, left, right);
        case ND_EQ:
        case ND_NE:
        case ND_GT:
        case ND_GE:
        case ND_LT:
        case ND_LE:
            return cgcompare(cg, left, right, node->type);
        case ND_BIT_OR:
        case ND_XOR:
        case ND_BIT_AND:
        case ND_BIT_RSHIFT:
        case ND_BIT_LSHIFT:
            return cgbitop(cg, left, right, node->type);
        default:
            UNREACHABLE();
    }
}

static void gen_stmt(CodeGenerator *cg, ASTNode *node) {
    switch(node->type) {
        case ND_PRINT: {
            cg->print_stmt_used = true;
            Register val = gen_expr(cg, node->left);
            if(val == NOREG) {
                return;
            }
            // could use 'stp' to save a few instructions and bytes
            println(cg, "// spilling all registers");
            for(int i = 0; i < _REG_COUNT; ++i) {
                println(cg, "str %s, [sp, -16]!", reg_to_str(i));
            }
            println(cg, "mov x1, %s", reg_to_str(val));
            println(cg, "ldr x0, =printf_int_str");
            println(cg, "bl printf");
            println(cg, "// unspilling all registers");
            for(int i = _REG_COUNT-1; i >= 0; --i) {
                println(cg, "ldr %s, [sp], 16", reg_to_str(i));
            }
            free_register(cg, val);
            break;
        }
        case ND_EXPR_STMT:
            free_register(cg, gen_expr(cg, node->left));
            break;
        case ND_ASSIGN:
            addGlobal(cg, node->left->as.var.name);
            free_register(cg, gen_expr(cg, node));
            break;
        case ND_VAR:
            addGlobal(cg, node->as.var.name);
            break;
        default:
            UNREACHABLE();
    }
    return;
}

static void emit_data(CodeGenerator *cg) {
    println(cg, ".data");
    // temporary data for builtin print statement
    if(cg->print_stmt_used) {
        println(cg, ".local printf_int_str\n"
                    "printf_int_str:\n"
                    "\t.string \"%%d\\n\"\n");
    }
    for(size_t i = 0; i < cg->globals.used; ++i) {
        ASTObj *g = ARRAY_GET_AS(ASTObj *, &cg->globals, i);
        println(cg, ".global %s\n"
                    "%s:", g->name, g->name);
        println(cg, ".zero 8");
    }
}

void codegen(CodeGenerator *cg) {
    println(cg, ".text\n"
                "\t.global main\n"
                "main:\n");
    println(cg, "stp fp, lr, [sp, -16]!");

    for(int i = 0; i < (int)cg->program->statements.used; ++i) {
        gen_stmt(cg, ARRAY_GET_AS(ASTNode *, &cg->program->statements, i));
        if(cg->had_error) {
            return;
        }
    }
    // return
    println(cg, "ldp fp, lr, [sp], 16"); 
    println(cg, "mov x0, 0");
    println(cg, "ret\n");

    emit_data(cg);
}
