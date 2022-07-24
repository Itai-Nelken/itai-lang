#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "Strings.h"
#include "Array.h"
#include "Errors.h"
#include "ast.h"
#include "Types.h"
#include "Codegen.h"

void initCodegen(Codegenerator *cg, ASTProg *prog) {
    cg->prog = prog;
    cg->output_buffer = NULL;
    cg->output_buffer_size = 0;
    cg->output = open_memstream(&cg->output_buffer, &cg->output_buffer_size);
}

void freeCodegen(Codegenerator *cg) {
    cg->prog = NULL;
    fclose(cg->output);
    free(cg->output_buffer);
    cg->output_buffer = NULL;
    cg->output_buffer_size = 0;
}

#define append(txt) fprintf(cg->output, "%s", (txt))
#define appendf(fmt, ...) fprintf(cg->output, (fmt), __VA_ARGS__)

static void codegen_header(Codegenerator *cg) {
    static const char *header =
        "#include <stdbool.h>\n"
        "#include <stdint.h>\n"
        "#include <stdlib.h>\n"
        "typedef int16_t i16;\n"
        "typedef int32_t i32;\n"
        "typedef int64_t i64;\n"
        "typedef __int128_t i128;\n"
        "typedef uint8_t u8;\n"
        "typedef uint16_t u16;\n"
        "typedef uint32_t u32;\n"
        "typedef uint64_t u64;\n"
        "typedef __uint128_t u128;\n"
        "typedef ssize_t isize;\n"
        "typedef size_t usize;\n"
        "typedef float f32;\n"
        "typedef double f64;\n"
        "typedef const char *str;\n";
    
    appendf("// Generated code for itai-lang.\n%s\n", header);
}

static char *get_identifier(Codegenerator *cg, int id) {
    ASTIdentifier *ident = GET_SYMBOL_AS(ASTIdentifier *, &cg->prog->identifiers, id);
    return ident->text;
}

static const char *get_type_name(Codegenerator *cg, Type ty) {
    static const char *names[TY_COUNT] = {
        [TY_I8]     = "i8",
        [TY_I16]    = "i16",
        [TY_I32]    = "i32",
        [TY_I64]    = "i64",
        [TY_I128]   = "i128",
        [TY_U8]     = "u8",
        [TY_U16]    = "u16",
        [TY_U32]    = "u32",
        [TY_U64]    = "u64",
        [TY_U128]   = "u128",
        [TY_F32]    = "f32",
        [TY_F64]    = "f64",
        [TY_ISIZE]  = "isize",
        [TY_USIZE]  = "usize",
        [TY_CHAR]   = "char",
        [TY_STR]    = "str",
        [TY_BOOL]   = "bool",
        [TY_CUSTOM] = "TY_CUSTOM",
        [TY_NONE]   = "void"
    };

    if(ty.type == TY_CUSTOM) {
        int name_id = (int)ARRAY_GET_AS(long, &cg->prog->types, ty.id);
        ASTIdentifier *id = GET_SYMBOL_AS(ASTIdentifier *, &cg->prog->identifiers, name_id);
        return (const char *)id->text;
    }
    return names[ty.type];
}

static void gen_expr(Codegenerator *cg, ASTNode *node) {
    // handle unary expressions
    switch(node->type) {
        case ND_IDENTIFIER:
            appendf("%s", get_identifier(cg, AS_IDENTIFIER_NODE(node)->id));
            return;
        case ND_NUM:
            appendf("%d", AS_LITERAL_NODE(node)->as.int32);
            return;
        case ND_NEG:
            append("-");
            gen_expr(cg, AS_UNARY_NODE(node)->child);
            return;
        case ND_CALL:
            appendf("%s()", get_identifier(cg, AS_IDENTIFIER_NODE(AS_UNARY_NODE(node)->child)->id));
            return;
        default:
            break;
    }

    ASTBinaryNode *n = AS_BINARY_NODE(node);
    gen_expr(cg, n->left);
    switch(node->type) {
        case ND_ASSIGN:
            append(" = ");
            break;
        case ND_ADD:
            append(" + ");
            break;
        case ND_SUB:
            append(" - ");
            break;
        case ND_MUL:
            append(" * ");
            break;
        case ND_DIV:
            append(" / ");
            break;
        case ND_REM:
            append(" % ");
            break;
        case ND_EQ:
            append(" == ");
            break;
        case ND_NE:
            append(" != ");
            break;
        case ND_GT:
            append(" > ");
            break;
        case ND_GE:
            append(" >= ");
            break;
        case ND_LT:
            append(" < ");
            break;
        case ND_LE:
            append(" <= ");
            break;
        case ND_BIT_OR:
            append(" | ");
            break;
        case ND_XOR:
            append(" ^ ");
            break;
        case ND_BIT_AND:
            append(" & ");
            break;
        case ND_BIT_RSHIFT:
            append(" >> ");
            break;
        case ND_BIT_LSHIFT:
            append(" << ");
            break;
        default:
            UNREACHABLE();
    }
    gen_expr(cg, n->right);
}

static void gen_stmt(Codegenerator *cg, ASTNode *node) {
    switch(node->type) {
        case ND_LOOP: {
            ASTLoopNode *n = AS_LOOP_NODE(node);
            append("for(");
            if(n->init) {
                gen_stmt(cg, n->init);
            } else {
                append(";");
            }
            if(n->condition) {
                gen_expr(cg, n->condition);
            }
            append(";");
            if(n->increment) {
                gen_expr(cg, n->increment);
            }
            append(")");
            gen_stmt(cg, n->body);
            break;
        }
        case ND_IF: {
            ASTConditionalNode *n = AS_CONDITIONAL_NODE(node);
            append("if(");
            gen_expr(cg, n->condition);
            append(")");
            gen_stmt(cg, n->body);
            if(n->else_) {
                append(" else ");
                gen_stmt(cg, n->else_);
            }
            break;
        }
        case ND_RETURN:
            append("return ");
            if(AS_UNARY_NODE(node)->child) {
                gen_expr(cg, AS_UNARY_NODE(node)->child);
            }
            append(";\n");
            break;
        case ND_BLOCK:
            append("{\n");
            for(usize i = 0; i < AS_BLOCK_NODE(node)->body.used; ++i) {
                gen_stmt(cg, ARRAY_GET_AS(ASTNode *, &AS_BLOCK_NODE(node)->body, i));
            }
            append("}\n");
            break;
        case ND_EXPR_STMT:
            gen_expr(cg, AS_UNARY_NODE(node)->child);
            append(";\n");
            break;
        default:
            UNREACHABLE();
    }
}

static void codegen_global(void *global, void *codegenerator) {
    Codegenerator *cg = (Codegenerator *)codegenerator;
    ASTNode *g = AS_NODE(global);
    switch(g->type) {
        case ND_IDENTIFIER: {
            Type ty = AS_IDENTIFIER_NODE(g)->type;
            if(ty.type == TY_NONE) {
                printErrorStr(ERR_ERROR, ty.location, "Expected a type!");
            } else {
                appendf("%s %s = 0;\n", get_type_name(cg, ty), get_identifier(cg, AS_IDENTIFIER_NODE(g)->id));
            }
            break;
        }
        case ND_EXPR_STMT: {
            ASTNode *rvalue = AS_BINARY_NODE(AS_UNARY_NODE(g)->child)->right;
            ASTIdentifierNode *lvalue = AS_IDENTIFIER_NODE(AS_BINARY_NODE(AS_UNARY_NODE(g)->child)->left);
            appendf("%s %s = ", get_type_name(cg, lvalue->type), get_identifier(cg, lvalue->id));
            gen_expr(cg, rvalue);
            append(";\n");
            break;
        }
        default:
            UNREACHABLE();
    }
}

static void codegen_function(void *function, void *codegenerator) {
    ASTFunction *fn = (ASTFunction *)function;
    Codegenerator *cg = (Codegenerator *)codegenerator;

    appendf("%s %s() {\n", get_type_name(cg, fn->return_type), GET_SYMBOL_AS(ASTIdentifier *, &cg->prog->identifiers, fn->name->id)->text);
    // emit locals
    for(usize i = 0; i < fn->locals.used; ++i) {
        ASTNode *l = ARRAY_GET_AS(ASTNode *, &fn->locals, i);
        switch(l->type) {
            case ND_IDENTIFIER: {
                Type ty = AS_IDENTIFIER_NODE(l)->type;
                if(ty.type == TY_NONE) {
                    printErrorStr(ERR_ERROR, ty.location, "Expected a type!");
                } else {
                    appendf("%s %s = 0;\n", get_type_name(cg, ty), get_identifier(cg, AS_IDENTIFIER_NODE(l)->id));
                }
                break;
            }
            case ND_EXPR_STMT: {
                ASTNode *rvalue = AS_BINARY_NODE(AS_UNARY_NODE(l)->child)->right;
                ASTIdentifierNode *lvalue = AS_IDENTIFIER_NODE(AS_BINARY_NODE(AS_UNARY_NODE(l)->child)->left);
                appendf("%s %s = ", get_type_name(cg, lvalue->type), get_identifier(cg, lvalue->id));
                gen_expr(cg, rvalue);
                append(";\n");
                break;
            }
            default:
                UNREACHABLE();
        }
    }
    // emit body
    gen_stmt(cg, AS_NODE(fn->body));
    append("}\n");
}

#undef append
#undef appendf

bool codegen(Codegenerator *cg) {
    codegen_header(cg);
    arrayMap(&cg->prog->globals, codegen_global, cg);
    arrayMap(&cg->prog->functions, codegen_function, cg);
    fflush(cg->output);
    printf("%s\n", cg->output_buffer);
    return true;
}
