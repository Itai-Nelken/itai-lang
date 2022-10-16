#include <stdbool.h>
#include "Types.h"

void typeInit(Type *ty, TypeType type, int size) {
    ty->type = type;
    ty->size = size;
}

bool typeEqual(Type *a, Type *b) {
    if(a->type != b->type) {
        return false;
    }
    return true;
}

static const char *type_type_name(TypeType type) {
    static const char *names[] = {
        [TY_I32] = "TY_I32"
    };
    return names[type];
}

void typePrint(FILE *to, Type *ty) {
    if(ty == NULL) {
        fputs("(null)", to);
        return;
    }

    fprintf(to, "Type{\x1b[1mtype:\x1b[0m %s", type_type_name(ty->type));
    fprintf(to, ", \x1b[1msize:\x1b[0m %d}", ty->size);
}
