#include <stdio.h>
#include <stdbool.h>
#include "Ast.h"
#include "Array.h"
#include "Types.h"

void typeInit(Type *ty, TypeType type, ASTString name, int size) {
    ty->type = type;
    ty->name = name;
    ty->size = size;
    if(type == TY_FN) {
        arrayInit(&ty->as.fn.parameter_types);
    }
}

void typeFree(Type *ty) {
    if(ty->type == TY_FN) {
        // No need to free the actual types as ownership of them is not taken.
        arrayFree(&ty->as.fn.parameter_types);
    }
}

bool typeIsNumeric(Type *ty) {
    // FIXME: no type should be represented as void/none instead of NULL.
    if(!ty) {
        return false;
    }
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
    // FIXME: no type should be represented as void/none instead of NULL.
    if(!ty) {
        return false;
    }
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
    // FIXME: no type should be represented as void/none instead of NULL.
    if(!ty) {
        return false;
    }
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
    // FIXME: no type should be represented as void/none instead of NULL.
    if(!ty) {
        return false;
    }
    switch(ty->type) {
        case TY_I32:
        case TY_U32:
            return true;
        default:
            break;
    }
    return false;
}

bool typeIsFunction(Type *ty) {
    // FIXME: no type should be represented as void/none instead of NULL.
    if(!ty) {
        return false;
    }
    return ty->type == TY_FN;
}

bool typeEqual(Type *a, Type *b) {
    // FIXME: no type should be represented as void/none instead of NULL.
    if(!a || !b) {
        return false;
    }

    if(a->type != b->type) {
        return false;
    }
    // from here on, both types have the same TY_* type.
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
    }

    return true;
}

static const char *type_type_name(TypeType type) {
    static const char *names[] = {
        [TY_I32] = "TY_I32",
        [TY_U32] = "TY_U32",
        [TY_FN]  = "TY_FN"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == TY_COUNT, "Missing type(s) in type_type_name()");
    return names[type];
}

void typePrint(FILE *to, Type *ty, bool compact) {
    if(ty == NULL) {
        fputs("(null)", to);
        return;
    }

    if(compact) {
        fprintf(to, "Type{\x1b[1m%s\x1b[0m}", type_type_name(ty->type));
    } else {
        fprintf(to, "Type{\x1b[1mtype:\x1b[0m %s", type_type_name(ty->type));
        fprintf(to, ", \x1b[1mname:\x1b[0m %s", ty->name);
        fprintf(to, ", \x1b[1msize:\x1b[0m %d", ty->size);
        switch(ty->type) {
            case TY_FN:
                fprintf(to, ", \x1b[1mreturn_type:\x1b[0m ");
                typePrint(to, ty->as.fn.return_type, true);
                fputs(", \x1b[1mparameter_types:\x1b[0m [", to);
                for(usize i = 0; i < ty->as.fn.parameter_types.used; ++i) {
                    Type *param_ty = ARRAY_GET_AS(Type *, &ty->as.fn.parameter_types, i);
                    typePrint(to, param_ty, true);
                }
                fputc(']', to);
                break;
            case TY_I32:
            case TY_U32:
                break;
            default:
                UNREACHABLE();
        }
        fputc('}', to);
    }
}
