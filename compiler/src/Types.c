#include <stdbool.h>
#include "Ast.h"
#include "Types.h"

void typeInit(Type *ty, TypeType type, ASTString name, int size) {
    ty->type = type;
    ty->name = name;
    ty->size = size;
}

bool typeEqual(Type *a, Type *b) {
    // TODO: Properly compare types:
    //       pointers - base types,
    //       etc.
    if(!a || !b) {
        return false;
    }

    if(a->type != b->type) {
        return false;
    }
    // from here on, both types have the same TY_* type.
    if(a->type == TY_FN) {
        return typeEqual(a->as.fn.return_type, b->as.fn.return_type);
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
                typePrint(to, ty->as.fn.return_type, compact);
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
