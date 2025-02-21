#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Token.h"
#include "Ast/Ast.h"

/* Helper functions */

static inline ASTStmtNode make_header(ASTStmtType type, Location loc) {
    return (ASTStmtNode){
        .type = type,
        .location = loc
    };
}

static const char *stmt_type_name(ASTStmtType type) {
    switch(type) {
        // VarDecl nodes
        case STMT_VAR_DECL:
            return "ASTVarDeclStmt";
        // Block nodes
        case STMT_BLOCK:
            return "ASTBlockStmt";
        // Conditional nodes
        case STMT_IF:
        case STMT_EXPECT:
            return "ASTConditionalStmt";
        // Loop nodes
        case STMT_LOOP:
            return "ASTLoopStmt";
        // Expr nodes
        case STMT_RETURN:
        case STMT_EXPR:
            return "ASTExprStmt";
        // Defer nodes
        case STMT_DEFER:
            return "ASTDeferStmt";
        default:
            UNREACHABLE();
    }
}

static const char *stmt_type_to_string(ASTStmtType type) {
    VERIFY(type < STMT_TYPE_COUNT);
    static const char *names[] = {
        [STMT_VAR_DECL] = "STMT_VAR_DECL",
        [STMT_BLOCK]    = "STMT_BLOCK",
        [STMT_IF]       = "STMT_IF",
        [STMT_EXPECT]   = "STMT_EXPECT",
        [STMT_LOOP]     = "STMT_LOOP",
        [STMT_RETURN]   = "STMT_RETURN",
        [STMT_EXPR]     = "STMT_EXPR",
        [STMT_DEFER]    = "STMT_DEFER"
    };
    return names[(int)type];
}


/* StmtNode functions */

void astStmtPrint(FILE *to, ASTStmtNode *stmt) {
    if(!stmt) {
        fputs("(null)", to);
        return;
    }

    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m, \x1b[1mlocation:\x1b[0m ", stmt_type_name(stmt->type), stmt_type_to_string(stmt->type));
    locationPrint(to, stmt->location, true);
    switch(stmt->type) {
        // VarDecl nodes
        case STMT_VAR_DECL:
            fputs(", \x1b[1mvariable:\x1b[0m ", to);
            astObjectPrint(to, NODE_AS(ASTVarDeclStmt, stmt)->variable, true);
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTVarDeclStmt, stmt)->initializer);
            break;
        // Block nodes
        case STMT_BLOCK: {
            fputs(", \x1b[1mnodes:\x1b[0m [", to);
            Array *nodes = &NODE_AS(ASTBlockStmt, stmt)->nodes;
            ARRAY_FOR(i, *nodes) {
                astStmtPrint(to, ARRAY_GET_AS(ASTStmtNode *, nodes, i));
                if(i + 1 < arrayLength(nodes)) {
                    fputs(", ", to);
                }
            }
            fputc(']', to);
            break;
        }
        // Conditional nodes
        case STMT_IF:
        case STMT_EXPECT:
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTConditionalStmt, stmt)->condition);
            fputs(", \x1b[1mthen:\x1b[0m ", to);
            astStmtPrint(to, NODE_AS(ASTConditionalStmt, stmt)->then);
            fputs(", \x1b[1melse:\x1b[0m ", to);
            astStmtPrint(to, NODE_AS(ASTConditionalStmt, stmt)->else_);
            break;
        // Loop nodes
        case STMT_LOOP:
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astStmtPrint(to, NODE_AS(ASTLoopStmt, stmt)->initializer);
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTLoopStmt, stmt)->condition);
            fputs(", \x1b[1mincrement:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTLoopStmt, stmt)->increment);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astStmtPrint(to, (ASTStmtNode *)NODE_AS(ASTLoopStmt, stmt)->body);
            break;
        // Expr nodes
        case STMT_RETURN:
        case STMT_EXPR:
            fputs(", \x1b[1mexpression:\x1b[0m ", to);
            astExprPrint(to, NODE_AS(ASTExprStmt, stmt)->expression);
            break;
        // Defer nodes
        case STMT_DEFER:
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astStmtPrint(to, NODE_AS(ASTDeferStmt, stmt)->body);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

ASTVarDeclStmt *astVarDeclStmtNew(Allocator *a, Location loc, ASTObj *var, ASTExprNode *init) {
    ASTVarDeclStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(STMT_VAR_DECL, loc);
    n->variable = var;
    n->initializer = init;
    return n;
}

ASTBlockStmt *astBlockStmtNew(Allocator *a, Location loc, Scope *scope, Array *nodes) {
    ASTBlockStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(STMT_BLOCK, loc);
    arrayInitAllocatorSized(&n->nodes, *a, arrayLength(nodes));
    n->scope = scope;
    arrayCopy(&n->nodes, nodes);
    return n;
}

ASTConditionalStmt *astConditionalStmtNew(Allocator *a, ASTStmtType type, Location loc, ASTExprNode *cond, ASTStmtNode *then, ASTStmtNode *else_) {
    ASTConditionalStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->condition = cond;
    n->then = then;
    n->else_ = else_;
    return n;
}

ASTLoopStmt *astLoopStmtNew(Allocator *a, Location loc, ASTStmtNode *init, ASTExprNode *cond, ASTExprNode *inc, ASTBlockStmt *body) {
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
