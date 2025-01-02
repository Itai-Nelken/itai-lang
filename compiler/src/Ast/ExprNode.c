#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Token.h"
#include "Ast/Ast.h" // NODE_AS, NODE_IS
#include "Ast/StringTable.h"
#include "Ast/Type.h"
#include "Ast/Object.h"
#include "Ast/ExprNode.h"

/* Helper functions */

static inline ASTExprNode make_header(ASTExprType type, Location loc, Type *exprTy) {
    return (ASTExprNode){
        .type = type,
        .location = loc,
        .dataType = exprTy
    };
}

static const char *node_name_to_string(ASTExprType t) {
    switch(t) {
        case EXPR_NUMBER_CONSTANT:
        case EXPR_STRING_CONSTANT:
            return "ASTConstantValueExpr";
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            return "ASTObjExpr";
        case EXPR_ASSIGN:
        case EXPR_PROPERTY_ACCESS:
        case EXPR_ADD:
        case EXPR_SUBTRACT:
        case EXPR_MULTIPLY:
        case EXPR_DIVIDE:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
            return "ASTBinaryExpr";
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF:
            return "ASTUnaryExpr";
        case EXPR_CALL:
            return "ASTCallExpr";
        case EXPR_IDENTIFIER:
            return "ASTIdentifierExpr";
        default:
            break;
    }
    UNREACHABLE();
}

static const char *node_type_to_string(ASTExprType t) {
    VERIFY(t < EXPR_TYPE_COUNT);
    const char *names[] = {
        [EXPR_NUMBER_CONSTANT] = "EXPR_NUMBER_CONSTANT",
        [EXPR_STRING_CONSTANT] = "EXPR_STRING_CONSTANT",
        [EXPR_VARIABLE]        = "EXPR_VARIABLE",
        [EXPR_FUNCTION]        = "EXPR_FUNCTION",
        [EXPR_ASSIGN]          = "EXPR_ASSIGN",
        [EXPR_PROPERTY_ACCESS] = "EXPR_PROPERTY_ACCESS",
        [EXPR_ADD]             = "EXPR_ADD",
        [EXPR_SUBTRACT]        = "EXPR_SUBTRACT",
        [EXPR_MULTIPLY]        = "EXPR_MULTIPLY",
        [EXPR_DIVIDE]          = "EXPR_DIVIDE",
        [EXPR_EQ]              = "EXPR_EQ",
        [EXPR_NE]              = "EXPR_NE",
        [EXPR_LT]              = "EXPR_LT",
        [EXPR_LE]              = "EXPR_LE",
        [EXPR_GT]              = "EXPR_GT",
        [EXPR_GE]              = "EXPR_GE",
        // Unary nodes
        [EXPR_NEGATE]          = "EXPR_NEGATE",
        [EXPR_ADDROF]          = "EXPR_ADDROF",
        [EXPR_DEREF]           = "EXPR_DEREF",
        // Call node
        [EXPR_CALL]            = "EXPR_CALL",
        // Other nodes.
        [EXPR_IDENTIFIER]      = "EXPR_IDENTIFIER"
    };
    return names[(int)t];
}

/* ExprNode functions */

#define PRINT_ARRAY(type, print_fn, stream, array) ARRAY_FOR(i, (array)) { \
    print_fn((stream), ARRAY_GET_AS(type, &(array), i)); \
    if(i + 1 < arrayLength(&(array))) { \
        fputs(", ", (stream)); \
        } \
    }

void astExprPrint(FILE *to, ASTExprNode *n) {
    if(!n) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m, \x1b[1mlocation: \x1b[0m", node_name_to_string(n->type), node_type_to_string(n->type));
    locationPrint(to, n->location, true);
    fputs(", \x1b[1mdataType: \x1b[0m", to);
    typePrint(to, n->dataType, true);
    switch(n->type) {
        case EXPR_NUMBER_CONSTANT:
            fprintf(to, ", \x1b[1mvalue: \x1b[0;34m%lu\x1b[0m", NODE_AS(ASTConstantValueExpr, n)->as.number);
            break;
        case EXPR_STRING_CONSTANT:
            fprintf(to, ", \x1b[1mvalue: \x1b[0;34m%s\x1b[0m", NODE_AS(ASTConstantValueExpr, n)->as.string);
            break;
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            fputs(", \x1b[1mobj:\x1b[0m", to);
            astObjectPrint(to, NODE_AS(ASTObjExpr, n)->obj, true);
            break;
        case EXPR_ASSIGN:
        case EXPR_PROPERTY_ACCESS:
        case EXPR_ADD:
        case EXPR_SUBTRACT:
        case EXPR_MULTIPLY:
        case EXPR_DIVIDE:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
            fputs(", \x1b[1mlhs:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTBinaryExpr, n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTBinaryExpr, n)->rhs);
            break;
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF:
            fputs(", \x1b[1moperand:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTUnaryExpr, n)->operand);
            break;
        case EXPR_CALL:
            fputs(", \x1b[1mcallee:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTCallExpr, n)->callee);
            fputs(", \x1b[1marguments:\x1b[0m [", to);
            PRINT_ARRAY(ASTExprNode *, astExprPrint, to, NODE_AS(ASTCallExpr, n)->arguments);
            fputs("]", to);
            break;
        case EXPR_IDENTIFIER:
            fprintf(to, ", \x1b[1mid:\x1b[0m '%s'", NODE_AS(ASTIdentifierExpr, n)->id);
            break;
        default:
            UNREACHABLE();
    }
    fputs("}", to);
}
#undef PRINT_ARRAY

ASTConstantValueExpr *astConstantValueExprNew(Allocator *a, ASTExprType type, Location loc, Type *valueTy) {
    ASTConstantValueExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc, valueTy);
    return n;
}

ASTObjExpr *astObjExprNew(Allocator *a, ASTExprType type, Location loc, ASTObj *obj) {
    VERIFY(obj != NULL);
    ASTObjExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc, obj->dataType);
    n->obj = obj;
    return n;
}

ASTBinaryExpr *astBinaryExprNew(Allocator *a, ASTExprType type, Location loc, Type *exprTy, ASTExprNode *lhs, ASTExprNode *rhs) {
    ASTBinaryExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc, exprTy);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

ASTUnaryExpr *astUnaryExprNew(Allocator *a, ASTExprType type, Location loc, Type *exprTy, ASTExprNode *operand) {
    ASTUnaryExpr *n = allocatorAllocate(a, sizeof (*n));
    n->header = make_header(type, loc, exprTy);
    n->operand = operand;
    return n;
}

ASTCallExpr *astCallExprNew(Allocator *a, Location loc, Type *exprTy, ASTExprNode *callee, Array *arguments) {
    ASTCallExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(EXPR_CALL, loc, exprTy);
    n->callee = callee;
    arrayInitSized(&n->arguments, arrayLength(arguments));
    arrayCopy(&n->arguments, arguments);
    return n;
}

ASTIdentifierExpr *astIdentifierExprNew(Allocator *a, Location loc, ASTString id) {
    ASTIdentifierExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(EXPR_IDENTIFIER, loc, NULL);
    n->id = id;
    return n;
}
