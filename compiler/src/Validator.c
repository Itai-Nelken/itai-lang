#include <stdarg.h>
#include <string.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Table.h"
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

// Notes: * Typename MUST be valid (C.R.E).
//        * Returned type is guaranteed to exist (not be NULL, C.R.E).
static Type *getType(Validator *v, const char *name) {
    Type *ty = astModuleGetType(getCurrentCheckedModule(v), name);
    VERIFY(ty);
    return ty;
}

// if !expr, returns NULL. otherwise expands to of expr.
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

// Find an ASTObj that is visible in the current scope (i.e. is in it or a parent scope.)
// depth: if not NULL, the variable pointed to will be set to the scope depth of the return object.
static ASTObj *findObjVisibleInCurrentScope(Validator *v, ASTString name, ScopeDepth *depth) {
    Scope *sc = getCurrentCheckedScope(v);
    while(sc != NULL) {
        ASTObj *obj = scopeGetAnyObject(sc, name);
        if(obj) {
            if(depth) {
                *depth = sc->depth;
            }
            return obj;
        }
        sc = sc->parent;
    }
    return NULL;
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
            return NODE_AS(ASTObjExpr, expr)->obj->dataType;
        case EXPR_FUNCTION:
            return NODE_AS(ASTObjExpr, expr)->obj->as.fn.returnType;
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
            return exprDataType(v, NODE_AS(ASTCallExpr, expr)->callee);
        default:
            UNREACHABLE();
    }
}

// Notes:
//   * C.R.E for [parsedType] to be NULL.
//   * If the [parsedType] was already validated, the previously validated type will be returned.
static Type *validateType(Validator *v, Type *parsedType) {
    VERIFY(parsedType);
    // * pointer types -> validate pointee
    // * fn types -> validate param types + return type
    // * struct types -> TODO
    // * id types -> the struct type they refer to.

    Type *checkedType = NULL;
    if((checkedType = astModuleGetType(getCurrentCheckedModule(v), parsedType->name)) != NULL) {
        return checkedType;
    }

    switch(parsedType->type) {
        case TY_VOID:
        case TY_I32:
        //case TY_U32:
        //case TY_STR:
            // Primitive types. Nothing to validate.
            checkedType = typeNew(parsedType->type, parsedType->name, parsedType->declLocation, v->current.module);
            break;
        case TY_POINTER: {
            Type *checkedPointee = TRY(Type *, validateType(v, parsedType->as.ptr.innerType));
            checkedType = typeNew(TY_POINTER, parsedType->name, parsedType->declLocation, v->current.module);
            checkedType->as.ptr.innerType = checkedPointee;
            break;
        }
        case TY_FUNCTION: {
            bool hadBadParameter = false;
            Array validatedParameterTypes;
            arrayInitSized(&validatedParameterTypes, arrayLength(&parsedType->as.fn.parameterTypes));
            ARRAY_FOR(i, parsedType->as.fn.parameterTypes) {
                Type *parsedParamType = ARRAY_GET_AS(Type *, &parsedType->as.fn.parameterTypes, i);
                Type *checkedParamType = validateType(v, parsedParamType);
                if(checkedParamType) {
                    arrayPush(&validatedParameterTypes, (void *)checkedParamType);
                } else {
                    hadBadParameter = true;
                }
            }

            Type *checkedReturnType = validateType(v, parsedType->as.fn.returnType);
            if(!checkedReturnType || hadBadParameter) {
                // Clean up (prevent memory leaks.)
                ARRAY_FOR(i, validatedParameterTypes) {
                    typeFree(ARRAY_GET_AS(Type *, &validatedParameterTypes, i));
                }
                arrayFree(&validatedParameterTypes);
                break; // checkedType will be NULL, and so NULL will be returned.
            }

            checkedType = typeNew(TY_FUNCTION, parsedType->name, parsedType->declLocation, v->current.module);
            arrayCopy(&checkedType->as.fn.parameterTypes, &validatedParameterTypes);
            checkedType->as.fn.returnType = checkedReturnType;
            arrayFree(&validatedParameterTypes);
            break;
        }
        case TY_STRUCT:
            LOG_ERR("Validator: Struct types are not supported yet.");
            // fallthrough
        default:
            UNREACHABLE();
    }

    VERIFY(checkedType != NULL);
    astModuleAddType(getCurrentCheckedModule(v), checkedType);
    return checkedType;
}

// Note: C.R.E for [parsedExpr] to be NULL.
static ASTExprNode *validateExpr(Validator *v, ASTExprNode *parsedExpr) {
    VERIFY(parsedExpr);
    // 1. For each node, do (depending on type):
    //      IDENTIFIER: replace with ASTObj the identifier refers to.
    //      ASSIGN: make sure assignment target is valid (variable - including property access, dereference (I think).)
    //      CALL: check that callee is callable, validate argument expressions + correct amount of them.
    // 2. Also set the data type for each node.

    ASTExprNode *checkedExpr = NULL;
    switch(parsedExpr->type) {
        // Constant value nodes.
        case EXPR_NUMBER_CONSTANT:
            // FIXME: Can I call exprDataType() on an unvalidated expr? I think it will work in this specific case,
            //        but is it something I should allow?
            checkedExpr = (ASTExprNode *)astConstantValueExprNew(getCurrentAllocator(v), EXPR_NUMBER_CONSTANT, parsedExpr->location, exprDataType(v, parsedExpr));
            NODE_AS(ASTConstantValueExpr, checkedExpr)->as.number = NODE_AS(ASTConstantValueExpr, parsedExpr)->as.number;
            break;
        case EXPR_STRING_CONSTANT:
            // FIXME: Can I call exprDataType() on an unvalidated expr? I think it will work in this specific case,
            //        but is it something I should allow?
            checkedExpr = (ASTExprNode *)astConstantValueExprNew(getCurrentAllocator(v), EXPR_STRING_CONSTANT, parsedExpr->location, exprDataType(v, parsedExpr));
            NODE_AS(ASTConstantValueExpr, checkedExpr)->as.string = NODE_AS(ASTConstantValueExpr, parsedExpr)->as.string;
            break;
        // Obj nodes
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
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
        case EXPR_GE:
        // Unary nodes
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF:
        // Call node
        case EXPR_CALL:
            UNREACHABLE();
        // Other nodes.
        case EXPR_IDENTIFIER: {
            // TODO: I think I can remove the last parameter from this function, since with the new
            //        design of the validator the checked scopes track whether a local variable has
            //        already been declared.
            ASTString name = NODE_AS(ASTIdentifierExpr, parsedExpr)->id;
            ASTObj *obj = findObjVisibleInCurrentScope(v, name, NULL);
            if(!obj) {
                error(v, parsedExpr->location, "Identifier '%s' doesn't exist.", name);
                break;
            }
            // FIXME: if I add more obj node types, the logic for expr type below will break.
            checkedExpr = (ASTExprNode *)astObjExprNew(getCurrentAllocator(v), obj->type == OBJ_VAR ? EXPR_VARIABLE : EXPR_FUNCTION, parsedExpr->location, obj);
            checkedExpr->dataType = obj->dataType;
            break;
        }
        default:
            UNREACHABLE();
    }

    return checkedExpr;
}


static ASTVarDeclStmt *validateVariableDecl(Validator *v, ASTVarDeclStmt *parsedVarDecl);
// Note: C.R.E for [parsedStmt] to be NULL.
static ASTStmtNode *validateStmt(Validator *v, ASTStmtNode *parsedStmt) {
    VERIFY(parsedStmt);
    ASTStmtNode *checkedStmt = NULL;
    switch(parsedStmt->type) {
        // VarDecl nodes
        case STMT_VAR_DECL:
            // Note: validateVariableDecl() also validates the object.
            checkedStmt = NODE_AS(ASTStmtNode, validateVariableDecl(v, NODE_AS(ASTVarDeclStmt, parsedStmt)));
            break;
        // Block nodes
        case STMT_BLOCK: {
            Array checkedNodes;
            arrayInitSized(&checkedNodes, arrayLength(&NODE_AS(ASTBlockStmt, parsedStmt)->nodes));
            enterScope(v, NODE_AS(ASTBlockStmt, parsedStmt)->scope);
            bool hadError = false;
            // Note: if I support closures/lambdas, validateCurrentScope() shuld be called here.
            ARRAY_FOR(i, NODE_AS(ASTBlockStmt, parsedStmt)->nodes) {
                ASTStmtNode *parsedNode = ARRAY_GET_AS(ASTStmtNode *, &NODE_AS(ASTBlockStmt, parsedStmt)->nodes, i);
                ASTStmtNode *checkedNode = validateStmt(v, parsedNode);
                if(checkedNode) {
                    arrayPush(&checkedNodes, (void *)checkedNode);
                } else {
                    hadError = true;
                }
            }
            Scope *checkedScope = getCurrentCheckedScope(v);
            leaveScope(v);

            if(hadError) {
                arrayFree(&checkedNodes);
                break;
            }
            checkedStmt = NODE_AS(ASTStmtNode, astBlockStmtNew(getCurrentAllocator(v), parsedStmt->location, checkedScope, &checkedNodes));
            arrayFree(&checkedNodes);
            break;
        }
        // Conditional nodes
        case STMT_IF:
        // Loop nodes
        case STMT_LOOP:
            UNREACHABLE();
        // Expr nodes
        case STMT_RETURN:
        case STMT_EXPR: {
            ASTExprNode *checkedExpr = TRY(ASTExprNode *, validateExpr(v, NODE_AS(ASTExprStmt, parsedStmt)->expression));
            checkedStmt = NODE_AS(ASTStmtNode, astExprStmtNew(getCurrentAllocator(v), parsedStmt->type, parsedStmt->location, checkedExpr));
            break;
        }
        // Defer nodes
        case STMT_DEFER:
        default:
            UNREACHABLE();
    }

    return checkedStmt;
}

// Notes:
//  * C.R.E for [parsedVarDecl] to be NULL.
//  * Adds the checked variable object to the current scope.
static ASTVarDeclStmt *validateVariableDecl(Validator *v, ASTVarDeclStmt *parsedVarDecl) {
    VERIFY(parsedVarDecl);
    // validate initializer (if it exists.)
    // infer type (if necessary.)
    // make sure there is a type (that is not void.)
    // Make sure not assigned to itself (above two checks should catch this error.)

    ASTExprNode *checkedInitializer = NULL;
    if(parsedVarDecl->initializer) {
        checkedInitializer = TRY(ASTExprNode *, validateExpr(v, parsedVarDecl->initializer));
    }

    if(NODE_IS(checkedInitializer, EXPR_VARIABLE) &&
       NODE_AS(ASTObjExpr, checkedInitializer)->obj->name == parsedVarDecl->variable->name) {
        error(v, parsedVarDecl->header.location, "Variable assigned to itself in declaration.");
        return NULL;
    }

    // Note: typechecking is NOT done here.
    Type *dataType = parsedVarDecl->variable->dataType;
    dataType = dataType ? dataType : checkedInitializer->dataType;
    if(dataType == NULL) {
        error(v, parsedVarDecl->variable->location, "Cannot infer type of variable '%s'.", parsedVarDecl->variable->name);
        hint(v, parsedVarDecl->header.location, "Consider adding an explicit type%s.", checkedInitializer ? "" : " or an initializer");
        return NULL;
    }
    if(dataType->type == TY_VOID) {
        error(v, parsedVarDecl->variable->location, "A variable cannot have the type 'void'.");
        return NULL;
    }

    ASTObj *checkedObj = astModuleNewObj(getCurrentCheckedModule(v), OBJ_VAR, parsedVarDecl->variable->location, parsedVarDecl->variable->name, dataType);
    bool added = scopeAddObject(getCurrentCheckedScope(v), checkedObj);
    VERIFY(added == true);
    ASTVarDeclStmt *checkedVarDecl = astVarDeclStmtNew(getCurrentAllocator(v), parsedVarDecl->header.location, checkedObj, checkedInitializer);
    return checkedVarDecl;
}

// Note: C.R.E for [fn] to be NULL.
static ASTObj *validateFunction(Validator *v, ASTObj *fn) {
    // Note: (parsed) fn scope already contains params. They are added by the parser.
    Type *checkedReturnType = TRY(Type *, validateType(v, fn->as.fn.returnType));
    Type *checkedFnType = TRY(Type *, validateType(v, fn->dataType));
    // Create the object now so we can put any data in it and then not free it (since the object is owned by the module.)
    ASTObj *checkedFn = astModuleNewObj(getCurrentCheckedModule(v), OBJ_FN, fn->location, fn->name, checkedFnType);
    checkedFn->as.fn.returnType = checkedReturnType;
    bool hadError = false;
    ARRAY_FOR(i, fn->as.fn.parameters) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        Type *checkedParamType = validateType(v, param->dataType);
        if(checkedParamType) {
            ASTObj *checkedParam = astModuleNewObj(getCurrentCheckedModule(v), OBJ_VAR, param->location, param->name, checkedParamType);
            arrayPush(&checkedFn->as.fn.parameters, (void *)checkedParam);
        } else {
            hadError = true;
        }
    }
    if(hadError) {
        return NULL;
    }
    ASTStmtNode *checkedBody = TRY(ASTStmtNode *, validateStmt(v, NODE_AS(ASTStmtNode, fn->as.fn.body)));
    checkedFn->as.fn.body = NODE_AS(ASTBlockStmt, checkedBody);

    return checkedFn;
}

// Note: C.R.E for parsedObj to be NULL.
static ASTObj *validateObject(Validator *v, ASTObj *parsedObj) {
    VERIFY(parsedObj);
    ASTObj *checkedObj = NULL;
    switch(parsedObj->type) {
        case OBJ_VAR:
            // OBJ_VAR's should be validated when the variable they refer to is declared.
            // Hence they are NOT allowed here.
            LOG_ERR("OBJ_VAR not allowed in validateObject()");
            UNREACHABLE();
        case OBJ_FN:
            checkedObj = validateFunction(v, parsedObj);
            break;
        //case OBJ_STRUCT:
        //case OBJ_ENUM?:
        default:
            UNREACHABLE();
    }

    return checkedObj;
}

// Note: scope here refers to a namespace type scope (i.e. SCOPE_DEPTH_MODULE_NAMESPACE or SCOPE_DEPTH_STRUCT)
static bool validateCurrentScope(Validator *v) {
    VERIFY(v->current.parsedScope);

    Array objectsInScope;
    // TODO: change size calculation as more tables are added to scope (for structs, enums? etc.)
    usize numObjects = tableSize(&getCurrentParsedScope(v)->variables) + tableSize(&getCurrentParsedScope(v)->functions);
    arrayInitSized(&objectsInScope, numObjects);
    scopeGetAllObjects(getCurrentParsedScope(v), &objectsInScope);

    bool hadError = false;
    ARRAY_FOR(i, objectsInScope) {
        ASTObj *parsedObj = ARRAY_GET_AS(ASTObj *, &objectsInScope, i);
        // TODO: Note: OBJ_VARs will be validated as the variables are declared (in validateStmt() probably.)
        if(parsedObj->type != OBJ_VAR) {
            ASTObj *checkedObj = validateObject(v, parsedObj);
            if(checkedObj) {
                scopeAddObject(getCurrentCheckedScope(v), checkedObj);
            } else {
                hadError = true;
            }
        }
    }
    arrayFree(&objectsInScope);

    // Note: no need to validate child scopes. They should be validated as necessary
    //       when entered.

    return !hadError;
}

static void validate_type_callback(TableItem *item, bool is_last, void *validator) {
    UNUSED(is_last);
    Validator *v = (Validator *)validator;
    Type *parsedType = (Type *)item->value;
    validateType(v, parsedType); // validateType() adds the type to the current module.
}

// Notes:
//   * The new module is created with astProgramNewModule(checked_prog), and so there is no need to return it.
//   * C.R.E for moduleID > amount of modules in parsed module.
//   * In case of any errors, v.hadError will be set. Use it to check for errors.
static void validateModule(Validator *v, ModuleID moduleID) {
    // Is the moduleID valid (withing the range of existing modules)?
    VERIFY(moduleID < arrayLength(&v->parsedProgram->modules));
    // validate:
    //     types
    //     variable declarations (module scoppe)
    //     scopes

    // Preliminary initialization, setting up of validator state.
    ASTModule *parsedModule = astProgramGetModule(v->parsedProgram, moduleID);
    ModuleID checkedModuleID = astProgramNewModule(v->checkedProgram, parsedModule->name);
    VERIFY(moduleID == checkedModuleID); // This should work the way this function is called. But its not nice code.
    ASTModule *checkedModule = astProgramGetModule(v->checkedProgram, checkedModuleID);
    v->current.module = moduleID;
    v->current.parsedScope = parsedModule->moduleScope;
    v->current.checkedScope = checkedModule->moduleScope;

    tableMap(&parsedModule->types, validate_type_callback, (void *)v);

    ARRAY_FOR(i, parsedModule->variableDecls) {
        ASTVarDeclStmt *parsedVarDecl = ARRAY_GET_AS(ASTVarDeclStmt *, &parsedModule->variableDecls, i);
        // Note: validateVariableDecl() also validates the objects.
        ASTVarDeclStmt *checkedVarDecl = validateVariableDecl(v, parsedVarDecl);
        if(checkedVarDecl) {
            arrayPush(&checkedModule->variableDecls, (void *)checkedVarDecl);
        }
    }
    // Notes to clear my confusion:
    // * When validating a scope, we DON'T have enought info to validate OBJ_VARs.
    //   - Hence we validate them as we encounter them, which doesn't cause any problem for locals
    //     since we can't use them before they are declared.
    //   - Since we validate OBJ_VARs as we encounter them, validateObject() just skips them and
    //     that means we can validate "globals" before the scope (but after the types).
    // * When is validateCurrentScope() called then?
    //   - Right now, when validating a module only. In the future, they will be called on structs
    //     and on enums (which have methods.)
    //   - In any case, it should NOT be called in enterScope() since the "scope" it validates is actually a
    //     namespace, and enterScope() is also used for block scopes which ONLY have variables.
    //   - If I decide to allow lambdas/closures, then it will have to be called on functions as well.

    // The module scope is already "entered" because we set the current scope to the module scope.
    if(!validateCurrentScope(v)) {
        return;
    }

    // See notes above declaration of this function on how caller should check for errors.
}

bool validatorValidate(Validator *v, ASTProgram *parsedProg, ASTProgram *checkedProg) {
    VERIFY(parsedProg);
    VERIFY(checkedProg);
    v->parsedProgram = parsedProg;
    v->checkedProgram = checkedProg;
    // Validate all modules.
    ARRAY_FOR(i, parsedProg->modules) {
        validateModule(v, i);
    }
    // If we had any error while parsing any of the modules, fail.
    return !v->hadError;
}

#undef TRY
