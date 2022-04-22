#include <stdio.h>
#include <string.h> // memset()
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include "common.h"
#include "utilities.h"
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

    cg->current_fn = NULL;
    cg->call_init_globals = false;
    cg->print_stmt_used = false;
    cg->had_error = false;
    free_all_registers(cg);
    cg->spilled_regs = 0;
    cg->counter = 0;
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
}

#define error(cg, location, format) do { printError(ERR_ERROR, location, format); cg->had_error = true; } while(0)
#define errorf(cg, location, format, ...) do { printErrorF(ERR_ERROR, location, format, __VA_ARGS__); cg->had_error = true; } while(0)

static void print(CodeGenerator *cg, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(cg->buff, format, ap);
    va_end(ap);
}

static void println(CodeGenerator *cg, const char *format, ...) {
    fputs("\t", cg->buff);
    va_list ap;
    va_start(ap, format);
    vfprintf(cg->buff, format, ap);
    va_end(ap);
    fputs("\n", cg->buff);
}

static const char *return_value_reg = "x0";
static const char *registers[_REG_COUNT] = {
    [R0]     = "x1",
    [R1]     = "x2",
    [R2]     = "x3",
    [R3]     = "x4",
    [R4]     = "x5",
};
static const char *reg_to_str(Register reg) {
    if(reg == NOREG) {
        return "NOREG";
    } else if(reg == RETREG) {
        return return_value_reg;
    } else if(reg > _REG_COUNT) {
        UNREACHABLE();
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
    if(reg == NOREG || reg == RETREG) {
        return;
    }
    if(cg->spilled_regs > 0) {
        cg->spilled_regs--;
        Register reg = (cg->spilled_regs % (_REG_COUNT - 1));
        println(cg, "// unspilling register %s", reg_to_str(reg));
        println(cg, "ldr %s, [sp], 16", reg_to_str(reg));
    } else {
        if(cg->free_regs[reg]) {
            LOG_ERR_F("Double free: register %s (%d)\n", reg_to_str(reg), (int)reg);
            UNREACHABLE();
        }
        cg->free_regs[reg] = true;
    }
}

static void free_all_registers(CodeGenerator *cg) {
    memset(cg->free_regs, true, _REG_COUNT-1);
}

static int count(CodeGenerator *cg) {
    return cg->counter++;
}

static Register cgloadint(CodeGenerator *cg, i32 value) {
    // TODO: handle large literals (larger than 12 bits?)
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
            println(cg, "cset %s, ne", reg_to_str(a));
            break;
        case ND_GT:
            println(cg, "cset %s, gt", reg_to_str(a));
            break;
        case ND_GE:
            println(cg, "cset %s, ge", reg_to_str(a));
            break;
        case ND_LT:
            println(cg, "cset %s, lt", reg_to_str(a));
            break;
        case ND_LE:
            println(cg, "cset %s, le", reg_to_str(a));
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

static ASTObj *get_global(CodeGenerator *cg, char *name) {
    for(size_t i = 0; i < cg->program->globals.used; ++i) {
        // because globals are wrapped in ND_EXPR_STMTs
        ASTNode *global = ARRAY_GET_AS(ASTNode *, &cg->program->globals, i)->left;
        ASTObj *g = global->type == ND_ASSIGN ? &global->left->as.var : &global->as.var;
        if(stringEqual(g->name, name)) {
            return g;
        }
    }
    return NULL;
}

static Register load_global_addr(CodeGenerator *cg, char *name, Location loc) {
    ASTObj *var = get_global(cg, name);
    if(var == NULL) {
        errorf(cg, loc, "Undeclared variable '%s'", name);
        return NOREG;
    }
    Register reg = allocate_register(cg);
    const char *r = reg_to_str(reg);
    println(cg, "adrp %s, %s", r, var->name);
    println(cg, "add %s, %s, :lo12:%s", r, r, var->name);
    return reg;
}

static Register load_global(CodeGenerator *cg, char *name, Location loc) {
    Register address = load_global_addr(cg, name, loc);
    if(address == NOREG) {
        return NOREG;
    }
    Register r = allocate_register(cg);
    println(cg, "ldr %s, [%s]", reg_to_str(r), reg_to_str(address));
    free_register(cg, address);
    return r;
}

static ASTObj *get_local(CodeGenerator *cg, char *name) {
    if(cg->current_fn == NULL) {
        return NULL;
    }
    for(size_t i = 0; i < cg->current_fn->locals.used; ++i) {
        ASTObj *local = ARRAY_GET_AS(ASTObj *, &cg->current_fn->locals, i);
        if(stringEqual(local->name, name)) {
            return local;
        }
    }
    return NULL;
}

static Register load_local(CodeGenerator *cg, char *name) {
    ASTObj *local = get_local(cg, name);
    if(local == NULL) {
        return NOREG;
    }

    Register reg = allocate_register(cg);
    println(cg, "ldr %s, [fp, %d]", reg_to_str(reg), local->offset);
    return reg;
}

// returns the register that will contain the result
static Register gen_expr(CodeGenerator *cg, ASTNode *node) {
    switch(node->type) {
        case ND_NUM:
            return cgloadint(cg, node->as.literal.int32);
        case ND_NEG:
            return cgneg(cg, gen_expr(cg, node->left));
        case ND_FN_CALL:
            println(cg, "mov %s, 0", reg_to_str(RETREG));
            println(cg, "bl %s", node->as.name);
            return RETREG;
        case ND_VAR:
            if(node->as.var.type == OBJ_LOCAL) {
                return load_local(cg, node->as.var.name);
            } else {
                assert(node->as.var.type == OBJ_GLOBAL);
                return load_global(cg, node->as.var.name, node->loc);
            }
        case ND_ASSIGN: {
            ASTObjType type = node->left->as.var.type;
            ASTObj *lvalue = get_local(cg, node->left->as.var.name);
            Register rvalue = gen_expr(cg, node->right);
            if(type == OBJ_LOCAL) {
                println(cg, "str %s, [fp, %d]", reg_to_str(rvalue), lvalue->offset);
            } else {
                assert(type == OBJ_GLOBAL);
                Register addr = load_global_addr(cg, node->left->as.var.name, node->loc);
                println(cg, "str %s, [%s]", reg_to_str(rvalue), reg_to_str(addr));
                free_register(cg, addr);
            }
            return rvalue;
        }
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
        case ND_LOOP: {
            int c = count(cg);
            // initializer
            if(node->as.conditional.initializer != NULL) {
                gen_stmt(cg, node->as.conditional.initializer);
            }
            print(cg, ".L.begin.%d:\n", c);
            // condition
            if(node->as.conditional.condition != NULL) {
                Register cond = gen_expr(cg, node->as.conditional.condition);
                println(cg, "cmp %s, 0", reg_to_str(cond));
                println(cg, "beq .L.end.%d", c);
                free_register(cg, cond);
            }
            // body
            gen_stmt(cg, node->as.conditional.then);
            // increment
            if(node->as.conditional.increment != NULL) {
                free_register(cg, gen_expr(cg, node->as.conditional.increment));
            }
            println(cg, "b .L.begin.%d", c);
            print(cg, ".L.end.%d:\n", c);
            break;
        }
        case ND_IF: {
            int c = count(cg);
            Register cond = gen_expr(cg, node->as.conditional.condition);
            println(cg, "cmp %s, 0", reg_to_str(cond));
            println(cg, "beq .L.else.%d", c);
            free_register(cg, cond);
            gen_stmt(cg, node->as.conditional.then);
            println(cg, "b .L.end.%d", c);
            print(cg, ".L.else.%d:\n", c);
            if(node->as.conditional.els) {
                gen_stmt(cg, node->as.conditional.els);
            }
            print(cg, ".L.end.%d:\n", c);
            break;
        }
        case ND_RETURN: {
            if(node->left) {
                Register val = gen_expr(cg, node->left);
                if(val == NOREG) {
                    return;
                }
                println(cg, "mov %s, %s", reg_to_str(RETREG), reg_to_str(val));
                free_register(cg, val);
            }
            println(cg, "b .L.return.%s", cg->current_fn->name);
            break;
        }
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
        case ND_BLOCK:
            for(int i = 0; i < (int)node->as.body.used; ++i) {
                gen_stmt(cg, ARRAY_GET_AS(ASTNode *, &node->as.body, i));
            }
            break;
        default:
            UNREACHABLE();
    }
    return;
}

static int assign_local_var_offsets(Array *locals) {
    int offset = 0;
    for(size_t i = 0; i < locals->used; ++i) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, locals, i);
        obj->offset = -offset;
        offset += 8;
    }
    return align_to(offset, 16);
}

static void emit_functions(CodeGenerator *cg) {
    Array *fns = &cg->program->functions;

    for(size_t i = 0; i < fns->used; ++i) {
        ASTFunction *fn = ARRAY_GET_AS(ASTFunction *, fns, i);
        int stack_frame_size = assign_local_var_offsets(&fn->locals);
        bool isMain = stringEqual(fn->name, "main");
        // NOTE: assumes there are only toplevel functions.
        cg->current_fn = fn;
        println(cg, ".global %s\n"
                    "%s:", fn->name, fn->name);
        // 16 == 2 64bit registers
        println(cg, "stp fp, lr, [sp, -16]!");
        if(cg->call_init_globals && isMain) {
            println(cg, "// initialize globals");
            println(cg, "bl .initialize_globals");
        }
        println(cg, "mov fp, sp");
        println(cg, "sub sp, sp, %d", stack_frame_size);

        gen_stmt(cg, fn->body);

        if(isMain) {
            // main returns 0 by default
            println(cg, "mov x0, 0");
        }
        print(cg, ".L.return.%s:\n", fn->name);
        println(cg, "mov sp, fp");
        println(cg, "ldp fp, lr, [sp], 16");
        println(cg, "ret\n");
    }
}

static void emit_global_initializers(CodeGenerator *cg) {
    Array initExprs;
    initArray(&initExprs);
    for(size_t i = 0; i < cg->program->globals.used; ++i) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &cg->program->globals, i)->left;
        if(g->type != ND_ASSIGN || (g->type == ND_ASSIGN && g->right->type == ND_NUM)) {
            continue;
        }
        arrayPush(&initExprs, g);
    }
    if(initExprs.used == 0) {
        // no globals to initialize
        return;
    }
    cg->call_init_globals = true;

    println(cg, ".local .initialize_globals\n"
                ".initialize_globals:\n");
    println(cg, "stp fp, lr, [sp, -16]!");
    for(size_t i = 0; i < initExprs.used; ++i) {
        ASTNode *g = ARRAY_GET_AS(ASTNode *, &initExprs, i);
        println(cg, "// initialize global variable '%s'", g->left->as.var.name);
        free_register(cg, gen_expr(cg, g));
    }
    println(cg, "ldp fp, lr, [sp], 16");
    println(cg, "ret\n");
}

void emit_text(CodeGenerator *cg) {
    println(cg, ".text");
    emit_global_initializers(cg);
    emit_functions(cg);
}

static void emit_data(CodeGenerator *cg) {
    println(cg, ".data");
    // temporary data for builtin print statement
    if(cg->print_stmt_used) {
        println(cg, ".local printf_int_str\n"
                    "printf_int_str:\n"
                    "\t.string \"%%d\\n\"\n");
    }

    for(size_t i = 0; i < cg->program->globals.used; ++i) {
        // globals are wrapped in ND_EXPR_STMTs
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &cg->program->globals, i)->left;
        ASTObj *g = NULL;
        if(n->type == ND_ASSIGN) {
            g = &n->left->as.var;
        } else {
            g = &n->as.var;
        }
        // NOTE: g->type isn't neccessarily OBJ_GLOBAL as global static variables are "local".
        println(cg, ".global %s\n"
                    "%s:", g->name, g->name);
        if(n->type == ND_ASSIGN && n->right->type == ND_NUM) {
            // dword == double word == 8 bytes
            println(cg, ".dword %d", n->right->as.literal.int32);
        } else {
            println(cg, ".zero 8");
        }
    }
}

void codegen(CodeGenerator *cg) {
    emit_text(cg);
    print(cg, "\n");
    emit_data(cg);
}

#undef error
#undef errorf
