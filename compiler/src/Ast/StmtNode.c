#include "common.h"
#include "memory.h"
#include "Ast/Object.h"
#include "Ast/ExprNode.h"
#include "Ast/StmtNode.h"

/* Helper functions */

static inline ASTStmtNode make_header(ASTStmtType type, Location loc) {
    return (ASTStmtNode){
        .type = type,
        .location = loc
    };
}


/* StmtNode functions */

ASTVarDeclStmt *astVarDeclStmtNew(Allocator *a, Location loc, ASTObj *var, ASTExprNode *init) {
    ASTVarDeclStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(STMT_VAR_DECL, loc);
    n->variable = var;
    n->initializer = init;
    return n;
}

ASTBlockStmt *astBlockStmtNew(Allocator *a, Location loc, Array *nodes) {
    ASTBlockStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(STMT_BLOCK, loc);
    arrayInitSized(&n->nodes, arrayLength(nodes));
    arrayCopy(&n->nodes, nodes);
    return n;
}

ASTConditionalStmt *astConditionalStmtNew(Allocator *a, Location loc, ASTExprNode *cond, ASTStmtNode *then, ASTStmtNode *else_) {
    ASTConditionalStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(STMT_IF, loc);
    n->condition = cond;
    n->then = then;
    n->else_ = else_;
    return n;
}

ASTLoopStmt *astLoopStmtNew(Allocator *a, Location loc, ASTExprNode *init, ASTExprNode *cond, ASTExprNode *inc, ASTBlockStmt *body) {
    ASTLoopStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(STMT_LOOP, loc);
    n->initializer = init;
    n->condition = cond;
    n->increment = inc;
    n->body = body;
    return n;
}

ASTExprStmt *astExprStmtNew(Allocator *a, ASTStmtType type, Location loc, ASTExprNode *expr) {
    ASTExprStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->expression = expr;
    return n;
}

ASTDeferStmt *astDeferStmtNew(Allocator *a, Location loc, ASTStmtNode *body) {
    ASTDeferStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(STMT_DEFER, loc);
    n->body = body;
    return n;
}
