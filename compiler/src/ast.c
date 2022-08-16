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

ASTNode *astNewListNode(ASTNodeType type, Location loc, Array body) {
    ASTListNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->body = body;
    return AS_NODE(n);
}

ASTNode *astNewConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTListNode *body, ASTNode *else_) {
    ASTConditionalNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->condition = condition;
    n->body = body;
    n->else_ = else_;
    return AS_NODE(n);
}

ASTNode *astNewLoopNode(ASTNodeType type, Location loc, ASTNode *initializer, ASTNode *condition, ASTNode *increment, ASTListNode *body) {
    ASTLoopNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->initializer = initializer;
    n->condition = condition;
    n->increment = increment;
    n->body = body;
    return AS_NODE(n);
}


static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_ADD]       = "ND_ADD",
        [ND_SUB]       = "ND_SUB",
        [ND_MUL]       = "ND_MUL",
        [ND_DIV]       = "ND_DIV",
        [ND_EQ]        = "ND_EQ",
        [ND_NE]        = "ND_NE",
        [ND_NEG]       = "ND_NEG",
        [ND_NUMBER]    = "ND_NUMBER",
        [ND_EXPR_STMT] = "ND_EXPR_STMT",
        [ND_IF]        = "ND_IF",
        [ND_BLOCK]     = "ND_BLOCK",
        [ND_LOOP]      = "ND_LOOP"
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
        case ND_EQ:
        case ND_NE:
            return "ASTBinaryNode";
        // unary node
        case ND_NEG:
        case ND_EXPR_STMT:
            return "ASTUnaryNode";
        // conditional nodes
        case ND_IF:
            return "ASTConditionalNode";
        // list nodes
        case ND_BLOCK:
            return "ASTListNode";
        // loop nodes
        case ND_LOOP:
            return "ASTLoopNode";
        // other nodes
        case ND_NUMBER:
            return "ASTNumberNode";
        default:
            break;
    }
    UNREACHABLE();
}

void astPrint(FILE *to, ASTNode *node) {
    if(!node) {
        fprintf(to, "(null)");
        return;
    }
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
        case ND_EQ:
        case ND_NE:
            fprintf(to, ", \x1b[1mleft:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->left);
            fprintf(to, ", \x1b[1mright:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
        case ND_EXPR_STMT:
            fprintf(to, ", \x1b[1moperand:\x1b[0m ");
            astPrint(to, AS_UNARY_NODE(node)->operand);
            break;
        // conditional nodes
        case ND_IF:
            fprintf(to, ", \x1b[1mcondition:\x1b[0m ");
            astPrint(to, AS_CONDITIONAL_NODE(node)->condition);
            fprintf(to, ", \x1b[1mbody:\x1b[0m ");
            astPrint(to, AS_NODE(AS_CONDITIONAL_NODE(node)->body));
            fprintf(to, ", \x1b[1melse:\x1b[0m ");
            astPrint(to, AS_CONDITIONAL_NODE(node)->else_);
            break;
        // list nodes
        case ND_BLOCK:
            fprintf(to, ", \x1b[1mbody:\x1b[0m ");
            if(AS_LIST_NODE(node)->body.used == 0) {
                fputs("(empty)", to);
            }
            for(usize i = 0; i < AS_LIST_NODE(node)->body.used; ++i) {
                astPrint(to, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(node)->body, i));
            }
            break;
        // loop nodes
        case ND_LOOP:
            fprintf(to, ", \x1b[1minitializer:\x1b[0m ");
            astPrint(to, AS_LOOP_NODE(node)->initializer);
            fprintf(to, ", \x1b[1mcondition:\x1b[0m ");
            astPrint(to, AS_LOOP_NODE(node)->condition);
            fprintf(to, ", \x1b[1mincrement:\x1b[0m ");
            astPrint(to, AS_LOOP_NODE(node)->increment);
            fprintf(to, ", \x1b[1mbody:\x1b[0m ");
            astPrint(to, AS_NODE(AS_LOOP_NODE(node)->body));
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

void astFree(ASTNode *node) {
    if(!node) {
        return;
    }
    switch(node->type) {
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_EQ:
        case ND_NE:
            astFree(AS_BINARY_NODE(node)->left);
            astFree(AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
        case ND_EXPR_STMT:
            astFree(AS_UNARY_NODE(node)->operand);
            break;
        // conditional nodes
        case ND_IF:
            astFree(AS_CONDITIONAL_NODE(node)->condition);
            astFree(AS_NODE(AS_CONDITIONAL_NODE(node)->body));
            astFree(AS_CONDITIONAL_NODE(node)->else_);
            break;
        // list nodes
        case ND_BLOCK:
            for(usize i = 0; i < AS_LIST_NODE(node)->body.used; ++i) {
                astFree(ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(node)->body, i));
            }
            arrayFree(&AS_LIST_NODE(node)->body);
            break;
        // loop nodes
        case ND_LOOP:
            astFree(AS_LOOP_NODE(node)->initializer);
            astFree(AS_LOOP_NODE(node)->condition);
            astFree(AS_LOOP_NODE(node)->increment);
            astFree(AS_NODE(AS_LOOP_NODE(node)->body));
            break;
        default:
            break;
    }
    FREE(node);
}
