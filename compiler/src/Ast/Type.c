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
        [TY_VOID]     = "TY_VOID",
        [TY_I32]      = "TY_I32",
        //[TY_U32]      = "TY_U32",
        //[TY_STR]      = "TY_STR",
        [TY_POINTER]  = "TY_POINTER",
        [TY_FUNCTION] = "TY_FUNCTION",
        [TY_STRUCT]   = "TY_STRUCT"
    };
    return names[(int)type];
}

void typePrint(FILE *to, Type *ty, bool compact) {
    if(!ty) {
        fputs("(null)", to);
        return;
    }
    if(compact) {
        fprintf(to, "Type{\x1b[1;33m%s\x1b[0m", type_type_to_string(ty->type));
        if(ty->type == TY_POINTER) {
            //fprintf(to, ", \x1b[1minner:\x1b[0m ");
            //typePrint(to, ty->as.ptr.innerType, true);
            LOG_ERR("Pointer pretty-printing not implemented yet.");
            UNREACHABLE();
        } else {
            // FIXME: always print name?
            fprintf(to, ", '%s'", ty->name);
        }
        fputc('}', to);
        return;
    }
    // verbose output
    fprintf(to, "Type{\x1b[1mtype:\x1b[33m %s\x1b[0m", type_type_to_string(ty->type));
    fprintf(to, ", \x1b[1mname:\x1b[0m '%s', \x1b[1mdeclLocation:\x1b[0m ", ty->name);
    locationPrint(to, ty->declLocation, true);
    fprintf(to, ", \x1b[1mdeclModule:\x1b[0;34m %zu\x1b[0m", ty->declModule);
    //switch(ty->type) {
    //    // TODO: handle ptr, fn, struct types
    //    default:
    //        UNREACHABLE();
    //}
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

    //switch(a->type) {
    //    case TY_POINTER:
    //        return typeEqual(a->as.ptr.innerType, b->as.ptr.innerType);
    //    case TY_FUNCTION:
    //        // check return type, parameter types
    //    case TY_STRUCT:
    //        // check field types.
    //    default:
    //        UNREACHABLE();
    //}

    return true;
}

bool typeIsPrimitive(Type *ty) {
    switch(ty->type) {
        case TY_VOID:
        case TY_I32:
            return true;
        default:
            break; // The return is not here to make it clearer that all execution paths are covered.
    }
    return false;
}
