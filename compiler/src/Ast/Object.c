#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Ast/StringTable.h"
#include "Ast/Type.h"
#include "Ast/Object.h"

static inline const char *obj_type_to_string(ASTObjType type) {
    VERIFY(type < OBJ_TYPE_COUNT);
    static const char *names[] = {
        [OBJ_VAR] = "OBJ_VAR"
    };
    return names[(int)type];
}

void astObjectPrint(FILE *to, ASTObj *obj, bool compact) {
    if(!obj) {
        fputs("(null)", to);
        return;
    }
    if(compact) {
        fprintf(to, "ASTObj{\x1b[1;33m%s\x1b[0m, '%s', ", obj_type_to_string(obj->type));
        typePrint(to, obj->dataType, true);
        fputc('}', to);
        return;
    }

    fprintf(to, "ASTObj{\x1b[1mtype: \x1b[33m%s\x1b[0;1m, name:\x1b[0m '%s', \x1b[1mdataType:\x1b[0m", obj_type_to_string(obj->type), obj->name);
    typePrint(to, obj->dataType, false);
    // TODO: implement OBJ_TYPE-specific pretty-printing (params for fn, fields for struct etc.)
    fputc('}', to);
}

ASTObj *astObjectNew(ASTObjType type, ASTString name, Type *dataType) {
    ASTObj *obj;
    NEW0(obj);
    obj->type = type;
    obj->name = name;
    obj->dataType = dataType;

    return obj;
}

void astObjectFree(ASTObj *obj) {
    FREE(obj);
}
