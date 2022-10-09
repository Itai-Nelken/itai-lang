#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"
#include "Ast.h"

/** callbacks **/

static void free_string_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    stringFree((String)item->value);
}

static void free_node_callback(void *node, void *cl) {
    UNUSED(cl);
    astFreeNode(AS_NODE(node));
}

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astFreeObj((ASTObj *)object);
}

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astFreeModule((ASTModule *)module);
}

/** ASTModule **/
ASTModule *astNewModule(ASTString name) {
    ASTModule *m;
    NEW0(m);
    m->name = name;
    arrayInit(&m->objects);
    arrayInit(&m->global_initializers);
    return m;
}

void astFreeModule(ASTModule *module) {
    arrayMap(&module->objects, free_object_callback, NULL);
    arrayFree(&module->objects);
    arrayMap(&module->global_initializers, free_node_callback, NULL);
    arrayFree(&module->global_initializers);
    FREE(module);
}

/** ASTProgram **/

void astProgramInit(ASTProgram *prog) {
    tableInit(&prog->strings, NULL, NULL);
    arrayInit(&prog->modules);
}

void astProgramFree(ASTProgram *prog) {
    tableMap(&prog->strings, free_string_callback, NULL);
    tableFree(&prog->strings);
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
}

ASTString astProgramAddString(ASTProgram *prog, char *str) {
    String string = str;
    if(!stringIsValid(str)) {
        string = stringCopy(str);
    }

    TableItem *existing_str = tableGet(&prog->strings, (void *)string);
    if(existing_str != NULL) {
        stringFree(string);
        return (ASTString)existing_str->value;
    }
    tableSet(&prog->strings, (void *)string, (void *)string);
    return (ASTString)string;
}

ModuleID astProgramAddModule(ASTProgram *prog, ASTModule *module) {
    return (ModuleID)arrayPush(&prog->modules, (void *)module);
}

ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID id) {
    // arrayGet() handles the id being out of bounds.
    return ARRAY_GET_AS(ASTModule *, &prog->modules, id);
}

/** ASTNode **/

void literalValuePrint(FILE *to, LiteralValue value) {
    fprintf(to, "LiteralValue{\x1b[1mvalue:\x1b[0m ");
    switch(value.type) {
        case LIT_NUMBER:
            fprintf(to, "\x1b[34m%lu\x1b[0m", value.as.number);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


static inline ASTNode make_header(ASTNodeType type, Location loc) {
    return (ASTNode){
        .node_type = type,
        .location = loc
    };
}

ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *lhs, ASTNode *rhs) {
    ASTBinaryNode *n;
    NEW0(n);
    n->header = make_header(type, loc);
    n->lhs = lhs;
    n->rhs = rhs;
    return AS_NODE(n);
}

ASTNode *astNewLiteralValueNode(ASTNodeType type, Location loc, LiteralValue value) {
    ASTLiteralValueNode *n;
    NEW0(n);
    n->header = make_header(type, loc);
    n->value = value;
    return AS_NODE(n);
}

ASTNode *astNewObjNode(ASTNodeType type, Location loc, ASTObj *obj) {
    ASTObjNode *n;
    NEW0(n);
    n->header = make_header(type, loc);
    n->obj = obj;
    return AS_NODE(n);
}

void astFreeNode(ASTNode *n) {
    if(n == NULL) {
        return;
    }
    switch(n->node_type) {
        case ND_VARIABLE: // fallthrough
        case ND_NUMBER_LITERAL:
            // nothing
        break;
        case ND_ASSIGN:
            astFreeNode(AS_BINARY_NODE(n)->lhs);
            astFreeNode(AS_BINARY_NODE(n)->rhs);
            break;
        default:
            UNREACHABLE();
    }
    FREE(n);
}

static const char *node_name(ASTNodeType type) {
    switch(type) {
        case ND_NUMBER_LITERAL:
            return "ASTLiteralValueNode";
        case ND_VARIABLE:
            return "ASTObjNode";
        case ND_ASSIGN:
            return "ASTBinaryNode";
        default:
            UNREACHABLE();
    }
}

static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_NUMBER_LITERAL] = "ND_NUMBER_LIERAL",
        [ND_VARIABLE]       = "ND_VARIABLE",
        [ND_ASSIGN]         = "ND_ASSIGN"
    };
    return names[type];
}

void astPrintNode(FILE *to, ASTNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mnode_type: \x1b[33m%s\x1b[0m", node_name(n->node_type), node_type_name(n->node_type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location);
    switch(n->node_type) {
        case ND_NUMBER_LITERAL:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            literalValuePrint(to, AS_LITERAL_NODE(n)->value);
            break;
        case ND_VARIABLE:
            fputs(", \x1b[1mobj:\x1b[0m ", to);
            astPrintObj(to, AS_OBJ_NODE(n)->obj);
            break;
        case ND_ASSIGN:
            fputs(", \x1b[1mlhs:\x1b[0m ", to);
            astPrintNode(to, AS_BINARY_NODE(n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astPrintNode(to, AS_BINARY_NODE(n)->rhs);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

ASTObj *astNewObj(ASTObjType type, Location loc) {
    ASTObj *o;
    NEW0(o);
    o->type = type;
    o->location = loc;
    return o;
}

void astFreeObj(ASTObj *obj) {
    if(obj == NULL) {
        return;
    }

    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        default:
            UNREACHABLE();
    }
    FREE(obj);
}

static const char *obj_type_name(ASTObjType type) {
    static const char *names[] = {
        [OBJ_VAR] = "OBJ_VAR"
    };
    return names[type];
}

void astPrintObj(FILE *to, ASTObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }

    fprintf(to, "ASTObj{\x1b[1mtype:\x1b[0m %s", obj_type_name(obj->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, obj->location);

    switch(obj->type) {
        case OBJ_VAR:
            fprintf(to, ", \x1b[1mname:\x1b[0m '%s'", obj->as.var.name);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}
