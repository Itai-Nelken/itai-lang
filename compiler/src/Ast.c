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
    astObjFree((ASTObj *)object);
}

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astModuleFree((ASTModule *)module);
}

static void free_block_scope_callback(void *scope, void *cl) {
    UNUSED(cl);
    blockScopeFree((BlockScope *)scope);
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

#define PRINT_ARRAY(type, print_fn, stream, array) for(usize i = 0; i < arrayLength(&(array)); ++i) { \
    print_fn((stream), ARRAY_GET_AS(type, &(array), i)); \
    if(i + 1 < (array).used) { \
        fputs(", ", (stream)); \
        } \
    }



/* ASTModule */

ASTModule *astModuleNew(ASTString name) {
    ASTModule *m;
    NEW0(m);
    m->name = name;
    arrayInit(&m->objects);
    arrayInit(&m->globals);
    tableInit(&m->types, hash_type, compare_type);
    return m;
}

void astModuleFree(ASTModule *module) {
    arrayMap(&module->objects, free_object_callback, NULL);
    arrayFree(&module->objects);
    arrayMap(&module->globals, free_node_callback, NULL);
    arrayFree(&module->globals);
    tableMap(&module->types, free_type_callback, NULL);
    tableFree(&module->types);
    FREE(module);
}

Type *astModuleAddType(ASTModule *module, Type *ty) {
    TableItem *existing_item;
    if((existing_item = tableGet(&module->types, (void *)ty)) != NULL) {
        FREE(ty);
        return (Type *)existing_item->key;
    }
    tableSet(&module->types, (void *)ty, NULL);
    return ty;
}

void astModulePrint(FILE *to, ASTModule *module) {
    fprintf(to, "ASTModule{\x1b[1mname:\x1b[0m '%s', \x1b[1mobjects:\x1b[0m [", module->name);
    PRINT_ARRAY(ASTObj *, astObjPrint, to, module->objects);
    fputs("], \x1b[1mglobals:\x1b[0m [", to);
    PRINT_ARRAY(ASTNode *, astNodePrint, to, module->globals);
    fputs("], \x1b[1mtypes:\x1b[0m [", to);
    tableMap(&module->types, print_type_table_callback, (void *)to);
    fputs("]}", to);
}


/* ASTProgram */

void astProgramInit(ASTProgram *prog) {
    tableInit(&prog->strings, NULL, NULL);
    arrayInit(&prog->modules);
#define NULL_TY(name) prog->primitives.name = NULL;
    NULL_TY(int32);
    NULL_TY(uint32);
#undef NULL_TY
}

void astProgramFree(ASTProgram *prog) {
    tableMap(&prog->strings, free_string_callback, NULL);
    tableFree(&prog->strings);
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
}

void astProgramPrint(FILE *to, ASTProgram *prog) {
    fputs("ASTProgram{\x1b[1mmodules:\x1b[0m [", to);
    PRINT_ARRAY(ASTModule *, astModulePrint, to, prog->modules);
    fputc(']', to);

    fputs(", \x1b[1mstrings:\x1b[0m [", to);
    tableMap(&prog->strings, print_string_table_callback, (void *)to);
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


/* LiteralValue */

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


/* BlockScope */

BlockScope *blockScopeNew(BlockScope *parent_scope, u32 depth) {
    BlockScope *sc;
    NEW0(sc);
    tableInit(&sc->visible_locals, NULL, NULL);
    arrayInit(&sc->children);
    sc->depth = depth;
    sc->parent = parent_scope;
    return sc;
}

ScopeID blockScopeAddChild(BlockScope *parent, BlockScope *child) {
    ScopeID id;
    id.depth = child->depth;
    id.index = arrayPush(&parent->children, (void *)child);
    return id;
}

BlockScope *blockScopeGetChild(BlockScope *parent, ScopeID child_id) {
    VERIFY(parent->depth < child_id.depth);
    BlockScope *child = ARRAY_GET_AS(BlockScope *, &parent->children, child_id.index);
    VERIFY(child);
    return child;
}

void blockScopeFree(BlockScope *scope_list) {
    // No need to free the strings in the table as they are
    // ASTString's which are owned by the ASTProgram
    // being used for the parse.
    // The same applies to the objects, but they are owned
    // by the parent function of the scope.
    tableFree(&scope_list->visible_locals);
    arrayMap(&scope_list->children, free_block_scope_callback, NULL);
    arrayFree(&scope_list->children);
    FREE(scope_list);
}


/* ASTNode */

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

ASTNode *astNewIdentifierNode(Location loc, ASTString str) {
    ASTIdentifierNode *n;
    NEW0(n);
    n->header = make_header(ND_IDENTIFIER, loc);
    n->identifier = str;
    return AS_NODE(n);
}

ASTNode *astNewBlockNode(Location loc, ScopeID scope) {
    ASTBlockNode *n;
    NEW0(n);
    n->header = make_header(ND_BLOCK, loc);
    n->scope = scope;
    arrayInit(&n->nodes);

    return AS_NODE(n);
}

void astNodeFree(ASTNode *n) {
    if(n == NULL) {
        return;
    }
    switch(n->node_type) {
        case ND_VARIABLE: // fallthrough
        case ND_NUMBER_LITERAL: // fallthrough
        case ND_IDENTIFIER:
            // nothing
        break;
        case ND_ASSIGN:
        case ND_ADD:
            astNodeFree(AS_BINARY_NODE(n)->lhs);
            astNodeFree(AS_BINARY_NODE(n)->rhs);
            break;
        case ND_BLOCK:
            arrayMap(&AS_BLOCK_NODE(n)->nodes, free_node_callback, NULL);
            arrayFree(&AS_BLOCK_NODE(n)->nodes);
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
        case ND_ASSIGN: // fallthrough
        case ND_ADD:
            return "ASTBinaryNode";
        case ND_IDENTIFIER:
            return "ASTIdentifierNode";
        case ND_BLOCK:
            return "ASTBlockNode";
        default:
            UNREACHABLE();
    }
}

static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_NUMBER_LITERAL] = "ND_NUMBER_LIERAL",
        [ND_VARIABLE]       = "ND_VARIABLE",
        [ND_ASSIGN]         = "ND_ASSIGN",
        [ND_ADD]            = "ND_ADD",
        [ND_BLOCK]          = "ND_BLOCK",
        [ND_IDENTIFIER]     = "ND_IDENTIFIER"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == ND_TYPE_COUNT, "Missing type(s) in node_type_name()");
    return names[type];
}

void astNodePrint(FILE *to, ASTNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mnode_type: \x1b[33m%s\x1b[0m", node_name(n->node_type), node_type_name(n->node_type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location, true);
    switch(n->node_type) {
        case ND_NUMBER_LITERAL:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            literalValuePrint(to, AS_LITERAL_NODE(n)->value);
            break;
        case ND_VARIABLE:
            fputs(", \x1b[1mobj:\x1b[0m ", to);
            astObjPrint(to, AS_OBJ_NODE(n)->obj);
            break;
        case ND_ASSIGN: // fallthrough
        case ND_ADD:
            fputs(", \x1b[1mlhs:\x1b[0m ", to);
            astNodePrint(to, AS_BINARY_NODE(n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astNodePrint(to, AS_BINARY_NODE(n)->rhs);
            break;
        case ND_IDENTIFIER:
            fprintf(to, ", \x1b[1midentifier:\x1b[0m '%s'", AS_IDENTIFIER_NODE(n)->identifier);
            break;
        case ND_BLOCK:
            fputs(", \x1b[1mnodes:\x1b[0m [", to);
            PRINT_ARRAY(ASTNode *, astNodePrint, to, AS_BLOCK_NODE(n)->nodes);
            fputc(']', to);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


/* ASTObj */

ASTObj *astNewObj(ASTObjType type, Location loc) {
    ASTObj *o;
    NEW0(o);
    o->type = type;
    o->location = loc;

    switch(type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            arrayInit(&o->as.fn.locals);
            break;
        default:
            UNREACHABLE();
    }

    return o;
}

void astObjFree(ASTObj *obj) {
    if(obj == NULL) {
        return;
    }

    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            arrayMap(&obj->as.fn.locals, free_object_callback, NULL);
            arrayFree(&obj->as.fn.locals);
            blockScopeFree(obj->as.fn.scopes);
            astNodeFree(AS_NODE(obj->as.fn.body));
            break;
        default:
            UNREACHABLE();
    }
    FREE(obj);
}

static const char *obj_type_name(ASTObjType type) {
    static const char *names[] = {
        [OBJ_VAR] = "OBJ_VAR",
        [OBJ_FN]  = "OBJ_FN"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == OBJ_TYPE_COUNT, "Missing type(s) in obj_type_name()");
    return names[type];
}

void astObjPrint(FILE *to, ASTObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }

    fprintf(to, "ASTObj{\x1b[1mtype: \x1b[36m%s\x1b[0m", obj_type_name(obj->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, obj->location, true);

    switch(obj->type) {
        case OBJ_VAR:
            fprintf(to, ", \x1b[1mname:\x1b[0m '%s'", obj->as.var.name);
            fputs(", \x1b[1mtype:\x1b[0m ", to);
            typePrint(to, obj->as.var.type);
            break;
        case OBJ_FN:
            fprintf(to, ", \x1b[1mname:\x1b[0m '%s'", obj->as.fn.name);
            fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
            typePrint(to, obj->as.fn.return_type);
            fputs(", \x1b[1mlocals:\x1b[0m [", to);
            PRINT_ARRAY(ASTObj *, astObjPrint, to, obj->as.fn.locals);
            fputs("], \x1b[1mbody:\x1b[0m ", to);
            astNodePrint(to, AS_NODE(obj->as.fn.body));
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

#undef PRINT_ARRAY
