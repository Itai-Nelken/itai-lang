#include <stdio.h>
#include <stdarg.h>
#include "common.h"
#include "codegen.h"

void initCodegen(CodeGenerator *cg, ASTProg program, FILE *file) {
    cg->out = file;
    cg->program = program;
    cg->stack_has_space = false;
}
void freeCodegen(CodeGenerator *cg) {
    // nothing
}

static void println(CodeGenerator *cg, const char *format, ...) {
    fputs("\t", cg->out);
    va_list ap;
    va_start(ap, format);
    vfprintf(cg->out, format, ap);
    va_end(ap);
    fputs("\n", cg->out);
}

static void gen_expr(CodeGenerator *cg, ASTNode *expr) {
    if(expr->type == ND_NUM) {
        println(cg, "mov x0, %d",expr->literal.int32);
        return;
    } else if(expr->type == ND_NEG) {
        gen_expr(cg, expr->left);
        println(cg, "neg x0, x0");
        return;
    }

    gen_expr(cg, expr->left);
    println(cg, "str x0, [sp, -16]!"); // push x0
    gen_expr(cg, expr->right);
    println(cg, "ldr x1, [sp, 8], 16"); // pop x1

    switch(expr->type) {
        case ND_ADD:
            println(cg, "add x0, x0, x1");
            break;
        case ND_SUB:
            println(cg, "sub x0, x0, x1");
            break;
        case ND_MUL:
            println(cg, "mul x0, x0, x1");
            break;
        case ND_DIV:
            // TODO: Check if unsigned values, and then use 'udiv'
            println(cg, "sdiv x0, x0, x1");
            break;
        case ND_EQ:
            println(cg, "cmp x0, x1");
            println(cg, "cset x0, eq");
            break;
        case ND_NE:
            println(cg, "cmp x0, x1");
            println(cg, "cset x0, ne");
            break;
        case ND_GT:
            println(cg, "cmp x0, x1");
            println(cg, "cset x0, gt");
            break;
        case ND_GE:
            println(cg, "cmp x0, x1");
            println(cg, "cset x0, ge");
            break;
        case ND_LT:
            println(cg, "cmp x0, x1");
            println(cg, "cset x0, lt");
            break;
        case ND_LE:
            println(cg, "cmp x0, x1");
            println(cg, "cset x0, le");
            break;
        case ND_BIT_OR:
            println(cg, "orr x0, x0, x1");
            break;
        case ND_XOR:
            println(cg, "eor x0, x0, x1");
            break;
        case ND_BIT_AND:
            println(cg, "and x0, x0, x1");
            break;
        default:
            UNREACHABLE();
    }
}

void codegen(CodeGenerator *cg) {
    println(cg, ".global main\n"
                "main:\n");

    gen_expr(cg, cg->program.expr);
    
    // return
    println(cg, "ret");
}
