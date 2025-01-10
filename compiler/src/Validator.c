#include <stdarg.h>
#include <string.h>
#include "common.h"
#include "memory.h"
#include "Error.h"
#include "Strings.h"
#include "Ast/Ast.h"
#include "Validator.h"

void validatorInit(Validator *v, Compiler *c) {
    v->parsedProgram = v->checkedProgram = NULL;
    v->compiler = c;
    v->hadError = false;
    v->current.checkedScope = v->current.parsedScope = NULL;
    v->current.module = 0;
}

void validatorFree(Validator *v) {
    // Set everything to 0 (NULL/false)
    memset(v, 0, sizeof(*v));
}

static void enterScope(Validator *v, Scope *parsedScope) {
    Scope *sc = scopeNew(v->current.checkedScope, parsedScope->depth);
    scopeAddChild(v->current.checkedScope, sc);
    v->current.parsedScope = parsedScope;
    v->current.checkedScope = sc;
}

static void leaveScope(Validator *v) {
    VERIFY(v->current.parsedScope->parent != NULL);
    VERIFY(v->current.checkedScope->parent != NULL);
    v->current.parsedScope = v->current.parsedScope->parent;
    v->current.checkedScope = v->current.checkedScope->parent;
}

static inline Scope *getCurrentParsedScope(Validator *v) {
    return v->current.parsedScope;
}

static inline Scope *getCurrentCheckedScope(Validator *v) {
    return v->current.checkedScope;
}

static inline ASTModule *getCurrentParsedModule(Validator *v) {
    return astProgramGetModule(v->parsedProgram, v->current.module);
}

static inline ASTModule *getCurrentCheckedModule(Validator *v) {
    return astProgramGetModule(v->checkedProgram, v->current.module);
}

static inline Allocator *getCurrentAllocator(Validator *v) {
    return &getCurrentCheckedModule(v)->ast_allocator.alloc;
}

// Note: returned type is guaranteed to exist (not be NULL).
static Type *getType(Validator *v, const char *name) {
    Type *ty = astModuleGetType(getCurrentCheckedModule(v), name);
    VERIFY(ty);
    return ty;
}

// if !expr, returns NULL. otherwise expands to said result.
#define TRY(type, expr) ({ \
        type _tmp = (expr); \
        if(!_tmp) { \
            return NULL; \
        } \
        _tmp;})

static void add_error(Validator *v, bool has_location, Location loc, ErrorType type, const char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, type, has_location, loc, message);
    compilerAddError(v->compiler, err);
    v->hadError = true;
}

static void error(Validator *v, Location loc, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String msg = stringVFormat(format, ap);
    va_end(ap);

    add_error(v, true, loc, ERR_ERROR, msg);
    stringFree(msg);
}

static void hint(Validator *v, Location loc, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String msg = stringVFormat(format, ap);
    va_end(ap);

    add_error(v, true, loc, ERR_HINT, msg);
    stringFree(msg);
}

static Type *exprDataType(Validator *v, ASTExprNode *expr) {
    if(!expr) {
        return NULL;
    }
    switch(expr->type) {
        case EXPR_NUMBER_CONSTANT:
            // FIXME: should be i64 (??)
            return getType(v, "i32");
        case EXPR_STRING_CONSTANT:
            return getType(v, "str");
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            return NODE_AS(ASTObjExpr, expr)->obj->dataType;
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
            // Type of a binary expression is the type of the left side.
            return exprDataType(v, NODE_AS(ASTBinaryExpr, expr)->lhs);
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF:
            // Type of unary expression is the type of the operand
            return exprDataType(v, NODE_AS(ASTUnaryExpr, expr)->operand);
        case EXPR_CALL:
            // Type of call expression is the return type of the callee.
            // FIXME: this will return the FUNCTION type, not the return type.
            return exprDataType(v, NODE_AS(ASTCallExpr, expr)->callee);
        default:
            UNREACHABLE();
    }
}

static Type *validateType(Validator *v, Type *parsedTy) {
    Type *checkedTy = NULL;
    // TODO: identifier types.
    switch(parsedTy->type) {
        case TY_VOID:
        case TY_I32:
            // Primitive types. Nothing to do. Simply make a new checked type and copy over the same data.
            checkedTy = typeNew(parsedTy->type, parsedTy->name, parsedTy->declLocation, parsedTy->declModule);
            break;
        case TY_POINTER:
            LOG_ERR("[Validator]: pointer types not supported yet.");
            UNREACHABLE();
        case TY_FUNCTION:
            checkedTy = typeNew(TY_FUNCTION, parsedTy->name, parsedTy->declLocation, parsedTy->declModule);
            ARRAY_FOR(i, parsedTy->as.fn.parameterTypes) {
                Type *paramType = validateType(v, ARRAY_GET_AS(Type *, &parsedTy->as.fn.parameterTypes, i));
                if(paramType) {
                    arrayPush(&checkedTy->as.fn.parameterTypes, (void *)paramType);
                }
            }
            checkedTy->as.fn.returnType = validateType(v, parsedTy->as.fn.returnType);
            break;
        case TY_STRUCT:
            LOG_ERR("[Validator]: struct types not supported yet.");
            // fallthrough
        default:
            UNREACHABLE();
    }
    // Note: can't use getType() since it fails if the type doesn't exist.
    if(astModuleGetType(getCurrentCheckedModule(v), checkedTy->name) == NULL) {
        astModuleAddType(getCurrentCheckedModule(v), checkedTy);
    } else {
        Type *ty = getType(v, checkedTy->name);
        typeFree(checkedTy);
        checkedTy = ty;
    }
    return checkedTy;
}

static void validate_type_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    Validator *v = (Validator *)cl;
    Type *ty = (Type *)item->value;
    validateType(v, ty); // Note: validateType() adds the type to the checked module.
}

static ASTExprNode *validateExpr(Validator *v, ASTExprNode *expr) {
    ASTExprNode *checkedExpr = NULL;
    switch(expr->type) {
        case EXPR_NUMBER_CONSTANT:
        case EXPR_STRING_CONSTANT:
            checkedExpr = (ASTExprNode *)astConstantValueExprNew(getCurrentAllocator(v), expr->type, expr->location, exprDataType(v, expr));
            break;
        // Obj nodes
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            UNREACHABLE();
        // Binary nodes
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
        case EXPR_GE: {
            ASTExprNode *lhs = validateExpr(v, NODE_AS(ASTBinaryExpr, expr)->lhs);
            ASTExprNode *rhs = validateExpr(v, NODE_AS(ASTBinaryExpr, expr)->rhs);
            checkedExpr = (ASTExprNode *)astBinaryExprNew(getCurrentAllocator(v), expr->type, expr->location, NULL, lhs, rhs);
            checkedExpr->dataType = exprDataType(v, checkedExpr);
            break;
        }
        // Unary nodes
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF: {
            ASTExprNode *operand = validateExpr(v, NODE_AS(ASTUnaryExpr, expr)->operand);
            checkedExpr = (ASTExprNode *)astUnaryExprNew(getCurrentAllocator(v), expr->type, expr->location, NULL, operand);
            checkedExpr->dataType = exprDataType(v, checkedExpr);
            break;
        }
        // Call node
        case EXPR_CALL: {
            ASTCallExpr *call = NODE_AS(ASTCallExpr, expr);
            ASTExprNode *callee = validateExpr(v, call->callee);
            Array checkedArguments;
            arrayInitSized(&checkedArguments, arrayLength(&call->arguments));
            bool hadError = false;
            ARRAY_FOR(i, call->arguments) {
                ASTExprNode *arg = ARRAY_GET_AS(ASTExprNode *, &call->arguments, i);
                arg = validateExpr(v, arg);
                if(arg) {
                    arrayPush(&checkedArguments, (void *)arg);
                } else {
                    hadError = true;
                }
            }
            if(hadError || !callee) {
                arrayFree(&checkedArguments);
                break;
            }
            checkedExpr = (ASTExprNode *)astCallExprNew(getCurrentAllocator(v), expr->location, exprDataType(v, callee), callee, &checkedArguments);
            arrayFree(&checkedArguments);
        }
        break;
        // Other nodes.
        case EXPR_IDENTIFIER:
            // TODO: EXPR_IDENT, EXPR_CALL
        default:
            UNREACHABLE();
    }
    return checkedExpr;
}

static ASTStmtNode *validateVariableDecl(Validator *v, ASTVarDeclStmt *varDecl);
static ASTStmtNode *validateStmt(Validator *v, ASTStmtNode *stmt) {
    ASTStmtNode *checkedStmt = NULL;
    switch(stmt->type) {
        // VarDecl nodes
        case STMT_VAR_DECL:
            // Note: no need for TRY() since if validate...() returns NULL, we simply also return NULL.
            checkedStmt = validateVariableDecl(v, NODE_AS(ASTVarDeclStmt, stmt));
            break;
        // Block nodes
        case STMT_BLOCK: {
            bool hadError = false;
            ASTBlockStmt *block = NODE_AS(ASTBlockStmt, stmt);
            Array checkedNodes;
            arrayInitSized(&checkedNodes, arrayLength(&block->nodes));
            enterScope(v, block->scope);
            ARRAY_FOR(i, block->nodes) {
                ASTStmtNode *n = ARRAY_GET_AS(ASTStmtNode *, &block->nodes, i);
                n = validateStmt(v, n);
                if(n) {
                    arrayPush(&checkedNodes, (void *)n);
                } else {
                    hadError = true;
                }
            }
            Scope *scope = getCurrentCheckedScope(v);
            leaveScope(v);
            if(hadError) {
                arrayFree(&checkedNodes);
                break;
            }
            checkedStmt = (ASTStmtNode *)astBlockStmtNew(getCurrentAllocator(v), stmt->location, scope, &checkedNodes);
            arrayFree(&checkedNodes);
        }
        break;
        // Conditional nodes
        case STMT_IF:
        // Loop nodes
        case STMT_LOOP:
        // Expr nodes
        case STMT_RETURN:
        case STMT_EXPR:
        // Defer nodes
        case STMT_DEFER:
        default:
            UNREACHABLE();
    }
    return checkedStmt;
}

static ASTStmtNode *validateVariableDecl(Validator *v, ASTVarDeclStmt *varDecl) {
    if(scopeHasObject(getCurrentCheckedScope(v), varDecl->variable)) {
        error(v, varDecl->variable->location, "Redeclaration of module variable '%s'", varDecl->variable->name);
        ASTObj *prevDecl = scopeGetObject(getCurrentParsedScope(v), OBJ_VAR, varDecl->variable->name);
        hint(v, prevDecl->location, "Previous declaration was here.");
        return NULL;
    }
    // If decl has an initializer expr, validate it.
    ASTExprNode *checkedInit = varDecl->initializer ? TRY(ASTExprNode *, validateExpr(v, varDecl->initializer)) : NULL;
    ASTVarDeclStmt *checkedDecl = astVarDeclStmtNew(getCurrentAllocator(v), varDecl->header.location, NULL, checkedInit);

    // If variable has a type, use it. Else attempt to infer the type from the initializer.
    // Note: if the initializer doesn't exist (is NULL), exprDataType() will return NULL.
    Type *varTy = varDecl->variable->dataType ? varDecl->variable->dataType : exprDataType(v, checkedInit);
    if(varTy == NULL) {
        error(v, varDecl->variable->location, "Cannot infer type of variable '%s'.");
        hint(v, varDecl->header.location, "Consider adding an explicit type%s.", checkedInit ? "" : " or initializer");
        return NULL;
    }
    ASTObj *checkedObj = astModuleNewObj(getCurrentCheckedModule(v), OBJ_VAR, varDecl->variable->location, varDecl->variable->name, varTy);
    checkedDecl->variable = checkedObj;
    return NODE_AS(ASTStmtNode, checkedDecl);
}

static void validateFunction(Validator *v, ASTObj *fn) {
    VERIFY(fn->type == OBJ_FN);

    bool hasBadParameter = false;
    ARRAY_FOR(i, fn->as.fn.parameters) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        if(param->dataType == NULL) {
            error(v, param->location, "Parameters '%s' has no type.", param->name);
            hasBadParameter = true;
        } else {
            param->dataType = getType(v, param->dataType->name);
        }
    }
    if(hasBadParameter) {
        return;
    }
    fn->as.fn.returnType = getType(v, fn->as.fn.returnType->name);

    ASTStmtNode *body = validateStmt(v, NODE_AS(ASTStmtNode, fn->as.fn.body));
    if(!body) {
        // Note: can't use TRY() since function doesn't return anything.
        return;
    }
    fn->as.fn.body = NODE_AS(ASTBlockStmt, body);
}

static void validateScope(Validator *v, Scope *scope) {
    Array objects;
    arrayInit(&objects);
    scopeGetAllObjects(scope, &objects);
    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        switch(obj->type) {
            case OBJ_VAR:
                break;
            case OBJ_FN:
                validateFunction(v, obj);
                break;
            default:
                UNREACHABLE();
        }
        scopeAddObject(getCurrentCheckedScope(v), obj);
    }
    arrayFree(&objects);
}

static void validateModule(Validator *v, ModuleID moduleID) {
    ModuleID checkedID = astProgramNewModule(v->checkedProgram, getCurrentParsedModule(v)->name);
    VERIFY(checkedID == moduleID);
    v->current.module = checkedID;
    v->current.parsedScope = getCurrentParsedModule(v)->moduleScope;
    v->current.checkedScope = getCurrentCheckedModule(v)->moduleScope;
    // Validate all types.
    tableMap(&getCurrentParsedModule(v)->types, validate_type_callback, (void *)v);

    // Validate all variable declarations.
    ARRAY_FOR(i, getCurrentParsedModule(v)->variableDecls) {
        ASTStmtNode *validatedDecl = validateVariableDecl(v, ARRAY_GET_AS(ASTVarDeclStmt *, &getCurrentParsedModule(v)->variableDecls, i));
        // TODO: error handling
        if(validatedDecl) {
            arrayPush(&getCurrentCheckedModule(v)->variableDecls, (void *)validatedDecl);
        }
    }

    // Validate the module scope.
    // TODO: error handling.
    validateScope(v, getCurrentParsedModule(v)->moduleScope);
    v->current.checkedScope = NULL;
}

bool validatorValidate(Validator *v, ASTProgram *parsedProg, ASTProgram *checkedProg) {
    VERIFY(parsedProg);
    VERIFY(checkedProg);
    v->parsedProgram = parsedProg;
    v->checkedProgram = checkedProg;
    // Validate all modules.
    ARRAY_FOR(i, parsedProg->modules) {
        // TODO: error handling
        validateModule(v, i);
    }
    return !v->hadError;
}

#undef TRY
