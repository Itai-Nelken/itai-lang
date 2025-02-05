#include "common.h"
#include "Array.h"
#include "Strings.h"
#include "Error.h"
#include "Ast/Ast.h"
#include "Compiler.h"
#include "Typechecker.h"

static void typechecker_init_internal(Typechecker *typechecker, Compiler *c) {
    typechecker->compiler = c;
    typechecker->program = NULL; // set in typecheckerTypecheck()
    typechecker->hadError = false;
    typechecker->current.scope = NULL;
    typechecker->current.function = NULL;
    typechecker->current.module = NULL;
}

void typecheckerInit(Typechecker *typechecker, Compiler *c) {
    typechecker_init_internal(typechecker, c);
}

void typecheckerFree(Typechecker *typechecker) {
    typechecker_init_internal(typechecker, NULL);
}

static void add_error(Typechecker *typ, bool has_location, Location loc, ErrorType type, const char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, type, has_location, loc, message);
    compilerAddError(typ->compiler, err);
    typ->hadError = true;
}

static void error(Typechecker *typ, Location loc, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String msg = stringVFormat(format, ap);
    va_end(ap);

    add_error(typ, true, loc, ERR_ERROR, msg);
    stringFree(msg);
}

//static void hint(Typechecker *typ, Location loc, const char *format, ...) {
//    va_list ap;
//    va_start(ap, format);
//    String msg = stringVFormat(format, ap);
//    va_end(ap);
//
//    add_error(typ, true, loc, ERR_HINT, msg);
//    stringFree(msg);
//}

static inline const char *typeName(Type *ty) {
    if(ty == NULL) {
        return "none";
    }
    return (const char *)ty->name;
}

static void enterScope(Typechecker *typ, Scope *sc) {
    VERIFY(sc->parent = typ->current.scope);
    typ->current.scope = sc;
}

static void leaveScope(Typechecker *typ) {
    VERIFY(typ->current.scope->parent != NULL);
    typ->current.scope = typ->current.scope->parent;
}

static inline Scope *getCurrentScope(Typechecker *typ) {
    return typ->current.scope;
}

static inline ASTModule *getCurrentModule(Typechecker *typ) {
    return typ->current.module;
}

static bool checkTypes(Typechecker *typ, Location errLoc, Type *a, Type *b) {
    // ASTStrings can be compared by address (since they are unique).
    if(!a || !b || a->name != b->name) {
        if(a->type == TY_U32 && b->type == TY_I32) {
            return true;
        }
        error(typ, errLoc, "Type mismatch: expected '%s' but got '%s'.", typeName(a), typeName(b));
        return false;
    }
    return true;
}

static void typecheckVariableDecl(Typechecker *typ, ASTVarDeclStmt *decl) {
    if(decl->variable->dataType->type == TY_POINTER) {
        error(typ, decl->header.location, "Pointer types are not allowed in this context.");
        return;
    }
    if(decl->initializer && !checkTypes(typ, decl->header.location, decl->variable->dataType, decl->initializer->dataType)) {
        return;
    }
}

static void typecheckExpr(Typechecker *typ, ASTExprNode *expr) {
    switch(expr->type) {
        // Constant value nodes.
        case EXPR_NUMBER_CONSTANT:
        case EXPR_STRING_CONSTANT:
            // TODO: When postfix types are supported, check that there isn't something stupid like 123str.
            break;
        // Obj nodes
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            // nothing.
            break;
        // Binary nodes
        case EXPR_PROPERTY_ACCESS:
            // Note: nothing to typecheck here. The Validator makes sure the fields exist and have a type.
            break;
        case EXPR_ASSIGN:
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
            typecheckExpr(typ, NODE_AS(ASTBinaryExpr, expr)->lhs);
            typecheckExpr(typ, NODE_AS(ASTBinaryExpr, expr)->rhs);
            if(NODE_IS(NODE_AS(ASTBinaryExpr, expr)->lhs, EXPR_PROPERTY_ACCESS)) {
                ASTExprNode *rhs = NODE_AS(ASTBinaryExpr, NODE_AS(ASTBinaryExpr, expr)->lhs)->rhs;
                while(NODE_IS(rhs, EXPR_PROPERTY_ACCESS)) {
                    rhs = NODE_AS(ASTBinaryExpr, expr)->rhs;
                }
                checkTypes(typ, expr->location, rhs->dataType, NODE_AS(ASTBinaryExpr, expr)->rhs->dataType);
            } else {
                checkTypes(typ, expr->location, NODE_AS(ASTBinaryExpr, expr)->lhs->dataType, NODE_AS(ASTBinaryExpr, expr)->rhs->dataType);
            }
            break;
        // Unary nodes
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF:
            typecheckExpr(typ, NODE_AS(ASTUnaryExpr, expr)->operand);
            // TODO: check if negatable, addrofable type. (derefable checked in validator.)
            break;
        // Call node
        case EXPR_CALL:
            // TODO: typecheck arguments vs parameters.
            break;
        // Other nodes.
        case EXPR_IDENTIFIER:
            // Identifier nodes should not exist at this stage.
        default:
            UNREACHABLE();
    }
}

static void typecheckStmt(Typechecker *typ, ASTStmtNode *stmt) {
    switch(stmt->type) {
        // VarDecl nodes
        case STMT_VAR_DECL:
            typecheckVariableDecl(typ, NODE_AS(ASTVarDeclStmt, stmt));
            break;
        // Block nodes
        case STMT_BLOCK: {
            ASTBlockStmt *block = NODE_AS(ASTBlockStmt, stmt);
            enterScope(typ, block->scope);
            ARRAY_FOR(i, block->nodes) {
                ASTStmtNode *n = ARRAY_GET_AS(ASTStmtNode *, &block->nodes, i);
                typecheckStmt(typ, n);
            }
            leaveScope(typ);
            break;
        }
        // Conditional nodes
        case STMT_IF:
        // Loop nodes
        case STMT_LOOP:
            // nothing for now.
            break;
        // Expr nodes
        case STMT_RETURN:
            VERIFY(typ->current.function);
            if(NODE_AS(ASTExprStmt, stmt)->expression != NULL) {
                checkTypes(typ, NODE_AS(ASTExprStmt, stmt)->expression->location, typ->current.function->as.fn.returnType, NODE_AS(ASTExprStmt, stmt)->expression->dataType);
            } else {
                if(typ->current.function->as.fn.returnType->type != TY_VOID) {
                    error(typ, stmt->location, "Return with no value in function '%s' returning '%s'.", typ->current.function->name, typ->current.function->as.fn.returnType->name);
                }
            }
            break;
        case STMT_EXPR:
            typecheckExpr(typ, NODE_AS(ASTExprStmt, stmt)->expression);
            break;
        // Defer nodes
        case STMT_DEFER:
            typecheckStmt(typ, NODE_AS(ASTDeferStmt, stmt)->body);
            break;
        default:
            UNREACHABLE();
    }
}

static void typecheckFunction(Typechecker *typ, ASTObj *fn) {
    // TODO: control flow
    typ->current.function = fn;
    typecheckStmt(typ, NODE_AS(ASTStmtNode, fn->as.fn.body));
    typ->current.function = NULL;
}

static bool typecheckCurrentScope(Typechecker *typ);
static void typecheckStruct(Typechecker *typ, ASTObj *st) {
    enterScope(typ, st->as.structure.scope);
    // TODO: check for recursive structs.
    typecheckCurrentScope(typ);
    leaveScope(typ);
}

// Note: here scope means namespace scope (SCOPE_DEPTH_MODULE_NAMESPACE and SCOPE_DEPTH_STRUCT).
static bool typecheckCurrentScope(Typechecker *typ) {
    Array objects; // Array<ASTObj *>
    arrayInitSized(&objects, scopeGetNumObjects(getCurrentScope(typ)));
    scopeGetAllObjects(getCurrentScope(typ), &objects);

    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        switch(obj->type) {
            case OBJ_VAR:
                // nothing. vars are typechecked as they are declared, assigned.
                break;
            case OBJ_FN:
                typecheckFunction(typ, obj);
                break;
            case OBJ_STRUCT:
                typecheckStruct(typ, obj);
                break;
            default:
                UNREACHABLE();
        }
    }

    arrayFree(&objects);

    // Note: no need to typecheck child scopes. They are typechecked as necessary when entered.

    return true;
}

static void typecheckModule(Typechecker *typ, ASTModule *module) {
    typ->current.module = module;
    typ->current.scope = module->moduleScope;

    ARRAY_FOR(i, module->variableDecls) {
        ASTVarDeclStmt *decl = ARRAY_GET_AS(ASTVarDeclStmt *, &module->variableDecls, i);
        typecheckVariableDecl(typ, decl);
    }
    if(typ->hadError) {
        // equivalent of a return for the purpos of control flow.
        goto typecheck_module_end;
    }

    typecheckCurrentScope(typ);

typecheck_module_end:
    typ->current.module = NULL;
    typ->current.scope = NULL;
}

bool typecheckerTypecheck(Typechecker *typechecker, ASTProgram *prog) {
    VERIFY(prog);
    typechecker->program = prog;

    ARRAY_FOR(i, prog->modules) {
        ASTModule *module = ARRAY_GET_AS(ASTModule *, &prog->modules, i);
        typecheckModule(typechecker, module);
    }
    return !typechecker->hadError;
}
