#include "memory.h"
#include "Ast/StringTable.h"
#include "Ast/Type.h"
#include "Ast/Object.h"

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
