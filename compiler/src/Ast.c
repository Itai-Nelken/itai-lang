#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"
#include "Types.h"
#include "Ast.h"

/** callbacks **/

static void free_string_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    stringFree((String)item->value);
}

static void free_node_callback(void *node, void *cl) {
    UNUSED(cl);
    astNodeFree(AS_NODE(node));
}

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astFreeObj((ASTObj *)object);
}

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astModuleFree((ASTModule *)module);
}

static unsigned hash_type(void *type) {
    Type *ty = (Type *)type;
    // The hash will overflow unsigned, so it will wrap around.
    unsigned hash = (unsigned)(ty->type ^ (u64)type >> 3);
    return hash;
}

static bool compare_type(void *type1, void *type2) {
    Type *ty1 = (Type *)type1;
    Type *ty2 = (Type *)type2;
    return typeEqual(ty1, ty2);
}

static void free_type_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    Type *ty = (Type *)item->key;
    FREE(ty);
}

static void print_type_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    Type *ty = (Type *)item->key;
    typePrint(to, ty);
    if(!is_last) {
        fputs(", ", to);
    }
}

static void print_string_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    String s = (String)item->key;
    fprintf(to, "\"%s\"", s);
    if(!is_last) {
        fputs(", ", to);
    }
}

/** ASTModule **/
ASTModule *astModuleNew(ASTString name) {
    ASTModule *m;
    NEW0(m);
    m->name = name;
    arrayInit(&m->objects);
    arrayInit(&m->globals);
    return m;
}

void astModuleFree(ASTModule *module) {
    arrayMap(&module->objects, free_object_callback, NULL);
    arrayFree(&module->objects);
    arrayMap(&module->globals, free_node_callback, NULL);
    arrayFree(&module->globals);
    FREE(module);
}

void astModulePrint(FILE *to, ASTModule *module) {
    fprintf(to, "ASTModule{\x1b[1mname:\x1b[0m '%s', \x1b[1mobjects:\x1b[0m [", module->name);
    for(usize i = 0 ; i < module->objects.used; ++i) {
        ASTObj *o = ARRAY_GET_AS(ASTObj *, &module->objects, i);
        astPrintObj(to, o);
        // If not last, add a ',' (comma).
        if(i + 1 < module->objects.used) {
            fputs(", ", to);
        }
    }

    fputs("], \x1b[1mglobals:\x1b[0m [", to);
    for(usize i = 0 ; i < module->globals.used; ++i) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &module->globals, i);
        astNodePrint(to, n);
        // If not last, add a ',' (comma).
        if(i + 1 < module->globals.used) {
            fputs(", ", to);
        }
    }

    fputs("]}", to);
}

/** ASTProgram **/

static void init_primitive_types(ASTProgram *prog) {
#define DEF(typename, type_, size_) {Type *ty; NEW0(ty); ty->type = type_; ty->size = size_; prog->primitives.typename = astProgramAddType(prog, ty);}

    DEF(int32, TY_I32, 4);
    DEF(uint32, TY_U32, 4);

#undef DEF
}

void astProgramInit(ASTProgram *prog) {
    tableInit(&prog->strings, NULL, NULL);
    tableInit(&prog->type_table, hash_type, compare_type);
    init_primitive_types(prog);
    arrayInit(&prog->modules);
}

void astProgramFree(ASTProgram *prog) {
    tableMap(&prog->strings, free_string_callback, NULL);
    tableFree(&prog->strings);
    tableMap(&prog->type_table, free_type_callback, NULL);
    tableFree(&prog->type_table);
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
}

void astProgramPrint(FILE *to, ASTProgram *prog) {
    fputs("ASTProgram{\x1b[1mmodules:\x1b[0m [", to);
    for(usize i = 0 ; i < prog->modules.used; ++i) {
        ASTModule *m = ARRAY_GET_AS(ASTModule *, &prog->modules, i);
        astModulePrint(to, m);
        // If not last, add a ',' (comma).
        if(i + 1 < prog->modules.used) {
            fputs(", ", to);
        }
    }
    fputc(']', to);

    fputs(", \x1b[1mstrings:\x1b[0m [", to);
    tableMap(&prog->strings, print_string_table_callback, (void *)to);
    fputs("], \x1b[1mtype_table:\x1b[0m [", to);
    tableMap(&prog->type_table, print_type_table_callback, (void *)to);
    fputs("]}", to);
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

Type *astProgramAddType(ASTProgram *prog, Type *ty) {
    TableItem *existing_item;
    if((existing_item = tableGet(&prog->type_table, (void *)ty)) != NULL) {
        FREE(ty);
        return (Type *)existing_item->key;
    }
    tableSet(&prog->type_table, (void *)ty, NULL);
    return ty;
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

void astNodeFree(ASTNode *n) {
    if(n == NULL) {
        return;
    }
    switch(n->node_type) {
        case ND_VARIABLE: // fallthrough
        case ND_NUMBER_LITERAL:
            // nothing
        break;
        case ND_ASSIGN:
            astNodeFree(AS_BINARY_NODE(n)->lhs);
            astNodeFree(AS_BINARY_NODE(n)->rhs);
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

void astNodePrint(FILE *to, ASTNode *n) {
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
            astNodePrint(to, AS_BINARY_NODE(n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astNodePrint(to, AS_BINARY_NODE(n)->rhs);
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
            fputs(", \x1b[1mtype:\x1b[0m ", to);
            typePrint(to, obj->as.var.type);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}
