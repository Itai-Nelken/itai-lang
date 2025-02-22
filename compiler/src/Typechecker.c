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
    typechecker->foundMain = false;
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

static bool checkTypes(Typechecker *typ, Location errLoc, Type *expected, Type *actual) {
    // ASTStrings can be compared by address (since they are unique).
    if(!expected || !actual || expected->name != actual->name) {
        error(typ, errLoc, "Type mismatch: expected '%s' but got '%s'.", typeName(expected), typeName(actual));
        return false;
    }
    return true;
}

static void typecheckExpr(Typechecker *typ, ASTExprNode *expr);
static void typecheckVariableDecl(Typechecker *typ, ASTVarDeclStmt *decl) {
    if(decl->variable->dataType->type == TY_POINTER) {
        error(typ, decl->header.location, "Pointer types are not allowed in this context.");
        return;
    }
    if(decl->initializer) {
        typecheckExpr(typ, decl->initializer);
        if(!checkTypes(typ, decl->header.location, decl->variable->dataType, decl->initializer->dataType)) {
            return;
        }
    }
}

static void typecheckExpr(Typechecker *typ, ASTExprNode *expr) {
    VERIFY(expr);
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
        case EXPR_NOT:
        case EXPR_ADDROF:
        case EXPR_DEREF:
            typecheckExpr(typ, NODE_AS(ASTUnaryExpr, expr)->operand);
            if(NODE_IS(expr, EXPR_NOT)) {
                checkTypes(typ, NODE_AS(ASTUnaryExpr, expr)->operand->location, astModuleGetType(getCurrentModule(typ), "bool"), NODE_AS(ASTUnaryExpr, expr)->operand->dataType);
            }
            // TODO: check if negatable, addrofable type. (derefable checked in validator.)
            break;
        // Call node
        case EXPR_CALL: {
            ASTCallExpr *call = NODE_AS(ASTCallExpr, expr);
            Type *calleeType = call->callee->dataType;
            // Guaranteed to be callable or validator is broken.
            VERIFY(calleeType->type == TY_FUNCTION);
            Array *parameterTypes = &calleeType->as.fn.parameterTypes;
            // Guaranteed to be true or validator is broken.
            VERIFY(arrayLength(parameterTypes) == arrayLength(&call->arguments));
            ARRAY_FOR(i, call->arguments) {
                ASTExprNode *arg = ARRAY_GET_AS(ASTExprNode *, &call->arguments, i);
                Type *paramType = ARRAY_GET_AS(Type *, parameterTypes, i);
                checkTypes(typ, arg->location, paramType, arg->dataType);
            }
            break;
        }
        // Other nodes.
        case EXPR_IDENTIFIER:
            // Identifier nodes should not exist at this stage.
        default:
            UNREACHABLE();
    }
}

static void typecheckStmt(Typechecker *typ, ASTStmtNode *stmt) {
    VERIFY(stmt);
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
        case STMT_EXPECT: {
            ASTConditionalStmt *conditionalStmt = NODE_AS(ASTConditionalStmt, stmt);
            typecheckExpr(typ, conditionalStmt->condition);
            if(conditionalStmt->then) { // expect stmt may not have a body.
                typecheckStmt(typ, conditionalStmt->then);
            }
            if(conditionalStmt->else_) {
                typecheckStmt(typ, conditionalStmt->else_);
            }
            checkTypes(typ, conditionalStmt->condition->location, astModuleGetType(getCurrentModule(typ), "bool"), conditionalStmt->condition->dataType);
            break;
        }
        // Loop nodes
        case STMT_LOOP: {
            ASTLoopStmt *loopStmt = NODE_AS(ASTLoopStmt, stmt);
            if(loopStmt->initializer) {
                typecheckStmt(typ, loopStmt->initializer);
            }
            typecheckExpr(typ, loopStmt->condition); // MUST exist.
            if(loopStmt->increment) {
                typecheckExpr(typ, loopStmt->increment);
            }
            typecheckStmt(typ, NODE_AS(ASTStmtNode, loopStmt->body)); // MUST exist.
            break;
        }
        // Expr nodes
        case STMT_RETURN:
            VERIFY(typ->current.function);
            if(NODE_AS(ASTExprStmt, stmt)->expression != NULL) {
                typecheckExpr(typ, NODE_AS(ASTExprStmt, stmt)->expression);
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

static bool isRecursiveStruct(Type *rootStructType, Type *fieldType) {
    VERIFY(rootStructType->type == TY_STRUCT);
    if(fieldType->type != TY_STRUCT) {
        return false;
    }
    ARRAY_FOR(i, fieldType->as.structure.fieldTypes) {
        Type *field = ARRAY_GET_AS(Type *, &fieldType->as.structure.fieldTypes, i);
        if(field->name == rootStructType->name || isRecursiveStruct(rootStructType, field)) {
            return true;
        }
    }
    return false;
}

static bool typecheckCurrentScope(Typechecker *typ, ASTObj *st);
static void typecheckStruct(Typechecker *typ, ASTObj *st) {
    enterScope(typ, st->as.structure.scope);
    VERIFY(st->dataType->type == TY_STRUCT);
    typecheckCurrentScope(typ, st);
    leaveScope(typ);
}

// Note: here scope means namespace scope (SCOPE_DEPTH_MODULE_NAMESPACE and SCOPE_DEPTH_STRUCT).
// st: OBJ_STRUCT for when the scope being validated is a struct scope.
static bool typecheckCurrentScope(Typechecker *typ, ASTObj *st) {
    Array objects; // Array<ASTObj *>
    arrayInitSized(&objects, scopeGetNumObjects(getCurrentScope(typ)));
    scopeGetAllObjects(getCurrentScope(typ), &objects);
    if(getCurrentScope(typ)->depth == SCOPE_DEPTH_STRUCT) {
        VERIFY(st);
    }

    bool hadError = false;
    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        switch(obj->type) {
            case OBJ_VAR:
                if(getCurrentScope(typ)->depth == SCOPE_DEPTH_STRUCT) {
                    if(isRecursiveStruct(st->dataType, obj->dataType)) {
                        error(typ, obj->location, "Struct '%s' cannot have a field that recursively contains it.", st->name);
                        hadError = true;
                    }
                }
                // nothing. vars are typechecked as they are declared, assigned.
                break;
            case OBJ_FN:
                if(getCurrentScope(typ)->depth == SCOPE_DEPTH_MODULE_NAMESPACE && stringEqual(obj->name, "main")) {
                    typ->foundMain = true;
                }
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

    return !hadError;
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

    typecheckCurrentScope(typ, NULL);

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
    if(!typechecker->foundMain) {
        // Since this error doesn't have a location, we need to manually create it.
        Error *err;
        NEW0(err);
        errorInit(err, ERR_ERROR, false, EMPTY_LOCATION, "No entry point (hint: Consider adding a 'main' function).");
        typechecker->hadError = true;
        compilerAddError(typechecker->compiler, err);
    }
    return !typechecker->hadError;
}
