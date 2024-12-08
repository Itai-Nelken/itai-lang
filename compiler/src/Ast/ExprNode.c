#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Token.h"
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


/* ExprNode functions */

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
