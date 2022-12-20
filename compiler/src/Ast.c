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

static void free_type_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    Type *ty = (Type *)item->key;
    typeFree(ty);
    FREE(ty);
}

static unsigned hash_type(void *type) {
    Type *ty = (Type *)type;
    return (ty->type + (uintptr_t)ty->name) >> 2; // implicitly cast to 'unsigned', so extra bits are discarded.
}

static bool compare_type(void *a, void *b) {
    // FIXME: Is there a better way to do this?
    //        comparing types all the time isn't very efficient.
    return typeEqual((Type *)a, (Type *)b);
}

static void print_type_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    Type *ty = (Type *)item->key;
    typePrint(to, ty, false);
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
    if(scope_list == NULL) {
        return;
    }
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

void scopeIDPrint(FILE *to, ScopeID scope_id, bool compact) {
    if(compact) {
        fprintf(to, "ScopeID{\x1b[1md:\x1b[0;34m%u\x1b[0m,\x1b[1mi:\x1b[0;34m%zu\x1b[0m}", scope_id.depth, scope_id.index);
    } else {
        fprintf(to, "ScopeID{\x1b[1mdepth:\x1b[0;34m %u\x1b[0m, \x1b[1mindex:\x1b[0;34m %zu\x1b[0m}", scope_id.depth, scope_id.index);
    }
}


/* ControlFlow */

ControlFlow controlFlowUpdate(ControlFlow old, ControlFlow new) {
    if(old == new) {
        return old;
    }
    if(old == CF_MAY_RETURN || new == CF_MAY_RETURN) {
        return CF_MAY_RETURN;
    }
    if((old == CF_NEVER_RETURNS && new == CF_ALWAYS_RETURNS) ||
       (old == CF_ALWAYS_RETURNS && new == CF_NEVER_RETURNS)) {
        return CF_MAY_RETURN;
    }
    UNREACHABLE();
}


/* ASTNode */

static inline ASTNode make_header(ASTNodeType type, Location loc) {
    return (ASTNode){
        .node_type = type,
        .location = loc
    };
}

ASTNode *astNewUnaryNode(ASTNodeType type, Location loc, ASTNode *operand) {
    ASTUnaryNode *n;
    NEW0(n);
    n->header = make_header(type, loc);
    n->operand = operand;
    return AS_NODE(n);
}

ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *lhs, ASTNode *rhs) {
    ASTBinaryNode *n;
    NEW0(n);
    n->header = make_header(type, loc);
    n->lhs = lhs;
    n->rhs = rhs;
    return AS_NODE(n);
}

ASTNode *astNewConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTNode *body, ASTNode *else_) {
    ASTConditionalNode *n;
    NEW0(n);
    n->header = make_header(type, loc);
    n->condition = condition;
    n->body = body;
    n->else_ = else_;
    return AS_NODE(n);
}

ASTNode *astNewLoopNode(Location loc, ASTNode *init, ASTNode *cond, ASTNode *inc, ASTNode *body) {
    ASTLoopNode *n;
    NEW0(n);
    n->header = make_header(ND_WHILE_LOOP, loc);
    n->initializer = init;
    n->condition = cond;
    n->increment = inc;
    n->body = body;
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

ASTNode *astNewListNode(ASTNodeType type, Location loc, ScopeID scope) {
    ASTListNode *n;
    NEW0(n);
    n->header = make_header(type, loc);
    n->scope = scope;
    n->control_flow = CF_NONE;
    arrayInit(&n->nodes);

    return AS_NODE(n);
}

void astNodeFree(ASTNode *n) {
    if(n == NULL) {
        return;
    }
    switch(n->node_type) {
        case ND_VAR_DECL:
        case ND_VARIABLE:
        case ND_FUNCTION:
        case ND_NUMBER_LITERAL:
        case ND_IDENTIFIER:
            // nothing
        break;
        case ND_ASSIGN:
        case ND_CALL:
        case ND_PROPERTY_ACCESS:
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
            astNodeFree(AS_BINARY_NODE(n)->lhs);
            astNodeFree(AS_BINARY_NODE(n)->rhs);
            break;
        case ND_IF:
            astNodeFree(AS_CONDITIONAL_NODE(n)->condition);
            astNodeFree(AS_CONDITIONAL_NODE(n)->body);
            astNodeFree(AS_CONDITIONAL_NODE(n)->else_);
            break;
        case ND_WHILE_LOOP:
            astNodeFree(AS_LOOP_NODE(n)->initializer);
            astNodeFree(AS_LOOP_NODE(n)->condition);
            astNodeFree(AS_LOOP_NODE(n)->increment);
            astNodeFree(AS_LOOP_NODE(n)->body);
            break;
        case ND_NEGATE:
        case ND_RETURN:
            astNodeFree(AS_UNARY_NODE(n)->operand);
            break;
        case ND_BLOCK:
        case ND_ARGS:
            arrayMap(&AS_LIST_NODE(n)->nodes, free_node_callback, NULL);
            arrayFree(&AS_LIST_NODE(n)->nodes);
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
        case ND_VAR_DECL:
        case ND_VARIABLE:
        case ND_FUNCTION:
            return "ASTObjNode";
        case ND_ASSIGN:
        case ND_CALL:
        case ND_PROPERTY_ACCESS:
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
            return "ASTBinaryNode";
        case ND_IF:
            return "ASTConditionalNode";
        case ND_WHILE_LOOP:
            return "ASTLoopNode";
        case ND_NEGATE:
        case ND_RETURN:
            return "ASTUnaryNode";
        case ND_IDENTIFIER:
            return "ASTIdentifierNode";
        case ND_BLOCK:
        case ND_ARGS:
            return "ASTListNode";
        default:
            UNREACHABLE();
    }
}

static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_NUMBER_LITERAL]  = "ND_NUMBER_LIERAL",
        [ND_VAR_DECL]        = "ND_VAR_DECL",
        [ND_VARIABLE]        = "ND_VARIABLE",
        [ND_FUNCTION]        = "ND_FUNCTION",
        [ND_ASSIGN]          = "ND_ASSIGN",
        [ND_CALL]            = "ND_CALL",
        [ND_PROPERTY_ACCESS] = "ND_PROPERTY_ACCESS",
        [ND_ADD]             = "ND_ADD",
        [ND_SUBTRACT]        = "ND_SUBTRACT",
        [ND_MULTIPLY]        = "ND_MULTIPLY",
        [ND_DIVIDE]          = "ND_DIVIDE",
        [ND_EQ]              = "ND_EQ",
        [ND_NE]              = "ND_NE",
        [ND_LT]              = "ND_LT",
        [ND_LE]              = "ND_LE",
        [ND_GT]              = "ND_GT",
        [ND_GE]              = "ND_GE",
        [ND_IF]              = "ND_IF",
        [ND_WHILE_LOOP]      = "ND_WHILE_LOOP",
        [ND_NEGATE]          = "ND_NEGATE",
        [ND_RETURN]          = "ND_RETURN",
        [ND_BLOCK]           = "ND_BLOCK",
        [ND_ARGS]            = "ND_ARGS",
        [ND_IDENTIFIER]      = "ND_IDENTIFIER"
    };
    // FIXME?: this static assert only fails if new node types are appended
    // to the end of the node type enum, something which almost never happens.
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
        case ND_VAR_DECL:
        case ND_VARIABLE:
        case ND_FUNCTION:
            fputs(", \x1b[1mobj:\x1b[0m ", to);
            astObjPrint(to, AS_OBJ_NODE(n)->obj);
            break;
        case ND_ASSIGN:
        case ND_CALL:
        case ND_PROPERTY_ACCESS:
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
            fputs(", \x1b[1mlhs:\x1b[0m ", to);
            astNodePrint(to, AS_BINARY_NODE(n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astNodePrint(to, AS_BINARY_NODE(n)->rhs);
            break;
        case ND_IF:
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astNodePrint(to, AS_CONDITIONAL_NODE(n)->condition);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astNodePrint(to, AS_CONDITIONAL_NODE(n)->body);
            fputs(", \x1b[1melse:\x1b[0m ", to);
            astNodePrint(to, AS_CONDITIONAL_NODE(n)->else_);
            break;
        case ND_WHILE_LOOP:
            // NOTE: ND_FOR_LOOP and ND_FOR_ITERATOR_LOOP
            //       should be printed separately because they use
            //       the rest of the clauses (initializer, increment).
            fputs(", \x1b[1mcondition:\x1b[0m", to);
            astNodePrint(to, AS_LOOP_NODE(n)->condition);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astNodePrint(to, AS_LOOP_NODE(n)->body);
            break;
        case ND_NEGATE:
        case ND_RETURN:
            fputs(", \x1b[1moperand:\x1b[0m ", to);
            astNodePrint(to, AS_UNARY_NODE(n)->operand);
            break;
        case ND_IDENTIFIER:
            fprintf(to, ", \x1b[1midentifier:\x1b[0m '%s'", AS_IDENTIFIER_NODE(n)->identifier);
            break;
        case ND_BLOCK:
            fputs(", \x1b[1mscope:\x1b[0m ", to);
            scopeIDPrint(to, AS_LIST_NODE(n)->scope, true);
            // fallthrough
        case ND_ARGS:
            fputs(", \x1b[1mnodes:\x1b[0m [", to);
            PRINT_ARRAY(ASTNode *, astNodePrint, to, AS_LIST_NODE(n)->nodes);
            fputc(']', to);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


/* ASTObj */

ASTObj *astNewObj(ASTObjType type, Location loc, ASTString name, Type *data_type) {
    ASTObj *o;
    NEW0(o);
    o->type = type;
    o->location = loc;
    o->name = name;
    o->data_type = data_type;
    switch(type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            arrayInit(&o->as.fn.parameters);
            arrayInit(&o->as.fn.locals);
            break;
        case OBJ_STRUCT:
            arrayInit(&o->as.structure.fields);
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
    // No need to free the name and type
    // as they aren't owned by the object.
    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            arrayMap(&obj->as.fn.parameters, free_object_callback, NULL);
            arrayFree(&obj->as.fn.parameters);
            arrayMap(&obj->as.fn.locals, free_object_callback, NULL);
            arrayFree(&obj->as.fn.locals);
            blockScopeFree(obj->as.fn.scopes);
            astNodeFree(AS_NODE(obj->as.fn.body));
            break;
        case OBJ_STRUCT:
            arrayMap(&obj->as.structure.fields, free_object_callback, NULL);
            arrayFree(&obj->as.structure.fields);
            break;
        default:
            UNREACHABLE();
    }
    FREE(obj);
}

static const char *obj_type_name(ASTObjType type) {
    static const char *names[] = {
        [OBJ_VAR]    = "OBJ_VAR",
        [OBJ_FN]     = "OBJ_FN",
        [OBJ_STRUCT] = "OBJ_STRUCT"
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

    fprintf(to, ", \x1b[1mname:\x1b[0m '%s'", obj->name);
    fputs(", \x1b[1mdata_type:\x1b[0m ", to);
    typePrint(to, obj->data_type, true);
    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
            typePrint(to, obj->as.fn.return_type, true);
            fputs(", \x1b[1mlocals:\x1b[0m [", to);
            PRINT_ARRAY(ASTObj *, astObjPrint, to, obj->as.fn.locals);
            fputs("], \x1b[1mbody:\x1b[0m ", to);
            astNodePrint(to, AS_NODE(obj->as.fn.body));
            break;
        case OBJ_STRUCT:
            fputs(", \x1b[1mmembers:\x1b[0m [", to);
            PRINT_ARRAY(ASTObj *, astObjPrint, to, obj->as.structure.fields);
            fputc(']', to);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

#undef PRINT_ARRAY
