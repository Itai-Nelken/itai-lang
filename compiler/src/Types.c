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
    //       functions - signature
    //       etc.
    if(a->type != b->type) {
        return false;
    }
    return true;
}

static const char *type_type_name(TypeType type) {
    static const char *names[] = {
        [TY_I32] = "TY_I32",
        [TY_U32] = "TY_U32"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == TY_COUNT, "Missing type(s) in type_type_name()");
    return names[type];
}

void typePrint(FILE *to, Type *ty) {
    if(ty == NULL) {
        fputs("(null)", to);
        return;
    }

    fprintf(to, "Type{\x1b[1mtype:\x1b[0m %s", type_type_name(ty->type));
    fprintf(to, ", \x1b[1mname:\x1b[0m %s", ty->name);
    fprintf(to, ", \x1b[1msize:\x1b[0m %d}", ty->size);
}
