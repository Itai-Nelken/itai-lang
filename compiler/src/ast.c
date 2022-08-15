#include <stdio.h>
#include "memory.h"
#include "Token.h"
#include "ast.h"

ASTNode *astNewNumberNode(Location loc, NumberConstant value) {
    ASTNumberNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = ND_NUMBER,
        .location = loc
    };
    n->value = value;
    return AS_NODE(n);
}

ASTNode *astNewUnaryNode(ASTNodeType type, Location loc, ASTNode *operand) {
    ASTUnaryNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->operand = operand;
    return AS_NODE(n);
}

ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right) {
    ASTBinaryNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->left = left;
    n->right = right;
    return AS_NODE(n);
}

static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_ADD]    = "ND_ADD",
        [ND_SUB]    = "ND_SUB",
        [ND_MUL]    = "ND_MUL",
        [ND_DIV]    = "ND_DIV",
        [ND_NEG]    = "ND_NEG",
        [ND_NUMBER] = "ND_NUMBER"
    };
    return names[(i32)type];
}

static const char *node_name(ASTNodeType type) {
    switch(type) {
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
            return "ASTBinaryNode";
        // unary node
        case ND_NEG:
            return "ASTUnaryNode";
        // other nodes
        case ND_NUMBER:
            return "ASTNumberNode";
        default:
            break;
    }
    UNREACHABLE();
}

void astPrint(FILE *to, ASTNode *node) {
    fprintf(to, "%s{\x1b[1mtype:\x1b[0;33m %s\x1b[0m", node_name(node->type), node_type_name(node->type));
    switch(node->type) {
        // other nodes
        case ND_NUMBER:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            printNumberConstant(to, AS_NUMBER_NODE(node)->value);
            break;
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
            fprintf(to, ", \x1b[1mleft:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->left);
            fprintf(to, ", \x1b[1mright:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
            fprintf(to, ", \x1b[1moperand:\x1b[0m ");
            astPrint(to, AS_UNARY_NODE(node)->operand);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

void astFree(ASTNode *node) {
    switch(node->type) {
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
            astFree(AS_BINARY_NODE(node)->left);
            astFree(AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
            astFree(AS_UNARY_NODE(node)->operand);
            break;
        default:
            break;
    }
    FREE(node);
}
