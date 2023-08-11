#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "Array.h"
#include "Token.h" // Location
#include "Ast/ParsedAst.h"
#include "Types/types_common.h"
#include "Types/ParsedType.h"

void parsedTypeInit(ParsedType *ty, TypeType type, ASTString name, ModuleID decl_module) {
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
            arrayInit(&ty->as.fn.parameter_types);
            break;
        case TY_STRUCT:
            arrayInit(&ty->as.structure.field_types);
            break;
        default:
            UNREACHABLE();
    }
}

void parsedTypeFree(ParsedType *ty) {
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
            // No need to free the actual types as ownership of them is not taken.
            arrayFree(&ty->as.fn.parameter_types);
            break;
        case TY_STRUCT:
            // No need to free the actual types as ownership of them is not taken.
            arrayFree(&ty->as.structure.field_types);
            break;
        default:
            UNREACHABLE();
    }
}


bool parsedTypeEqual(ParsedType *a, ParsedType *b) {
    VERIFY(a && b);
    if(a->type != b->type) {
        return false;
    }

    // Compare function types regardless of their address
    // because function types are equal even if they are in
    // different modules.
    if(a->type == TY_FN) {
        if(!parsedTypeEqual(a->as.fn.return_type, b->as.fn.return_type)) {
            return false;
        }
        if(a->as.fn.parameter_types.used != b->as.fn.parameter_types.used) {
            return false;
        }
        // past here, both function types have the same return type and parameter count.
        for(usize i = 0; i < a->as.fn.parameter_types.used; ++i) {
            ParsedType *a_param = ARRAY_GET_AS(ParsedType *, &a->as.fn.parameter_types, i);
            ParsedType *b_param = ARRAY_GET_AS(ParsedType *, &b->as.fn.parameter_types, i);
            if(!parsedTypeEqual(a_param, b_param)) {
                return false;
            }
        }
        return true;
    } else if(a->type == TY_PTR) {
        return parsedTypeEqual(a->as.ptr.inner_type, b->as.ptr.inner_type);
    }

    if(a->decl_module != b->decl_module) {
        return false;
    }
    // If both types are from the same module (above test), we can simply compare their addresses.
    return a == b;
}

unsigned parsedTypeHash(ParsedType *ty) {
    // hash the name using the fnv-la string hashing algorithm.
    unsigned length = (unsigned)strlen(ty->name.data);
    unsigned hash = 2166136261u;
    for(unsigned i = 0; i < length; ++i) {
        hash ^= (char)ty->name.data[i];
        hash *= 16777619;
    }
    if(ty->type == TY_FN) {
        for(usize i = 0; i < ty->as.fn.parameter_types.used; ++i) {
            hash |= parsedTypeHash(ARRAY_GET_AS(ParsedType *, &ty->as.fn.parameter_types, i));
        }
        hash &= parsedTypeHash(ty->as.fn.return_type);
    } else if(ty->type == TY_PTR) {
        hash &= parsedTypeHash(ty->as.ptr.inner_type);
    }
    return (unsigned)((ty->type + (uintptr_t)hash) >> 2); // Hash is cast to 'unsigned', so extra bits are discarded.
}

void parsedTypePrint(FILE *to, ParsedType *ty, bool compact) {
    VERIFY(ty);
    if(compact) {
        fprintf(to, "ParsedType{\x1b[1m%s\x1b[0m", type_type_name(ty->type));
        if(ty->type == TY_ID || ty->type == TY_STRUCT || ty->type == TY_FN) {
            fputs(", ", to);
            astStringPrint(to, &ty->name);
        } else if(ty->type == TY_PTR) {
            fputs(", \x1b[1minner:\x1b[0m ", to);
            parsedTypePrint(to, ty->as.ptr.inner_type, true);
        }
        fputc('}', to);
    } else {
        fprintf(to, "ParsedType{\x1b[1mtype:\x1b[0m %s", type_type_name(ty->type));
        fputs(", \x1b[1mname:\x1b[0m ", to);
        astStringPrint(to, &ty->name);
        switch(ty->type) {
            case TY_FN:
                fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
                parsedTypePrint(to, ty->as.fn.return_type, true);
                fputs(", \x1b[1mparameter_types:\x1b[0m [", to);
                for(usize i = 0; i < ty->as.fn.parameter_types.used; ++i) {
                    ParsedType *param_ty = ARRAY_GET_AS(ParsedType *, &ty->as.fn.parameter_types, i);
                    parsedTypePrint(to, param_ty, true);
                    if(i + 1 < ty->as.fn.parameter_types.used) {
                        fputs(", ", to);
                    }
                }
                fputc(']', to);
                break;
            case TY_STRUCT:
                fputs(", \x1b[1mfield_types:\x1b[0m [", to);
                for(usize i = 0; i < ty->as.structure.field_types.used; ++i) {
                    ParsedType *field_ty = ARRAY_GET_AS(ParsedType *, &ty->as.structure.field_types, i);
                    parsedTypePrint(to, field_ty, true);
                    if(i + 1 < ty->as.structure.field_types.used) {
                        fputs(", ", to);
                    }
                }
                fputc(']', to);
                break;
            case TY_PTR:
                fputs(", \x1b[1minner_type:\x1b[0m ", to);
                parsedTypePrint(to, ty->as.ptr.inner_type, true);
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
