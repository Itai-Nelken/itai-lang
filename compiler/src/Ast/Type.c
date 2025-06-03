#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Ast/StringTable.h"
#include "Ast/Module.h"
#include "Ast/Type.h"

static inline const char *type_type_to_string(TypeType type) {
    VERIFY(type < TY_TYPE_COUNT);
    const char *names[] = {
        [TY_VOID]       = "TY_VOID",
        [TY_I32]        = "TY_I32",
        [TY_U32]        = "TY_U32",
        [TY_STR]        = "TY_STR",
        [TY_BOOL]       = "TY_BOOL",
        [TY_POINTER]    = "TY_POINTER",
        [TY_FUNCTION]   = "TY_FUNCTION",
        [TY_STRUCT]     = "TY_STRUCT",
        [TY_IDENTIFIER] = "TY_IDENTIFIER"
    };
    return names[(int)type];
}

void typePrint(FILE *to, Type *ty, bool compact) {
    if(!ty) {
        fputs("(null)", to);
        return;
    }
    if(compact) {
        fprintf(to, "Type{%s%s}", ty->type == TY_IDENTIFIER ? "\x1b[1mid:\x1b[0m " : "", ty->name);
        return;
    }
    // verbose output
    fprintf(to, "Type{\x1b[1mtype:\x1b[33m %s\x1b[0m", type_type_to_string(ty->type));
    fprintf(to, ", \x1b[1mname:\x1b[0m '%s', \x1b[1mdeclLocation:\x1b[0m ", ty->name);
    locationPrint(to, ty->declLocation, true);
    fprintf(to, ", \x1b[1mdeclModule:\x1b[0;34m %zu\x1b[0m", ty->declModule);
    switch(ty->type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
        case TY_BOOL:
            // nothing
            break;
        case TY_POINTER:
            fputs(", \x1b[1minnerType:\x1b[0m ", to);
            typePrint(to, ty->as.ptr.innerType, true);
            break;
        case TY_FUNCTION:
            fputs(", \x1b[1mreturnType:\x1b[0m ", to);
            typePrint(to, ty->as.ptr.innerType, true);
            fputs(", \x1b[1mparameterTypes:\x1b[0m [", to);
            ARRAY_FOR(i, ty->as.fn.parameterTypes) {
                typePrint(to, ARRAY_GET_AS(Type *, &ty->as.fn.parameterTypes, i), true);
                if(i + 1 < arrayLength(&ty->as.fn.parameterTypes)) {
                    fputs(", ", to);
                }
            }
            fputs("]", to);
            break;
        case TY_STRUCT:
            fputs(", \x1b[1mfieldTypes:\x1b[0m [", to);
            ARRAY_FOR(i, ty->as.structure.fieldTypes) {
                typePrint(to, ARRAY_GET_AS(Type *, &ty->as.structure.fieldTypes, i), true);
                if(i + 1 < arrayLength(&ty->as.structure.fieldTypes)) {
                    fputs(", ", to);
                }
            }
            fputs("]", to);
            break;
        case TY_IDENTIFIER:
            fprintf(to, ", \x1b[1mactualName:\x1b[0m '%s'", ty->as.id.actualName);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

Type *typeNew(TypeType type, ASTString name, Location declLocation, ModuleID declModule) {
    Type *ty;
    NEW0(ty);
    ty->type = type;
    ty->name = name;
    ty->declLocation = declLocation;
    ty->declModule = declModule;

    switch(type) {
        case TY_FUNCTION:
            arrayInit(&ty->as.fn.parameterTypes);
            break;
        case TY_STRUCT:
            arrayInit(&ty->as.structure.fieldTypes);
            break;
        case TY_SCOPE_RESOLUTION:
            arrayInit(&ty->as.scopeResolution.path);
            break;
        default:
            break;
    }

    return ty;
}

void typeFree(Type *ty) {
    switch(ty->type) {
        case TY_FUNCTION:
            // Note: types are owned by modules, and so should be freed by them as well.
            //       (refering to the parameter types here.)
            arrayFree(&ty->as.fn.parameterTypes);
            break;
        case TY_STRUCT:
            arrayFree(&ty->as.structure.fieldTypes);
            break;
        case TY_SCOPE_RESOLUTION:
            // Note: types are owned by modules, and so should be freed by them as well.
            //       (refering to the scope res. types here.)
            arrayFree(&ty->as.scopeResolution.path);
            break;
        default:
            break;
    }
    FREE(ty);
}

bool typeEqual(Type *a, Type *b) {
    // Primitive types are equal regardless of which module they are in.
    if(typeIsPrimitive(a) && typeIsPrimitive(b)) {
        return a->type == b->type;
    }

    // Equal types will have the same TypeType, be in the same module (i.e. declModule points to the same memory address),
    // and have the same name (there is only one copy of each ASTString, so we can compare memory addresses here as well.)
    if(a->type != b->type || a->declModule != b->declModule || a->name != b->name) {
        return false;
    }

    switch(a->type) {
        case TY_POINTER:
            return typeEqual(a->as.ptr.innerType, b->as.ptr.innerType);
        case TY_FUNCTION:
            if(!typeEqual(a->as.fn.returnType, b->as.fn.returnType)) {
                return false;
            }
            if(arrayLength(&a->as.fn.parameterTypes) != arrayLength(&b->as.fn.parameterTypes)) {
                return false;
            }
            ARRAY_FOR(i, a->as.fn.parameterTypes) {
                Type *aParam = ARRAY_GET_AS(Type *, &a->as.fn.parameterTypes, i);
                Type *bParam = ARRAY_GET_AS(Type *, &b->as.fn.parameterTypes, i);
                if(!typeEqual(aParam, bParam)) {
                    return false;
                }
            }
            break;
        case TY_STRUCT:
            if(arrayLength(&a->as.structure.fieldTypes) != arrayLength(&b->as.structure.fieldTypes)) {
                return false;
            }
            ARRAY_FOR(i, a->as.structure.fieldTypes) {
                Type *aField = ARRAY_GET_AS(Type *, &a->as.structure.fieldTypes, i);
                Type *bField = ARRAY_GET_AS(Type *, &b->as.structure.fieldTypes, i);
                if(!typeEqual(aField, bField)) {
                    return false;
                }
            }
            break;
        case TY_IDENTIFIER:
            // TODO: maybe actualName == actualName???
            //       I'm pretty sure this would be correct since we make sure both types are in same module,
            //       however I don't think identifier types should be comparable.
        default:
            UNREACHABLE();
    }

    return true;
}

bool typeIsPrimitive(Type *ty) {
    switch(ty->type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
        case TY_BOOL:
            return true;
        default:
            break; // The return is not here to make it clearer that all execution paths are covered.
    }
    return false;
}
