#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "Array.h"
#include "Token.h" // Location
#include "Ast/CheckedAst.h"
#include "Types/types_common.h"
#include "Types/CheckedType.h"

void checkedTypeInit(CheckedType *ty, TypeType type, ASTString name, ModuleID decl_module) {
    ty->type = type;
    ty->name = name;
    ty->decl_module = decl_module;
    ty->decl_location = EMPTY_LOCATION;
    switch(type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
        case TY_PTR:
        case TY_ID:
            // nothing
            break;
        case TY_FN:
            ty->as.fn_obj = NULL;
            break;
        case TY_STRUCT:
            ty->as.struct_obj = NULL;
            break;
        default:
            UNREACHABLE();
    }
}

void checkedTypeFree(CheckedType *ty) {
    switch(ty->type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
        case TY_PTR: // The inner type isn't owned by the pointer type.
        case TY_ID:
            // nothing
            break;
        case TY_FN:
            ty->as.fn_obj = NULL;
            break;
        case TY_STRUCT:
            ty->as.struct_obj = NULL;
            break;
        default:
            UNREACHABLE();
    }
}

bool checkedTypeEqual(CheckedType *a, CheckedType *b) {
    VERIFY(a && b);
    if(a->type != b->type) {
        return false;
    }

    // Compare function types regardless of their address
    // because function types are equal even if they are in
    // different modules.
    if(a->type == TY_FN) {
        if(!checkedTypeEqual(a->as.fn_obj->as.fn.return_type, b->as.fn_obj->as.fn.return_type)) {
            return false;
        }
        if(arrayLength(&a->as.fn_obj->as.fn.parameters) != arrayLength(&b->as.fn_obj->as.fn.parameters)) {
            return false;
        }
        // past here, both function types have the same return type and parameter count.
        ARRAY_FOR(i, a->as.fn_obj->as.fn.parameters) {
            ASTObj *a_param = ARRAY_GET_AS(ASTObj *, &a->as.fn_obj->as.fn.parameters, i);
            ASTObj *b_param = ARRAY_GET_AS(ASTObj *, &b->as.fn_obj->as.fn.parameters, i);
            if(!typeEqual(a_param->data_type, b_param->data_type)) {
                return false;
            }
        }
        return true;
    } else if(a->type == TY_PTR) {
        return checkedTypeEqual(a->as.ptr.inner_type, b->as.ptr.inner_type);
    }

    if(a->decl_module != b->decl_module) {
        return false;
    }
    // If both types are from the same module (above test), we can simply compare their addresses in memory.
    return a == b;
}

unsigned checkedTypeHash(CheckedType *ty) {
    // hash the name using the fnv-la string hashing algorithm.
    unsigned length = (unsigned)stringLength(ty->name.data);
    unsigned hash = 2166136261u;
    for(unsigned i = 0; i < length; ++i) {
        hash ^= (char)ty->name.data[i];
        hash *= 16777619;
    }
    if(ty->type == TY_FN) {
        ARRAY_FOR(i, ty->as.fn_obj->as.fn.parameters) {
            hash |= checkedTypeHash(ARRAY_GET_AS(ASTObj *, &ty->as.fn_obj->as.fn.parameters, i)->data_type);
        }
        hash &= checkedTypeHash(ty->as.fn_obj->as.fn.return_type);
    } else if(ty->type == TY_PTR) {
        hash &= checkedTypeHash(ty->as.ptr.inner_type);
    }
    return (unsigned)((ty->type + (uintptr_t)hash) >> 2); // Hash is cast to 'unsigned', so extra bits are discarded.
}

void checkedTypePrint(FILE *to, CheckedType *ty, bool compact) {
    if(ty == NULL) {
        fputs("(null)", to);
        return;
    }

    if(compact) {
        fprintf(to, "CheckedType{\x1b[1m%s\x1b[0m", type_type_name(ty->type));
        if(ty->type == TY_ID || ty->type == TY_STRUCT || ty->type == TY_FN) {
            fputs(", ", to);
            astStringPrint(to, &ty->name);
        } else if(ty->type == TY_PTR) {
            fputs(", \x1b[1minner:\x1b[0m ", to);
            typePrint(to, ty->as.ptr.inner_type, true);
        }
        fputc('}', to);
    } else {
        fprintf(to, "CheckedType{\x1b[1mtype:\x1b[0m %s", type_type_name(ty->type));
        fputs(", \x1b[1mname:\x1b[0m ", to);
        astStringPrint(to, &ty->name);
        switch(ty->type) {
            case TY_FN:
                fputs(", \x1b[1mfn_obj:\x1b[0m ", to);
                astObjPrint(to, ty->as.fn_obj);
                break;
            case TY_STRUCT: {
                fputs(", \x1b[1mstruct_obj:\x1b[0m ", to);
                astObjPrint(to, ty->as.struct_obj);
                break;
            }
            case TY_PTR:
                fputs(", \x1b[1minner_type:\x1b[0m ", to);
                typePrint(to, ty->as.ptr.inner_type, true);
                break;
            case TY_VOID:
            case TY_I32:
            case TY_U32:
            case TY_STR:
            case TY_ID:
                break;
            default:
                UNREACHABLE();
        }
        fputc('}', to);
    }
}
