#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "Token.h" // Location
#include "Ast.h"
#include "Array.h"
#include "Types.h"

void typeInit(Type *ty, TypeType type, ASTString name, ModuleID decl_module) {
    ty->type = type;
    ty->name = name;
    ty->decl_module = decl_module;
    ty->decl_location = EMPTY_LOCATION();
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

void typeFree(Type *ty) {
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

bool typeIsNumeric(Type *ty) {
    switch(ty->type) {
        case TY_I32:
        case TY_U32:
            return true;
        default:
            break;
    }
    return false;
}

bool typeIsSigned(Type *ty) {
    VERIFY(IS_NUMERIC(ty));
    switch(ty->type) {
        case TY_I32:
            return true;
        default:
            break;
    }
    return false;
}

bool typeIsUnsigned(Type *ty) {
    VERIFY(IS_NUMERIC(ty));
    switch(ty->type) {
        case TY_U32:
            return true;
        default:
            break;
    }
    return false;
}

bool typeIsPrimitive(Type *ty) {
    switch(ty->type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
            return true;
        default:
            break;
    }
    return false;
}

bool typeIsFunction(Type *ty) {
    return ty->type == TY_FN;
}

bool typeEqual(Type *a, Type *b) {
    VERIFY(a && b);
    if(a->type != b->type) {
        return false;
    }

    // Compare function types regardless of their address
    // because function types are equal even if they are in
    // different modules.
    if(a->type == TY_FN) {
        if(!typeEqual(a->as.fn.return_type, b->as.fn.return_type)) {
            return false;
        }
        if(a->as.fn.parameter_types.used != b->as.fn.parameter_types.used) {
            return false;
        }
        // past here, both function types have the same return type and parameter count.
        for(usize i = 0; i < a->as.fn.parameter_types.used; ++i) {
            Type *a_param = ARRAY_GET_AS(Type *, &a->as.fn.parameter_types, i);
            Type *b_param = ARRAY_GET_AS(Type *, &b->as.fn.parameter_types, i);
            if(!typeEqual(a_param, b_param)) {
                return false;
            }
        }
        return true;
    } else if(a->type == TY_PTR) {
        return typeEqual(a->as.ptr.inner_type, b->as.ptr.inner_type);
    }

    if(a->decl_module != b->decl_module) {
        return false;
    }
    // If both types are from the same module (above test), we can simply compare their addresses.
    return a == b;
/*
    // from here on, both types have the same TY_* type and are in the same module.
    if(a->type == TY_FN) {
        if(!typeEqual(a->as.fn.return_type, b->as.fn.return_type)) {
            return false;
        }
        if(a->as.fn.parameter_types.used != b->as.fn.parameter_types.used) {
            return false;
        }
        // past here, both function types have the same return type and parameter count.
        for(usize i = 0; i < a->as.fn.parameter_types.used; ++i) {
            Type *a_param = ARRAY_GET_AS(Type *, &a->as.fn.parameter_types, i);
            Type *b_param = ARRAY_GET_AS(Type *, &b->as.fn.parameter_types, i);
            if(!typeEqual(a_param, b_param)) {
                return false;
            }
        }
    } else if(a->type == TY_STRUCT) {
        if(a->as.structure.field_types.used != b->as.structure.field_types.used) {
            return false;
        }
        for(usize i = 0; i < a->as.structure.field_types.used; ++i) {
            Type *a_field = ARRAY_GET_AS(Type *, &a->as.structure.field_types, i);
            Type *b_field = ARRAY_GET_AS(Type *, &b->as.structure.field_types, i);
            if(!typeEqual(a_field, b_field)) {
                return false;
            }
        }
    } else if(a->type == TY_ID) {
        // It is ok to check the names as we already made sure
        // both types are in the same module.
        if(!stringEqual(a->name, b->name)) {
            return false;
        }
    }

    return true;
*/
}

static const char *type_type_name(TypeType type) {
    static const char *names[] = {
        [TY_VOID]   = "TY_VOID",
        [TY_I32]    = "TY_I32",
        [TY_U32]    = "TY_U32",
        [TY_STR]    = "TY_STR",
        [TY_PTR]    = "TY_PTR",
        [TY_FN]     = "TY_FN",
        [TY_STRUCT] = "TY_STRUCT",
        [TY_ID]     = "TY_ID"
    };
    return names[type];
}

void typePrint(FILE *to, Type *ty, bool compact) {
    VERIFY(ty);
    //if(ty == NULL) {
    //    fputs("(null)", to);
    //    return;
    //}

    if(compact) {
        fprintf(to, "Type{\x1b[1m%s\x1b[0m", type_type_name(ty->type));
        if(ty->type == TY_ID || ty->type == TY_STRUCT || ty->type == TY_FN) {
            fprintf(to, ", %s", ty->name);
        } else if(ty->type == TY_PTR) {
            fputs(", \x1b[1minner:\x1b[0m ", to);
            typePrint(to, ty->as.ptr.inner_type, true);
        }
        fputc('}', to);
    } else {
        fprintf(to, "Type{\x1b[1mtype:\x1b[0m %s", type_type_name(ty->type));
        fprintf(to, ", \x1b[1mname:\x1b[0m '%s'", ty->name);
        switch(ty->type) {
            case TY_FN:
                fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
                typePrint(to, ty->as.fn.return_type, true);
                fputs(", \x1b[1mparameter_types:\x1b[0m [", to);
                for(usize i = 0; i < ty->as.fn.parameter_types.used; ++i) {
                    Type *param_ty = ARRAY_GET_AS(Type *, &ty->as.fn.parameter_types, i);
                    typePrint(to, param_ty, true);
                    if(i + 1 < ty->as.fn.parameter_types.used) {
                        fputs(", ", to);
                    }
                }
                fputc(']', to);
                break;
            case TY_STRUCT:
                fputs(", \x1b[1mfield_types:\x1b[0m [", to);
                for(usize i = 0; i < ty->as.structure.field_types.used; ++i) {
                    Type *field_ty = ARRAY_GET_AS(Type *, &ty->as.structure.field_types, i);
                    typePrint(to, field_ty, true);
                    if(i + 1 < ty->as.structure.field_types.used) {
                        fputs(", ", to);
                    }
                }
                fputc(']', to);
                break;
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
