#include <stdio.h>
#include <string.h> // memset()
#include <stdarg.h>
#include "common.h"
#include "codegen.h"

void initCodegen(CodeGenerator *cg, ASTProg *program, FILE *file) {
    cg->out = file;
    cg->program = program;
    memset(cg->free_regs, true, _REG_COUNT);
}
void freeCodegen(CodeGenerator *cg) {
    // nothing
}

// TODO: register spilling
static Register allocate_register(CodeGenerator *cg) {
    for(int i = 0; i < _REG_COUNT; ++i) {
        if(cg->free_regs[i]) {
            cg->free_regs[i] = false;
            return (Register)i;
        }
    }
    // no registers left
    UNREACHABLE();
}

static void free_register(CodeGenerator *cg, Register reg) {
    if(cg->free_regs[reg]) {
        // double free
        UNREACHABLE();
    }
    cg->free_regs[reg] = true;
}

static const char *registers[_REG_COUNT] = {"x0", "x1", "x2", "x3"};
static const char *reg_to_str(Register reg) {
    if(reg > _REG_COUNT) {
        UNREACHABLE();
    }
    return registers[reg];
}

static void println(CodeGenerator *cg, const char *format, ...) {
    fputs("\t", cg->out);
    va_list ap;
    va_start(ap, format);
    vfprintf(cg->out, format, ap);
    va_end(ap);
    fputs("\n", cg->out);
}

// returns the register that will contain the result
static Register gen_expr(CodeGenerator *cg, ASTNode *expr) {
    if(expr->type == ND_NUM) {
        Register reg = allocate_register(cg);
        println(cg, "mov %s, %d", reg_to_str(reg), expr->literal.int32);
        return reg;
    } else if(expr->type == ND_NEG) {
        Register reg = gen_expr(cg, expr->left);
        println(cg, "neg %s, %s", reg_to_str(reg), reg_to_str(reg));
        return reg;
    }

    Register leftreg = gen_expr(cg, expr->left);
    Register rightreg = gen_expr(cg, expr->right);

    const char *left = reg_to_str(leftreg);
    const char *right = reg_to_str(rightreg);

    switch(expr->type) {
        case ND_ADD:
            println(cg, "add %s, %s, %s", left, left, right);
            break;
        case ND_SUB:
            println(cg, "sub %s, %s, %s", left, left, right);
            break;
        case ND_MUL:
            println(cg, "mul %s, %s, %s", left, left, right);
            break;
        case ND_DIV:
            // use 'udiv' for unsigned values
            println(cg, "sdiv %s, %s, %s", left, left, right);
            break;
        case ND_REM: {
            // tmp = a / b
            // tmp = tmp * b
            // result = tmp - b
            Register tmp = allocate_register(cg);
            println(cg, "sdiv %s, %s, %s", reg_to_str(tmp), left, right);
            println(cg, "mul %s, %s, %s", right, reg_to_str(tmp), right);
            println(cg, "sub %s, %s, %s", left, left, right);
            free_register(cg, tmp);
            break;
        }
        case ND_EQ:
            println(cg, "cmp %s, %s", left, right);
            println(cg, "cset %s, eq", left);
            break;
        case ND_NE:
            println(cg, "cmp %s, %s", left, right);
            println(cg, "cset %s, ne", left);
            break;
        case ND_GT:
            println(cg, "cmp %s, %s", left, right);
            println(cg, "cset %s, gt", left);
            break;
        case ND_GE:
            println(cg, "cmp %s, %s", left, right);
            println(cg, "cset %s, ge", left);
            break;
        case ND_LT:
            println(cg, "cmp %s, %s", left, right);
            println(cg, "cset %s, lt", left);
            break;
        case ND_LE:
            println(cg, "cmp %s, %s", left, right);
            println(cg, "cset %s, le", left);
            break;
        case ND_BIT_OR:
            println(cg, "orr %s, %s, %s", left, left, right);
            break;
        case ND_XOR:
            println(cg, "eor %s, %s, %s", left, left, right);
            break;
        case ND_BIT_AND:
            println(cg, "and %s, %s, %s", left, left, right);
            break;
        case ND_BIT_RSHIFT:
            println(cg, "lsr %s, %s, %s", left, left, right);
            break;
        case ND_BIT_LSHIFT:
            println(cg, "lsl %s, %s, %s", left, left, right);
            break;
        default:
            UNREACHABLE();
    }

    free_register(cg, rightreg);
    return leftreg;
}

void gen_stmt(CodeGenerator *cg, ASTNode *node) {
    if(node->type != ND_EXPR_STMT) {
        UNREACHABLE();
    }
    // FIXME: what if result isn't always in x0 (R0)?
    gen_expr(cg, node->left);
}

void codegen(CodeGenerator *cg) {
    println(cg, ".global main\n"
                "main:\n");

    for(int i = 0; i < (int)cg->program->used; ++i) {
        gen_stmt(cg, ASTProgAt(cg->program, i));
    }
    
    // return
    println(cg, "ret");
}
