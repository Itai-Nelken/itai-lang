#include <stdarg.h>
#include <string.h>
#include "Ast/ExprNode.h"
#include "Ast/Module.h"
#include "Ast/Object.h"
#include "Ast/Program.h"
#include "Ast/Scope.h"
#include "Ast/Type.h"
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
    v->current.function = NULL;
    v->current.module = 0;
    v->current.objParent = NULL;
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

static ASTModule *getImportedModule(Validator *v, ASTString name) {
    TableItem *item = tableGet(&getCurrentCheckedModule(v)->importedModules, (void *)name);
    if(item == NULL) {
        return NULL;
    }
    ModuleID mID = (ModuleID)item->value;
    // FIXME: module might not necessarily exist. Need to make "predeclare" all modules for this to work.
    //        This would be a similar solution to how I handle validated objects not necessarily existing yet.
    return astProgramGetModule(v->checkedProgram, mID);
}

// if !expr, returns NULL. otherwise expands to of expr.
// Undefed at end of file.
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

static inline const char *typeName(Type *ty) {
    if(ty == NULL) {
        return "none";
    }
    return (const char *)ty->name;
}

// TODO: I think I can remove the last parameter, since with the new
//        design of the validator the checked scopes track whether a local variable has
//        already been declared.
//
// Find an ASTObj that is visible in a scope (i.e. is in it or a parent scope.)
// depth: if not NULL, the variable pointed to will be set to the scope depth of the return object.
static ASTObj *findObjVisibleInScope(Scope *sc, ASTString name, ScopeDepth *depth) {
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

// same as above but uses current checked scope.
static inline ASTObj *findObjVisibleInCurrentScope(Validator *v, ASTString name, ScopeDepth *depth) {
    return findObjVisibleInScope(getCurrentCheckedScope(v), name, depth);
}

static Type *expr_data_type_complex(Validator *v, ASTExprNode *expr, bool isInCall) {
    if(!expr) {
        return NULL;
    }
    switch(expr->type) {
        case EXPR_NUMBER_CONSTANT:
            if(expr->dataType) {
                // For postfix types.
                VERIFY(expr->dataType->type == TY_I32 || expr->dataType->type == TY_U32);
                return expr->dataType;
            }
            // FIXME: should be i64 (??)
            return getType(v, "i32");
        case EXPR_STRING_CONSTANT:
            return getType(v, "str");
        case EXPR_BOOLEAN_CONSTANT:
            // Must be true since parser sets it.
            VERIFY(expr->dataType->type == TY_BOOL);
            return expr->dataType;
        case EXPR_VARIABLE: {
            Type *ty = NODE_AS(ASTObjExpr, expr)->obj->dataType;
            VERIFY(ty);
            if(isInCall && ty->type == TY_FUNCTION) {
                return ty->as.fn.returnType;
            }
            return ty;
        }
        case EXPR_FUNCTION:
            return NODE_AS(ASTObjExpr, expr)->obj->as.fn.returnType;
        case EXPR_PROPERTY_ACCESS:
            // The type of a property access expression is the type of the rightmost element.
            // For example, the type of 'a.b.c' is the type of 'c'.
            return expr_data_type_complex(v, NODE_AS(ASTBinaryExpr, expr)->rhs, isInCall);
        case EXPR_ASSIGN:
        case EXPR_ADD:
        case EXPR_SUBTRACT:
        case EXPR_MULTIPLY:
        case EXPR_DIVIDE:
            // Type of a binary expression is the type of the left side.
            return expr_data_type_complex(v, NODE_AS(ASTBinaryExpr, expr)->lhs, isInCall);
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
        case EXPR_LOGICAL_AND:
        case EXPR_LOGICAL_OR:
        case EXPR_LOGICAL_NOT: // Unary node, but fits here.
            // Type of conditional expression is boolean.
            return getType(v, "bool");
        case EXPR_ADDROF:
            VERIFY(expr->dataType); // In case called on parsed expr.
            return expr->dataType; // The pointer type.
        case EXPR_NEGATE:
            // Type of unary expression is the type of the operand
            return expr_data_type_complex(v, NODE_AS(ASTUnaryExpr, expr)->operand, isInCall);
        case EXPR_DEREF:
            VERIFY(expr->dataType);
            return expr->dataType;
        case EXPR_CALL:
            // Type of call expression is the return type of the callee.
            return expr_data_type_complex(v, NODE_AS(ASTCallExpr, expr)->callee, isInCall);
        default:
            UNREACHABLE();
    }
}

static inline Type *exprDataType(Validator *v, ASTExprNode *expr) {
    return expr_data_type_complex(v, expr, false);
}

// Notes:
//   * C.R.E for [parsedType] to be NULL.
//   * If the [parsedType] was already validated, the previously validated type will be returned.
static Type *validateType(Validator *v, Type *parsedType) {
    VERIFY(parsedType);

    Type *checkedType = NULL;
    if((checkedType = astModuleGetType(getCurrentCheckedModule(v), parsedType->name)) != NULL) {
        return checkedType;
    }

    switch(parsedType->type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
        case TY_BOOL:
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
                return NULL;
            }

            checkedType = typeNew(TY_FUNCTION, parsedType->name, parsedType->declLocation, v->current.module);
            arrayCopy(&checkedType->as.fn.parameterTypes, &validatedParameterTypes);
            checkedType->as.fn.returnType = checkedReturnType;
            arrayFree(&validatedParameterTypes);
            break;
        }
        case TY_STRUCT: {
            bool hadError = false;
            checkedType = typeNew(TY_STRUCT, parsedType->name, parsedType->declLocation, parsedType->declModule);
            // To prevent recursion.
            astModuleAddType(getCurrentCheckedModule(v), checkedType);
            ARRAY_FOR(i, parsedType->as.structure.fieldTypes) {
                Type *fType = ARRAY_GET_AS(Type *, &parsedType->as.structure.fieldTypes, i);
                // Note: parser checks for 'void' and 'pointer' types.
                Type *newfType = validateType(v, fType);
                if(newfType) {
                    arrayPush(&checkedType->as.structure.fieldTypes, (void *)newfType);
                } else {
                    hadError = true;
                }
            }
            if(hadError) {
                // It doesn't matter that the type was already added since
                // after validating all the types, validateModule() returns early on errors.
                return NULL;
            }
            break;
        }
        case TY_IDENTIFIER: {
            // Simply get the type the id type refers to and return it.
            Type *ty = astModuleGetType(getCurrentParsedModule(v), parsedType->as.id.actualName);
            if(ty == NULL) {
                error(v, parsedType->declLocation, "Type '%s' doesn't exist.", parsedType->as.id.actualName);
                return NULL;
            }
            // Note: we return early since the type is already added, so we don't want to re-add it because that will cause an abort.
            return validateType(v, ty);
        }
        case TY_SCOPE_RESOLUTION: {
            // Example 1: A::B
            //  * What we get: Type{name = "A::B", path = [A], ty = B}
            //  * Expected checked output: Type{name = "B", declModule = "A"}

            // Example 2: A::B::C
            //  * What we get: Type{name = "A::B::C", path = [A,B], ty = C}
            //  * Expected checked output: Type{name = "C", declModule = ???}

            // Notes:
            //  * Example 2 shouldn't even be possible since nested modules aren't a thing.
            //    However, nested types could be possible when type aliases (`type X = Y;`)
            //    are implemented.
            //  * So for now, the validator will only support single level type scope resolution.

            if(arrayLength(&parsedType->as.scopeResolution.path) > 1) {
                error(v, parsedType->declLocation, "Type scope resolution deeper than 1 level is not supported.");
                return NULL;
            }

            // FIXME: The following code is hardcoded for single level scope resolution. Fix this eventually, see above notes.
            Type *moduleID = ARRAY_GET_AS(Type *, &parsedType->as.scopeResolution.path, 0);
            Type *actualTypeID = parsedType->as.scopeResolution.ty;
            VERIFY(moduleID->type == TY_IDENTIFIER);
            VERIFY(actualTypeID->type == TY_IDENTIFIER);
            ASTModule *m = getImportedModule(v, moduleID->as.id.actualName);
            if(!m) {
                error(v, moduleID->declLocation, "Module '%s' has not been imported.", moduleID->as.id.actualName);
                return NULL;
            }
            // Properly returns NULL if the type doesn't exist.
            return astModuleGetType(m, actualTypeID->as.id.actualName);
        }
        default:
            UNREACHABLE();
    }

    VERIFY(checkedType != NULL);
    // The check is because for TY_STRUCT we add the type earlier to prevent infinite recursion
    // in the case of recursive structs.
    if(astModuleGetType(getCurrentCheckedModule(v), checkedType->name) == NULL) {
        astModuleAddType(getCurrentCheckedModule(v), checkedType);
    }
    return checkedType;
}

// Note: C.R.E for [n] or [a] to be NULL or for [n] to not be an EXPR_PROPERTY_ACCESS
static void unpackPropertyAccessExpr(Array *a, ASTExprNode *n) {
    VERIFY(a);
    VERIFY(n);
    VERIFY(NODE_IS(n, EXPR_PROPERTY_ACCESS));

    while(NODE_IS(NODE_AS(ASTBinaryExpr, n)->lhs, EXPR_PROPERTY_ACCESS)) {
        arrayPush(a, (void *)NODE_AS(ASTBinaryExpr, n)->rhs);
        n = NODE_AS(ASTBinaryExpr, n)->lhs;
    }

    arrayPush(a, NODE_AS(ASTBinaryExpr, n)->rhs);
    arrayPush(a, NODE_AS(ASTBinaryExpr, n)->lhs);

    // [a] will now contain all the identifier nodes from the property access expression in reverse order (root at top).
    // For example, for the expression a.b.c, [a] will contain [c, b, a] <--top.
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
            // FIXME: Can I call exprDataType() on a parsed (not checked) expr?
            //        It will work in this specific case, but is it something I should allow?
            checkedExpr = (ASTExprNode *)astConstantValueExprNew(getCurrentAllocator(v), EXPR_NUMBER_CONSTANT, parsedExpr->location, exprDataType(v, parsedExpr));
            NODE_AS(ASTConstantValueExpr, checkedExpr)->as.number = NODE_AS(ASTConstantValueExpr, parsedExpr)->as.number;
            break;
        case EXPR_STRING_CONSTANT:
            // FIXME: Can I call exprDataType() on a parsed (not checked) expr?
            //        It will work in this specific case, but is it something I should allow?
            checkedExpr = (ASTExprNode *)astConstantValueExprNew(getCurrentAllocator(v), EXPR_STRING_CONSTANT, parsedExpr->location, exprDataType(v, parsedExpr));
            NODE_AS(ASTConstantValueExpr, checkedExpr)->as.string = NODE_AS(ASTConstantValueExpr, parsedExpr)->as.string;
            break;
        case EXPR_BOOLEAN_CONSTANT:
            // Parser sets the type since true/false are always of type bool.
            VERIFY(parsedExpr->dataType->type == TY_BOOL);
            checkedExpr = NODE_AS(ASTExprNode, astConstantValueExprNew(getCurrentAllocator(v), EXPR_BOOLEAN_CONSTANT, parsedExpr->location, parsedExpr->dataType));
            NODE_AS(ASTConstantValueExpr, checkedExpr)->as.boolean = NODE_AS(ASTConstantValueExpr, parsedExpr)->as.boolean;
            break;
        // Obj nodes
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            LOG_ERR("Validator: object expr nodes not supported yet.");
            UNREACHABLE();
        // Binary nodes
        case EXPR_PROPERTY_ACCESS: {
            Array stack; // Array<ASTExprNode *>
            arrayInit(&stack);
            // a.b.c will become [c, b, a] <--top.
            unpackPropertyAccessExpr(&stack, parsedExpr);

            // Validate the root variable (a in a.b.c).
            ASTExprNode *checkedLhs = validateExpr(v, ARRAY_POP_AS(ASTExprNode *, &stack));
            if(!checkedLhs) {
                arrayFree(&stack);
                return NULL;
            }
            // Is [lhs] a variable (EXPR_VARIABLE, EXPR_DEREF)?
            if(!(NODE_IS(checkedLhs, EXPR_VARIABLE) || NODE_IS(checkedLhs, EXPR_DEREF))) {
                error(v, checkedLhs->location, "Property access can only be done on variables.");
                arrayFree(&stack);
                break;
            }

            // This is generally done in the typechecker, but we need the type here, so we check.
            if(exprDataType(v, checkedLhs) == NULL) {
                ASTObj *lhsObj = NULL;
                switch(checkedLhs->type) {
                    case EXPR_VARIABLE:
                        lhsObj = NODE_AS(ASTObjExpr, checkedLhs)->obj;
                        break;
                    case EXPR_DEREF:
                        lhsObj = NODE_AS(ASTObjExpr, NODE_AS(ASTUnaryExpr, checkedLhs)->operand)->obj;
                        break;
                    default:
                        UNREACHABLE();
                }
                error(v, checkedLhs->location, "Variable '%s' has no type.", lhsObj->name);
                hint(v, lhsObj->location, "Consider adding an explicit type here.");
                arrayFree(&stack);
                break;
            }

            while(arrayLength(&stack) > 0) {
                ASTExprNode *rhs = ARRAY_POP_AS(ASTExprNode *, &stack);
                Type *lhsTy = exprDataType(v, checkedLhs);
                // If [lhs] is of a pointer type, wrap it in a dereference (auto-deref in property access).
                // An example of why we do this is as following:
                // | var a: &someStruct = getPointerToStruct();
                // If we didn't auto-dereference, we would have to access a's fields like this:
                // | (*a).someField;
                // With auto-deref we can simply access the fields as normal:
                // | a.someField;
                if(lhsTy->type == TY_POINTER && lhsTy->as.ptr.innerType->type == TY_STRUCT) {
                    checkedLhs = NODE_AS(ASTExprNode, astUnaryExprNew(getCurrentAllocator(v), EXPR_DEREF, checkedLhs->location, lhsTy->as.ptr.innerType, checkedLhs));
                    lhsTy = lhsTy->as.ptr.innerType;
                }

                // Does [lhs] refer to a struct-type variable?
                if(lhsTy->type != TY_STRUCT) {
                    error(v, checkedLhs->location, "Field access on value of non-struct type '%s'.", typeName(lhsTy));
                    break;
                }

                // Get the struct ASTObj.
                ASTObj *structure = scopeGetObject(getCurrentCheckedModule(v)->moduleScope, OBJ_STRUCT, lhsTy->name);
                VERIFY(structure);
                ASTObj *field = scopeGetAnyObject(structure->as.structure.scope, NODE_AS(ASTIdentifierExpr, rhs)->id);
                if(!field) {
                    error(v, rhs->location, "Field '%s' doesn't exist in struct '%s'.", NODE_AS(ASTIdentifierExpr, rhs)->id, structure->name);
                    break;
                }
                ASTExprNode *fieldNode = NODE_AS(ASTExprNode, astObjExprNew(getCurrentAllocator(v), EXPR_VARIABLE, field->location, field));
                if(checkedExpr) {
                    checkedExpr = NODE_AS(ASTExprNode, astBinaryExprNew(getCurrentAllocator(v), EXPR_PROPERTY_ACCESS, locationMerge(checkedExpr->location, rhs->location), rhs->dataType, checkedExpr, fieldNode));
                } else {
                    checkedExpr = NODE_AS(ASTExprNode, astBinaryExprNew(getCurrentAllocator(v), EXPR_PROPERTY_ACCESS, locationMerge(checkedLhs->location, rhs->location), field->dataType, checkedLhs, fieldNode));
                }
                checkedLhs = fieldNode;
            }

            arrayFree(&stack);
            break;
        }
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
        case EXPR_LOGICAL_AND:
        case EXPR_LOGICAL_OR: {
            #define IS_ASSIGNMENT_TARGET(expr) ((expr)->type == EXPR_VARIABLE || (expr)->type == EXPR_DEREF || (expr)->type == EXPR_PROPERTY_ACCESS)
            ASTExprNode *lhs = TRY(ASTExprNode *, validateExpr(v, NODE_AS(ASTBinaryExpr, parsedExpr)->lhs));
            if(NODE_IS(parsedExpr, EXPR_ASSIGN) && !IS_ASSIGNMENT_TARGET(lhs)) {
                error(v, lhs->location, "Invalid assignment target (only variables can be assigned).");
                break;
            }
            ASTExprNode *rhs = TRY(ASTExprNode *, validateExpr(v, NODE_AS(ASTBinaryExpr, parsedExpr)->rhs));
            checkedExpr = (ASTExprNode *)astBinaryExprNew(getCurrentAllocator(v), parsedExpr->type, parsedExpr->location, NULL, lhs, rhs);
            checkedExpr->dataType = exprDataType(v, checkedExpr);
            break;
            #undef IS_ASSIGNMENT_TARGET
        }
        // Unary nodes
        case EXPR_NEGATE:
        case EXPR_LOGICAL_NOT:
        case EXPR_ADDROF:
        case EXPR_DEREF: {
            // TODO: move to Type.h/c (header only if made inline function)
            #define IS_POINTER(ty) ((ty)->type == TY_POINTER)
            ASTExprNode *checkedOperand = TRY(ASTExprNode *, validateExpr(v, NODE_AS(ASTUnaryExpr, parsedExpr)->operand));
            Type *exprTy = exprDataType(v, checkedOperand);
            if(NODE_IS(parsedExpr, EXPR_ADDROF)) {
                String ptrName = stringFormat("&%s", exprTy->name);
                exprTy = getType(v, ptrName);
                stringFree(ptrName);
            }
            // !<expr> generates a boolean value.
            if(NODE_IS(parsedExpr, EXPR_LOGICAL_NOT)) {
                exprTy = getType(v, "bool"); // since exprTy was type of operand.
            }
            checkedExpr = NODE_AS(ASTExprNode, astUnaryExprNew(getCurrentAllocator(v), parsedExpr->type, parsedExpr->location, exprTy, checkedOperand));
            if(NODE_IS(checkedExpr, EXPR_DEREF)) {
                if(!(NODE_IS(checkedOperand, EXPR_VARIABLE) || NODE_IS(checkedOperand, EXPR_PROPERTY_ACCESS) || NODE_IS(checkedOperand, EXPR_DEREF) || NODE_IS(checkedOperand, EXPR_ADDROF))) {
                    error(v, checkedOperand->location, "Cannot dereference non-variable object.");
                    checkedExpr = NULL;
                } else if(!IS_POINTER(checkedOperand->dataType)) {
                    error(v, checkedOperand->location, "Cannot dereference non-pointer type '%s'.", exprTy->name);
                    checkedExpr = NULL;
                } else {
                    // If no errors, set deref expr type to correct type.
                    VERIFY(exprTy->type == TY_POINTER);
                    checkedExpr->dataType = exprTy->as.ptr.innerType;
                }
            }
            break;
            #undef IS_POINTER
        }
        // Call node
        case EXPR_CALL: {
            ASTCallExpr *parsedCall = NODE_AS(ASTCallExpr, parsedExpr);
            // 1. validate callee, make sure its callable.
            ASTExprNode *checkedCallee = TRY(ASTExprNode *, validateExpr(v, parsedCall->callee));
            VERIFY(checkedCallee->dataType);
            if(checkedCallee->dataType->type != TY_FUNCTION) {
                error(v, checkedCallee->location, "Type '%s' isn't callable.", checkedCallee->dataType->name);
                // Note: we can continue validating since we don't typecheck the arguments yet,
                //       so we don't need to know what type they are supposed to be, info that's
                //       only available if the type is callable (and eventually refers to a function.)
                break;
            }

            Array checkedArguments;
            // size + 1 for potential 'this' implicit argument.
            arrayInitSized(&checkedArguments, arrayLength(&parsedCall->arguments) + 1);
            // 1a. If callee is a method (i.e. callee node is EXPR_PROPERTY_ACCESS), pass 'this' as first argument.
            if(NODE_IS(checkedCallee, EXPR_PROPERTY_ACCESS)) {
                // Struct method belongs to is lhs/lhs.rhs of 'checkedCallee'.
                // First option for expressions of the form 'a.b()' (struct is 'a',)
                // and the second option for longer chains such as 'a.b.c.d()' (struct is 'c'.)
                ASTBinaryExpr *callee = NODE_AS(ASTBinaryExpr, checkedCallee);
                ASTExprNode *thisArg = NULL;
                if(NODE_IS(callee->lhs, EXPR_PROPERTY_ACCESS)) {
                    thisArg = NODE_AS(ASTBinaryExpr, callee->lhs)->rhs;
                } else {
                    VERIFY(NODE_IS(callee->lhs, EXPR_VARIABLE));
                    thisArg = callee->lhs;
                }
                // Make the expression "passed" as 'this' generate a pointer.
                Type *thisType = getType(v, stringTableFormat(v->checkedProgram->strings, "&%s", thisArg->dataType->name));
                thisArg = (ASTExprNode *)astUnaryExprNew(getCurrentAllocator(v), EXPR_ADDROF, EMPTY_LOCATION, thisType, thisArg);
                // Finally, add 'this' to the argument list (as the first argument.)
                arrayPush(&checkedArguments, (void *)thisArg);
            }
            // 2. validate arguments.
            bool hadError = false;
            ARRAY_FOR(i, parsedCall->arguments) {
                ASTExprNode *arg = ARRAY_GET_AS(ASTExprNode *, &parsedCall->arguments, i);
                // When 'this' is validated, a new AST is created for it which is fine.
                arg = validateExpr(v, arg);
                if(arg) {
                    arrayPush(&checkedArguments, (void *)arg);
                } else {
                    hadError = true;
                }
            }
            if(hadError) {
                arrayFree(&checkedArguments);
                break;
            }
            usize argumentCount = arrayLength(&checkedArguments);
            usize expectedArgumentCount = arrayLength(&checkedCallee->dataType->as.fn.parameterTypes);
            if(expectedArgumentCount != argumentCount) {
                error(v, parsedExpr->location, "Expected %lu argument%s but got %lu.", expectedArgumentCount, expectedArgumentCount != 1 ? "s" : "", argumentCount);
                // TODO: hint to where fn is declared. Can't do simply right now since callee may be a variable refering to a function.
                arrayFree(&checkedArguments);
                break;
            }
            Type *calleeType = expr_data_type_complex(v, checkedCallee, true);
            VERIFY(calleeType);
            checkedExpr = (ASTExprNode *)astCallExprNew(getCurrentAllocator(v), parsedExpr->location, calleeType, checkedCallee, &checkedArguments);
            arrayFree(&checkedArguments);
            break;
        }
        // Other nodes.
        case EXPR_IDENTIFIER: {
            ASTIdentifierExpr *idExpr = NODE_AS(ASTIdentifierExpr, parsedExpr);
            // An ID can have a maximum part count of 3: Module::Record::ID, where the maximum path length is 2 (b/c the actual ID is the last part.)
            // (A record is a struct or enum.)
            // Note that Record::ID is also valid, so if we fail to get an imported module,
            // the modulePart is "promoted" to be a "recordPart" and we try again as if
            // the modulePart never existed.
            VERIFY(arrayLength(&idExpr->path) <= 2);

            // Note: arrayGet() returns NULL on an out-of-bounds index.
            ASTString modulePart = ARRAY_GET_AS(ASTString, &idExpr->path, 0);
            ASTString recordPart = ARRAY_GET_AS(ASTString, &idExpr->path, 1);
            ASTString idPart = idExpr->id;

            Scope *scope = NULL; // will contain the ID.
            bool failedModule = false; // To handle the case of Record::ID.
            ASTObj *record = NULL; // for the following case: Module::Record::ID.
            ASTObj *id = NULL;

            // TODO for the following 5 errors: get better locations (involves making the parser save the location of each part.)

            if(modulePart != NULL) {
                ASTModule *m = getImportedModule(v, modulePart);
                if(!m) {
                    // Failing to get the module could mean that this refers to a struct.
                    // However, that is only possible if the recordPart doesn't exist.
                    if(recordPart != NULL) {
                        error(v, parsedExpr->location, "Module '%s' has not been imported.", modulePart);
                        break;
                    }
                    failedModule = true;
                    recordPart = modulePart;
                } else {
                    scope = m->moduleScope;
                }
            }

            // Note: Since the parts come from an array, recordPart existing guranatees modulePart's existence.
            if(recordPart != NULL) {
                if(failedModule) {
                    record = findObjVisibleInCurrentScope(v, recordPart, NULL);
                } else {
                    record = findObjVisibleInScope(scope, recordPart, NULL);
                }
                if(!record) {
                    if(failedModule) {
                        // FIXME: Deduplicate errors that appear in multiple places. Otherwise this will get confusing.
                        //        (this error also appears above.)
                        error(v, parsedExpr->location, "Module '%s' has not been imported.", modulePart);
                    } else {
                        // TODO: when enums are added, make "Structure" be either "Structure" or "Enum", depending on the type.
                        error(v, parsedExpr->location, "Structure '%s' does not exist in module '%s'.", recordPart, modulePart);
                    }
                    break;
                }
            }
            // FIXME (not here): have a problem with the module scope being the same as the struct scope?????

            // get the ID part (always exists b/c its the last part of the ID which always exists).
            // Check if the record part exists first (since if it exists the id is inside of it.)
            if(record != NULL) {
                VERIFY(record->type == OBJ_STRUCT); // TODO: when enums are added, support them here (maybe generalize? probably overkill).
                id = scopeGetAnyObject(record->as.structure.scope, idPart);
                if(!id) {
                    // TODO: when enums are added, change "structure" to "structure" or "enum" depending on the type.
                    error(v, parsedExpr->location, "Function '%s' does not exist in structure '%s'.", idPart, recordPart);
                    break;
                }
            } else if(scope != NULL) { // Now check if the module exists (and the record does not). If it does, get the id from it.
                id = findObjVisibleInScope(scope, idPart, NULL);
            } else { // Otherwise, simply get the id from the current scope (and parents).
                id = findObjVisibleInCurrentScope(v, idPart, NULL);
            }
            if(!id) {
                error(v, parsedExpr->location, "Identifier '%s' does not exist.", idPart);
                break;
            }


            ASTExprType exprType;
            switch(id->type) {
                case OBJ_VAR:
                    exprType = EXPR_VARIABLE;
                    break;
                case OBJ_FN:
                    exprType = EXPR_FUNCTION;
                    break;
                default:
                    error(v, parsedExpr->location, "Identifier '%s' does not refer to a variable or a function.", idExpr->id);
                    return NULL;
            }
            checkedExpr = (ASTExprNode *)astObjExprNew(getCurrentAllocator(v), exprType, parsedExpr->location, id);
            checkedExpr->dataType = id->dataType;
            break;
        }
        case EXPR_MODULE: // Is only created by ourselves.
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
            // Add parameters to current scope as local variables (only for the first function scope.)
            // FIXME: parser already does that. We should either do it here only, or there only.
            VERIFY(v->current.function);
            if(getCurrentCheckedScope(v)->depth == SCOPE_DEPTH_BLOCK) {
                ARRAY_FOR(i, v->current.function->as.fn.parameters) {
                    ASTObj *param = ARRAY_GET_AS(ASTObj *, &v->current.function->as.fn.parameters, i);
                    scopeAddObject(getCurrentCheckedScope(v), param);
                }
            }
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
        case STMT_IF: {
            ASTConditionalStmt *parsedIf = NODE_AS(ASTConditionalStmt, parsedStmt);
            ASTExprNode *checkedCondition = TRY(ASTExprNode *, validateExpr(v, parsedIf->condition));
            ASTStmtNode *checkedThen = TRY(ASTStmtNode *, validateStmt(v, parsedIf->then));
            ASTStmtNode *checkedElse = NULL;
            if(parsedIf->else_) {
                checkedElse = TRY(ASTStmtNode *, validateStmt(v, parsedIf->else_));
            }
            checkedStmt = NODE_AS(ASTStmtNode, astConditionalStmtNew(getCurrentAllocator(v), STMT_IF, parsedStmt->location, checkedCondition, checkedThen, checkedElse));
            break;
        }
        case STMT_EXPECT: {
            ASTConditionalStmt *parsedExpect = NODE_AS(ASTConditionalStmt, parsedStmt);
            ASTExprNode *checkedCondition = TRY(ASTExprNode *, validateExpr(v, parsedExpect->condition));
            ASTStmtNode *checkedThen = NULL;
            if(parsedExpect->then) {
                checkedThen = TRY(ASTStmtNode *, validateStmt(v, parsedExpect->then));
            }
            checkedStmt = NODE_AS(ASTStmtNode, astConditionalStmtNew(getCurrentAllocator(v), STMT_EXPECT, parsedStmt->location, checkedCondition, checkedThen, NULL));
            break;
        }
        // Loop nodes
        case STMT_LOOP: {
            ASTLoopStmt *parsedLoop = NODE_AS(ASTLoopStmt, parsedStmt);
            ASTStmtNode *checkedInit = NULL;
            if(parsedLoop->initializer) {
                checkedInit = TRY(ASTStmtNode *, validateStmt(v, parsedLoop->initializer));
            }
            ASTExprNode *checkedCondition = TRY(ASTExprNode *, validateExpr(v, parsedLoop->condition));
            ASTExprNode *checkedInc = NULL;
            if(parsedLoop->increment) {
                checkedInc = TRY(ASTExprNode *, validateExpr(v, parsedLoop->increment));
            }
            // Note: body will ALWAYS be a block. That is how the parser parses it.
            ASTBlockStmt *checkedBody = NODE_AS(ASTBlockStmt, TRY(ASTStmtNode *, validateStmt(v, NODE_AS(ASTStmtNode, parsedLoop->body))));
            checkedStmt = NODE_AS(ASTStmtNode, astLoopStmtNew(getCurrentAllocator(v), parsedStmt->location, checkedInit, checkedCondition, checkedInc, checkedBody));
            break;
        }
        // Expr nodes
        case STMT_RETURN: {
            // Can't use same code as STMT_EXPR due to return statements not requiring an operand.
            ASTExprNode *checkedOperand = NULL;
            if(NODE_AS(ASTExprStmt, parsedStmt)->expression) {
                checkedOperand = TRY(ASTExprNode *, validateExpr(v, NODE_AS(ASTExprStmt, parsedStmt)->expression));
                if(v->current.function->as.fn.returnType->type == TY_U32 && NODE_IS(checkedOperand, EXPR_NUMBER_CONSTANT)) {
                    // TODO: wrap operand in type conversion expression.
                    checkedOperand->dataType = getType(v, "u32");
                }
            }
            checkedStmt = (ASTStmtNode *)astExprStmtNew(getCurrentAllocator(v), STMT_RETURN, parsedStmt->location, checkedOperand);
            break;
        }
        case STMT_EXPR: {
            ASTExprNode *checkedExpr = TRY(ASTExprNode *, validateExpr(v, NODE_AS(ASTExprStmt, parsedStmt)->expression));
            checkedStmt = NODE_AS(ASTStmtNode, astExprStmtNew(getCurrentAllocator(v), parsedStmt->type, parsedStmt->location, checkedExpr));
            break;
        }
        // Defer nodes
        case STMT_DEFER: {
            ASTStmtNode *checkedOperand = TRY(ASTStmtNode *, validateStmt(v, NODE_AS(ASTDeferStmt, parsedStmt)->body));
            checkedStmt = NODE_AS(ASTStmtNode, astDeferStmtNew(getCurrentAllocator(v), parsedStmt->location, checkedOperand));
            break;
        }
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

    // TODO: properly define this. another global variable is also const.
    #define IS_CONST_EXPR(expr) ((expr)->type == EXPR_NUMBER_CONSTANT || (expr)->type == EXPR_STRING_CONSTANT)
    if(getCurrentCheckedScope(v)->depth == SCOPE_DEPTH_MODULE_NAMESPACE && parsedVarDecl->initializer && !IS_CONST_EXPR(parsedVarDecl->initializer)) {
        error(v, parsedVarDecl->initializer->location, "Initializer of module variable is not a constant expression.");
        return NULL;
    }
    #undef IS_CONST_EXPR

    ASTExprNode *checkedInitializer = NULL;
    if(parsedVarDecl->initializer) {
        checkedInitializer = TRY(ASTExprNode *, validateExpr(v, parsedVarDecl->initializer));
    }


    // Note: typechecking is NOT done here.
    Type *dataType = parsedVarDecl->variable->dataType;
    if(dataType == NULL && checkedInitializer) {
        dataType = checkedInitializer->dataType;
    }
    if(dataType == NULL) {
        error(v, parsedVarDecl->variable->location, "Cannot infer type of variable '%s'.", parsedVarDecl->variable->name);
        hint(v, parsedVarDecl->header.location, "Consider adding an explicit type%s.", checkedInitializer ? "" : " or an initializer");
        return NULL;
    }
    if(dataType->type == TY_VOID) {
        error(v, parsedVarDecl->variable->location, "A variable cannot have the type 'void'.");
        return NULL;
    }
    dataType = TRY(Type *, validateType(v, dataType));

    if(dataType->type == TY_U32 && NODE_IS(checkedInitializer, EXPR_NUMBER_CONSTANT)) {
        // TODO: wrap initializer in type conversion expression.
        checkedInitializer->dataType = getType(v, "u32");
    }

    ASTObj *checkedObj = astModuleNewObj(getCurrentCheckedModule(v), OBJ_VAR, parsedVarDecl->variable->location, parsedVarDecl->variable->name, dataType);
    bool added = scopeAddObject(getCurrentCheckedScope(v), checkedObj);
    VERIFY(added == true);
    ASTVarDeclStmt *checkedVarDecl = astVarDeclStmtNew(getCurrentAllocator(v), parsedVarDecl->header.location, checkedObj, checkedInitializer);
    return checkedVarDecl;
}

// Notes:
// * C.R.E for [fn] to be NULL.
// * Adds the function object to the current scope.
// * returns true on success, and false on failure.
static bool validateFunction(Validator *v, ASTObj *fn) {
    // Note: (parsed) fn scope already contains params. They are added by the parser.
    // Get the predecled checked function.
    ASTObj *checkedFn = scopeGetObject(getCurrentCheckedScope(v), OBJ_FN, fn->name);
    VERIFY(checkedFn); // MUST exist.
    v->current.function = checkedFn;
    ASTStmtNode *checkedBody = TRY(ASTStmtNode *, validateStmt(v, NODE_AS(ASTStmtNode, fn->as.fn.body)));
    v->current.function = NULL;
    checkedFn->as.fn.body = NODE_AS(ASTBlockStmt, checkedBody);

    // checkedFn is already in the current scope. see above.
    return true;
}

static bool validateCurrentScope(Validator *v);
static bool validateStruct(Validator *v, ASTObj *st) {
    Type *checkedStructType = validateType(v, st->dataType);
    VERIFY(checkedStructType); // Guranteed to exist (types are validated in validateModule().)
    ASTObj *checkedStruct = astModuleNewObj(getCurrentCheckedModule(v), OBJ_STRUCT, st->location, st->name, checkedStructType);

    // Note: need to enter scope before setting the scope in the checkedObj and adding it to the current module.
    //       Otherwise we set the scope of the struct to the scope of the module...
    enterScope(v, st->as.structure.scope);

    checkedStruct->as.structure.scope = getCurrentCheckedScope(v);
    scopeAddObject(getCurrentCheckedModule(v)->moduleScope, checkedStruct);

    // Note: when we validate types, we validate the fields (their types.)
    //       So here we only add them to the scope.
    Array objects;
    arrayInit(&objects);
    scopeGetAllObjects(st->as.structure.scope, &objects);
    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        if(obj->type == OBJ_VAR) {
            Type *checkedFieldType = validateType(v, obj->dataType);
            VERIFY(checkedFieldType); // TODO: Guaranteed to exist I think (type validation in module)??
            ASTObj *checkedField = astModuleNewObj(getCurrentCheckedModule(v), obj->type, obj->location, obj->name, checkedFieldType);
            // Set the parent of the field as the struct.
            checkedField->parent = checkedStruct;
            // Note: parser checks for field re-declaration.
            scopeAddObject(getCurrentCheckedScope(v), checkedField);
        }
    }
    arrayFree(&objects);
    // Set objParent to this struct so that all of the functions in its scope have it as their parent.
    v->current.objParent = checkedStruct;
    // Note: the checked struct has to exist before validating the methods since they
    //       use the struct. Here, the struct is now created at the beginning of this function.
    bool hadError = !validateCurrentScope(v);
    v->current.objParent = NULL;
    leaveScope(v);
    return !hadError;
}

// Note: sets the parent of the new checked function to whatever v.current.objParent is.
static void predeclFunction(Validator *v, ASTObj *parsedFn) {
    ASTObj *checkedFnPredecl = astModuleNewObj(getCurrentCheckedModule(v), OBJ_FN, parsedFn->location, parsedFn->name, NULL);
    // Note: struct fields get this in validateStruct().
    checkedFnPredecl->parent = v->current.objParent;
    Type *checkedDataType = validateType(v, parsedFn->dataType);
    checkedFnPredecl->dataType = checkedDataType;
    Type *checkedReturnType = validateType(v, parsedFn->as.fn.returnType);
    checkedFnPredecl->as.fn.returnType = checkedReturnType;
    bool hadError = false;
    ARRAY_FOR(i, parsedFn->as.fn.parameters) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &parsedFn->as.fn.parameters, i);
        Type *checkedParamType = validateType(v, param->dataType);
        if(checkedParamType) {
            ASTObj *checkedParam = astModuleNewObj(getCurrentCheckedModule(v), OBJ_VAR, param->location, param->name, checkedParamType);
            arrayPush(&checkedFnPredecl->as.fn.parameters, (void *)checkedParam);
        } else {
            hadError = true;
        }
    }
    if(!checkedDataType || !checkedReturnType || hadError) {
        return;
    }
    scopeAddObject(getCurrentCheckedScope(v), checkedFnPredecl);
}

// Note: scope here refers to a namespace type scope (i.e. SCOPE_DEPTH_MODULE_NAMESPACE or SCOPE_DEPTH_STRUCT)
static bool validateCurrentScope(Validator *v) {
    VERIFY(v->current.parsedScope);

    Array objectsInScope;
    arrayInitSized(&objectsInScope, scopeGetNumObjects(getCurrentParsedScope(v)));
    scopeGetAllObjects(getCurrentParsedScope(v), &objectsInScope);

    bool hadError = false;
    // Validate structs first.
    // This is done since functions likely will use the structs, which requires that their fields be validated.
    // TODO: When methods are added, will have to make a "validate fields only" pass
    //       before methods are validated for the same reason.
    ARRAY_FOR(i, objectsInScope) {
        ASTObj *parsedObj = ARRAY_GET_AS(ASTObj *, &objectsInScope, i);
        if(parsedObj->type == OBJ_STRUCT) {
            if(!validateStruct(v, parsedObj)) {
                hadError = true;
            }
        }
    }
    if(hadError) {
        arrayFree(&objectsInScope);
        return false;
    }

    // Predeclare functions so they all exist in the checked scope.
    ARRAY_FOR(i, objectsInScope) {
        ASTObj *parsedObj = ARRAY_GET_AS(ASTObj *, &objectsInScope, i);
        // Note: OBJ_VARs will be validated as the variables are declared (in validateStmt().)
        if(parsedObj->type == OBJ_FN) {
            predeclFunction(v, parsedObj);
        } // TODO: set parent (only if enclosing scope is a struct.)
    }
    if(v->hadError) {
        arrayFree(&objectsInScope);
        return false;
    }

    // Now validate functions.
    ARRAY_FOR(i, objectsInScope) {
        ASTObj *parsedObj = ARRAY_GET_AS(ASTObj *, &objectsInScope, i);
        // Note: OBJ_VARs will be validated as the variables are declared (in validateStmt().)
        if(parsedObj->type == OBJ_FN) {
            // validateObject() adds the object to the current scope.
            if(!validateFunction(v, parsedObj)) {
                hadError = true;
            }
        }
    }
    arrayFree(&objectsInScope);

    // Note: no need to validate child scopes. They are validated as necessary when entered.

    return !hadError;
}

static void validate_type_callback(TableItem *item, bool is_last, void *validator) {
    UNUSED(is_last);
    Validator *v = (Validator *)validator;
    Type *parsedType = (Type *)item->value;
    // Note: ID types are validated as used.
    if(parsedType->type != TY_IDENTIFIER) {
        validateType(v, parsedType); // validateType() adds the type to the current module.
    }
}

static void validate_import_callback(TableItem *item, bool isLast, void *validator) {
    UNUSED(isLast);
    Validator *v = (Validator *)validator;
    ASTString importName = (ASTString)item->key;
    // TODO: store modules in a set (table with no value) for O(1) lookup.
    ARRAY_FOR(i, v->parsedProgram->modules) {
        ASTModule *m = ARRAY_GET_AS(ASTModule *, &v->parsedProgram->modules, i);
        if(importName == m->name) {
            astModuleAddImport(getCurrentCheckedModule(v), importName, (ModuleID)i);
            return;
        }
    }
    // Modules are checked for existence in parser.
    // If the Validator is called, then all imports in the parsedProgram MUST exist.
    UNREACHABLE();
}

// Notes:
//   * The new module is created with astProgramNewModule(checked_prog), and so there is no need to return it.
//   * C.R.E for moduleID > amount of modules in parsed module.
//   * In case of any errors, v.hadError will be set. Use it to check for errors.
static void validateModule(Validator *v, ModuleID moduleID) {
    // Is the moduleID valid (withing the range of existing modules)?
    VERIFY(moduleID < arrayLength(&v->parsedProgram->modules));
    // validate:
    //     imports
    //     types
    //     variable declarations (module scoppe)
    //     scopes

    // Preliminary initialization, setting up of validator state.
    ASTModule *parsedModule = astProgramGetModule(v->parsedProgram, moduleID);
    astProgramNewModuleWithID(v->checkedProgram, moduleID, parsedModule->name);
    ASTModule *checkedModule = astProgramGetModule(v->checkedProgram, moduleID);
    v->current.module = moduleID;
    v->current.parsedScope = parsedModule->moduleScope;
    v->current.checkedScope = checkedModule->moduleScope;

    // Validate imports.
    tableMap(&parsedModule->importedModules, validate_import_callback, (void *)v);
    if(v->hadError) {
        return;
    }

    // Validate types.
    tableMap(&parsedModule->types, validate_type_callback, (void *)v);
    if(v->hadError) {
        return;
    }

    // Validate module scope level variable declarations ("globals".)
    ARRAY_FOR(i, parsedModule->variableDecls) {
        ASTVarDeclStmt *parsedVarDecl = ARRAY_GET_AS(ASTVarDeclStmt *, &parsedModule->variableDecls, i);
        // Note: validateVariableDecl() also validates the objects.
        ASTVarDeclStmt *checkedVarDecl = validateVariableDecl(v, parsedVarDecl);
        if(checkedVarDecl) {
            arrayPush(&checkedModule->variableDecls, (void *)checkedVarDecl);
        }
    }
    if(v->hadError) {
        return;
    }
    // Notes to clear my confusion:
    // * When validating a scope, we DON'T have enought info to validate OBJ_VARs.
    //   - Hence we validate them as we encounter them, which doesn't cause any problem for locals
    //     since we can't use them before they are declared.
    //   - Since we validate OBJ_VARs as we encounter them, we skip them when validating the objects
    //     in a scope. This means we can validate "globals" before the scope (but after the types).
    // * When is validateCurrentScope() called then?
    //   - When validating a module or a struct only. In the future, they will be called on enums
    //     as well (unless they are implemented using structs.)
    //   - In any case, it should NOT be called in enterScope() since the "scope" it validates is actually a
    //     namespace, and enterScope() is also used for block scopes which ONLY have variables.
    //   - If I decide to allow lambdas/closures, then it will have to be called on functions as well.

    // The module scope is already "entered" because we set the current scope to the module scope.
    validateCurrentScope(v);

    // See notes above declaration of this function on how caller should check for errors.
}

static void collect_imports_callback(TableItem *item, bool isLast, void *output) {
    UNUSED(isLast);
    arrayPush((Array *)output, item->key);
}

static ASTModule *get_module_by_name(Array *modules, ASTString name) {
    ARRAY_FOR(i, *modules) {
        ASTModule *m = ARRAY_GET_AS(ASTModule *, modules, i);
        if(name == m->name) {
            return m;
        }
    }
    UNREACHABLE();
}

static bool topographicallySortModules(Validator *v, Array *modules, Array *output) {
    Table dependencies; // Table<ASTString, i64> (string is module name.)
    tableInit(&dependencies, NULL, NULL);

    ARRAY_FOR(i, *modules) {
        ASTModule *m = ARRAY_GET_AS(ASTModule *, modules, i);
        Array imports; // Array<ASTModule *>
        arrayInitSized(&imports, tableSize(&m->importedModules));
        tableMap(&m->importedModules, collect_imports_callback, (void *)&imports);
        ARRAY_FOR(j, imports) {
            ASTString importName = ARRAY_GET_AS(ASTString, &imports, j);
            TableItem *item = tableGet(&dependencies, (void *)importName);
            i64 existing = item == NULL ? 0 : (i64)item->value;
            tableSet(&dependencies, (void *)importName, (void *)(existing + 1));
        }
        arrayFree(&imports);
        if(tableGet(&dependencies, (void *)m->name) == NULL) {
            tableSet(&dependencies, (void *)m->name, (void *)0l);
        }
    }

    Array stack; // Array<ASTModule *>
    arrayInit(&stack);
    ARRAY_FOR(i, *modules) {
        ASTModule *m = ARRAY_GET_AS(ASTModule *, modules, i);
        TableItem *item = tableGet(&dependencies, (void *)m->name);
        if((i64)item->value == 0) {
            arrayPush(&stack, (void *)m);
        }
    }

    arrayClear(output);
    while(arrayLength(&stack) > 0) {
        ASTModule *m = ARRAY_POP_AS(ASTModule *, &stack);
        arrayPush(output, (void *)m);
        Array imports; // Array<ASTString>
        arrayInitSized(&imports, tableSize(&m->importedModules));
        tableMap(&m->importedModules, collect_imports_callback, (void *)&imports);
        ARRAY_FOR(i, imports) {
            ASTString importName = ARRAY_GET_AS(ASTString, &imports, i);
            TableItem *item = tableGet(&dependencies, (void *)importName);
            i64 existing = (i64)item->value;
            tableSet(&dependencies, (void *)importName, (void *)(existing - 1));
            if(existing == 1) {
                arrayPush(&stack, get_module_by_name(modules, importName));
            }
        }
        arrayFree(&imports);
    }
    arrayFree(&stack);

    if(arrayLength(output) != arrayLength(modules)) {
        VERIFY(arrayLength(modules) > arrayLength(output));
        for(usize i = arrayLength(output); i < arrayLength(modules); ++i) {
            ASTModule *m = ARRAY_GET_AS(ASTModule *, modules, i);
            String msg = stringFormat("Module '%s' has cyclic imports.", m->name);
            add_error(v, false, EMPTY_LOCATION, ERR_ERROR, msg);
            stringFree(msg);
        }
    }
    // Reverse output array so the module with the least dependencies
    // will be first, then the next one etc.
    // The last modules has the most dependencies.
    // Note: dependency means that another module imports it.
    arrayReverse(output);

    tableFree(&dependencies);
    return true;
}

bool validatorValidate(Validator *v, ASTProgram *parsedProg, ASTProgram *checkedProg) {
    VERIFY(parsedProg);
    VERIFY(checkedProg);
    v->parsedProgram = parsedProg;
    v->checkedProgram = checkedProg;
    // Sort all the modules by imports. This will also detect cyclic imports.
    // Example: if a imports b and b imports c, c will be first, then b, then a.
    //          It would be an error for a to import b, and for b to import a (directly or indeirectly.)
    Array sortedModules; // Array<ASTModule *>
    arrayInitSized(&sortedModules, arrayLength(&parsedProg->modules));
    if(!topographicallySortModules(v, &parsedProg->modules, &sortedModules)) {
        // error: cyclic modules. already reported.
        return false;
    }
    // Validate all modules.
    ARRAY_FOR(i, sortedModules) {
        ASTModule *m = ARRAY_GET_AS(ASTModule *, &sortedModules, i);
        validateModule(v, m->id);
    }
    arrayFree(&sortedModules);
    // If we had any error while parsing any of the modules, fail.
    return !v->hadError;
}

#undef TRY
