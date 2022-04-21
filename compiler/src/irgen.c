#include "common.h"
#include "ast.h"
#include "IR.h"

#define emitByte(builder, byte) IRBuilderWriteByte(builder, byte)

static void gen_expr(IRBuilder *builder, ASTNode *node) {
    switch(node->type) {
        case ND_NUM:
            emitByte(builder, IRBuilderAddInt32Literal(builder, node->as.literal.int32));
            return;
        case ND_NEG:
            gen_expr(builder, node->left);
            emitByte(builder, OP_NEG);
            return;
        default:
            break;
    }

    gen_expr(builder, node->left);
    gen_expr(builder, node->right);

    switch(node->type) {
        case ND_ADD:
            emitByte(builder, OP_ADD);
            break;
        case ND_SUB:
            emitByte(builder, OP_SUB);
            break;
        case ND_MUL:
            emitByte(builder, OP_MUL);
            break;
        case ND_DIV:
            emitByte(builder, OP_DIV);
            break;
        case ND_REM:
            emitByte(builder, OP_MOD);
            break;
        case ND_EQ:
            emitByte(builder, OP_EQ);
            break;
        case ND_NE:
            emitByte(builder, OP_NE);
            break;
        case ND_GT:
            emitByte(builder, OP_GT);
            break;
        case ND_GE:
            emitByte(builder, OP_GE);
            break;
        case ND_LT:
            emitByte(builder, OP_LT);
            break;
        case ND_LE:
            emitByte(builder, OP_LE);
            break;
        case ND_BIT_OR:
            emitByte(builder, OP_OR);
            break;
        case ND_XOR:
            emitByte(builder, OP_XOR);
            break;
        case ND_BIT_AND:
            emitByte(builder, OP_AND);
            break;
        case ND_BIT_RSHIFT:
            emitByte(builder, OP_RSHIFT);
            break;
        case ND_BIT_LSHIFT:
            emitByte(builder, OP_LSHIFT);
            break;
        default:
            UNREACHABLE();
    }
}

void genIR(ASTProg *prog) {
    IRBuilder builder;
    initIRBuilder(&builder);


}


#undef emitByte
