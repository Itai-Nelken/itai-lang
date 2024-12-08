#include "common.h"
#include "memory.h"
#include "Token.h"
#include "Ast/StringTable.h"
#include "Ast/Module.h"
#include "Ast/Type.h"

Type *typeNew(TypeType type, ASTString name, Location declLocation, ASTModule *declModule) {
    Type *ty;
    NEW0(ty);
    ty->type = type;
    ty->name = name;
    ty->declLocation = declLocation;
    ty->declModule = declModule;

    return ty;
}

void typeFree(Type *ty) {
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
