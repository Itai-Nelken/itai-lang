#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Ast/Ast.h"

static inline const char *obj_type_to_string(ASTObjType type) {
    VERIFY(type < OBJ_TYPE_COUNT);
    static const char *names[] = {
        [OBJ_VAR] = "OBJ_VAR",
        [OBJ_FN]  = "OBJ_FN"
    };
    return names[(int)type];
}

void astObjectPrint(FILE *to, ASTObj *obj, bool compact) {
    if(!obj) {
        fputs("(null)", to);
        return;
    }
    if(compact) {
        fprintf(to, "ASTObj{\x1b[1;33m%s\x1b[0m, '%s', ", obj_type_to_string(obj->type), obj->name);
        typePrint(to, obj->dataType, true);
        fputc('}', to);
        return;
    }

    fprintf(to, "ASTObj{\x1b[1mtype: \x1b[33m%s\x1b[0;1m, name:\x1b[0m '%s', \x1b[1mdataType:\x1b[0m", obj_type_to_string(obj->type), obj->name);
    typePrint(to, obj->dataType, false);
    switch(obj->type) {
        case OBJ_FN:
            fputs(", \x1b[1mparameters:\x1b[0m :", to);
            ARRAY_FOR(i, obj->as.fn.parameters) {
                ASTObj *param = ARRAY_GET_AS(ASTObj *, &obj->as.fn.parameters, i);
                astObjectPrint(to, param, true);
                if(i + 1 < arrayLength(&obj->as.fn.parameters)) {
                    fputs(", ", to);
                }
            }
            fputs("], \x1b[1mreturnType:\x1b[0m ", to);
            typePrint(to, obj->as.fn.returnType, true);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astStmtPrint(to, NODE_AS(ASTStmtNode, obj->as.fn.body));
        default:
            break;
    }
    fputc('}', to);
}

ASTObj *astObjectNew(ASTObjType type, Location loc, ASTString name, Type *dataType) {
    ASTObj *obj;
    NEW0(obj);
    obj->type = type;
    obj->loc = loc;
    obj->name = name;
    obj->dataType = dataType;

    switch(type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            arrayInit(&obj->as.fn.parameters);
            break;
        default:
            UNREACHABLE();
    }

    return obj;
}

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astObjectFree((ASTObj *)object);
}

void astObjectFree(ASTObj *obj) {
    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            // TODO: objects should REALLY be stored in an arena. That would save so much pain.
            arrayMap(&obj->as.fn.parameters, free_object_callback, NULL);
            arrayFree(&obj->as.fn.parameters);
            break;
        default:
            UNREACHABLE();

    }
    FREE(obj);
}
