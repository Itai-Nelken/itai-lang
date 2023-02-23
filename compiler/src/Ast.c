#include <stdio.h>
#include <string.h> // strlen
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

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astObjFree((ASTObj *)object);
}

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astModuleFree((ASTModule *)module);
}

static void free_scope_callback(void *scope, void *cl) {
    UNUSED(cl);
    scopeFree((Scope *)scope);
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
    // hash the name using the fnv-la string hashing algorithm.
    unsigned length = (unsigned)strlen(ty->name);
    unsigned hash = 2166136261u;
    for(unsigned i = 0; i < length; ++i) {
        hash ^= (char)ty->name[i];
        hash *= 16777619;
    }
    if(ty->type == TY_FN) {
        for(usize i = 0; i < ty->as.fn.parameter_types.used; ++i) {
            hash |= hash_type(arrayGet(&ty->as.fn.parameter_types, i));
        }
        hash &= hash_type((void *)ty->as.fn.return_type);
    } else if(ty->type == TY_PTR) {
        hash &= hash_type((void *)ty->as.ptr.inner_type);
    }
    return (ty->type + (uintptr_t)hash) >> 2; // implicitly cast to 'unsigned', so extra bits are discarded.
}

static bool compare_type(void *a, void *b) {
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

static void print_object_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    ASTObj *obj = (ASTObj *)item->value;
    astObjPrint(to, obj);
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
    arenaInit(&m->ast_allocator.storage);
    m->ast_allocator.alloc = arenaMakeAllocator(&m->ast_allocator.storage);
    arrayInit(&m->scopes);
    m->module_scope = scopeNew(EMPTY_SCOPE_ID(), false);
    // Note: if module_scope is not the first scope, change astModuleGetModuleScopeID().
    arrayPush(&m->scopes, m->module_scope);
    arrayInit(&m->globals);
    return m;
}

void astModuleFree(ASTModule *module) {
    module->module_scope = NULL;
    arrayMap(&module->scopes, free_scope_callback, NULL);
    arrayFree(&module->scopes);
    arrayFree(&module->globals);
    arenaFree(&module->ast_allocator.storage);
    FREE(module);
}

void astModulePrint(FILE *to, ASTModule *module) {
    fprintf(to, "ASTModule{\x1b[1mname:\x1b[0m '%s', \x1b[1mscopes:\x1b[0m [", module->name);
    PRINT_ARRAY(Scope *, scopePrint, to, module->scopes);
    fputs("], \x1b[1mglobals:\x1b[0m [", to);
    PRINT_ARRAY(ASTNode *, astNodePrint, to, module->globals);
    fputs("]}", to);
}

ScopeID astModuleAddScope(ASTModule *module, Scope *scope) {
    ScopeID id;
    id.module = module->id;
    id.index = (ModuleID)arrayPush(&module->scopes, (void *)scope);
    return id;
}

Scope *astModuleGetScope(ASTModule *module, ScopeID id) {
    VERIFY(module->id == id.module);
    VERIFY(id.index < arrayLength(&module->scopes));
    Scope *scope = ARRAY_GET_AS(Scope *, &module->scopes, id.index);
    VERIFY(scope != NULL);
    return scope;
}

ScopeID astModuleGetModuleScopeID(ASTModule *module) {
    return (ScopeID){
        .module = module->id,
        .index = 0
    };
}


/* ASTProgram */

void astProgramInit(ASTProgram *prog) {
    tableInit(&prog->strings, NULL, NULL);
    arrayInit(&prog->modules);
#define NULL_TY(name) prog->primitives.name = NULL;
    NULL_TY(void_);
    NULL_TY(int32);
    NULL_TY(uint32);
    NULL_TY(str);
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
    // arrayGet() returns NULL on invalid indexes.
    ASTModule *m = ARRAY_GET_AS(ASTModule *, &prog->modules, id);
    VERIFY(m != NULL);
    return m;
}


/* LiteralValue */

void literalValuePrint(FILE *to, LiteralValue value) {
    fprintf(to, "LiteralValue{\x1b[1mvalue:\x1b[0m ");
    switch(value.type) {
        case LIT_NUMBER:
            fprintf(to, "\x1b[34m%lu\x1b[0m", value.as.number);
            break;
        case LIT_STRING:
            fprintf(to, "'%s'", value.as.str);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


/* Scope */

Scope *scopeNew(ScopeID parent_scope, bool is_block_scope) {
    Scope *sc;
    NEW0(sc);
    sc->is_block_scope = is_block_scope;
    sc->depth = 0;
    arrayInit(&sc->objects);
    tableInit(&sc->variables, NULL, NULL);
    tableInit(&sc->functions, NULL, NULL);
    tableInit(&sc->structures, NULL, NULL);
    tableInit(&sc->types, hash_type, compare_type);
    // sc.children is lazy-allocated. see scopeAddChild()
    sc->parent = parent_scope;
    return sc;
}

void scopeAddChild(Scope *parent, ScopeID child_id) {
    if(parent->children.length + 1 > parent->children.capacity) {
        // Note: The initial capacity of 4 is pretty random. 4 just seemed like a good number
        //       that will be enough most of the time.
        parent->children.capacity = parent->children.capacity == 0 ? 4 : parent->children.capacity * 2;
        parent->children.children_scope_ids = REALLOC(parent->children.children_scope_ids, sizeof(*parent->children.children_scope_ids) * parent->children.capacity);
    }
    parent->children.children_scope_ids[parent->children.length++] = child_id;
}

Type *scopeAddType(Scope *scope, Type *ty) {
    TableItem *existing_item = NULL;
    if((existing_item = tableGet(&scope->types, (void *)ty)) != NULL) {
        typeFree(ty);
        FREE(ty);
        return (Type *)existing_item->key;
    }
    tableSet(&scope->types, (void *)ty, NULL);
    return ty;
}

void scopeFree(Scope *scope_list) {
    if(scope_list == NULL) {
        return;
    }
    // No need to free the strings in the table as they are
    // ASTString's which are owned by the ASTProgram
    // being used for the parse.
    arrayMap(&scope_list->objects, free_object_callback, NULL);
    arrayFree(&scope_list->objects);
    tableFree(&scope_list->variables);
    tableFree(&scope_list->functions);
    tableFree(&scope_list->structures);
    tableMap(&scope_list->types, free_type_callback, NULL);
    tableFree(&scope_list->types);
    scope_list->children.capacity = 0;
    scope_list->children.length = 0;
    if(scope_list->children.children_scope_ids != NULL) {
        FREE(scope_list->children.children_scope_ids);
        scope_list->children.children_scope_ids = NULL;
    }
    FREE(scope_list);
}

void scopeIDPrint(FILE *to, ScopeID scope_id, bool compact) {
    if(compact) {
        fprintf(to, "ScopeID{\x1b[1mm:\x1b[0;34m%zu\x1b[0m,\x1b[1mi:\x1b[0;34m%zu\x1b[0m}", scope_id.module, scope_id.index);
    } else {
        fprintf(to, "ScopeID{\x1b[1mmodule:\x1b[0;34m %zu\x1b[0m, \x1b[1mindex:\x1b[0;34m %zu\x1b[0m}", scope_id.module, scope_id.index);
    }
}

void scopePrint(FILE *to, Scope *scope) {
    fprintf(to, "Scope{\x1b[1mis_block_scope: \x1b[31m%s\x1b[0m", scope->is_block_scope ? "true" : "false");
    // scope.objects isn't printed because all the objects in it will
    // be printed in the scope.variables, scope.functions, & scope.structures tables.
    fputs(", \x1b[1mvariables:\x1b[0m [", to);
    tableMap(&scope->variables, print_object_table_callback, (void *)to);
    fputs("], \x1b[1mfunctions:\x1b[0m [", to);
    tableMap(&scope->functions, print_object_table_callback, (void *)to);
    fputs("], \x1b[1mstructures:\x1b[0m [", to);
    tableMap(&scope->structures, print_object_table_callback, (void *)to);
    fputs("], \x1b[1mtypes:\x1b[0m [", to);
    tableMap(&scope->types, print_type_table_callback, (void *)to);
    fputs("], \x1b[1mchildren:\x1b[0m [", to);
    for(usize i = 0; i < scope->children.length; ++i) {
        scopeIDPrint(to, scope->children.children_scope_ids[i], true);
        if(i + i < scope->children.length) { // If not the last element, print a comma followed by a space.
            fputs(", ", to);
        }
    }
    fputs("]}", to);
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

ASTNode *astNewUnaryNode(Allocator *a, ASTNodeType type, Location loc, ASTNode *operand) {
    ASTUnaryNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->operand = operand;
    return AS_NODE(n);
}

ASTNode *astNewBinaryNode(Allocator *a, ASTNodeType type, Location loc, ASTNode *lhs, ASTNode *rhs) {
    ASTBinaryNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->lhs = lhs;
    n->rhs = rhs;
    return AS_NODE(n);
}

ASTNode *astNewConditionalNode(Allocator *a, ASTNodeType type, Location loc, ASTNode *condition, ASTNode *body, ASTNode *else_) {
    ASTConditionalNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->condition = condition;
    n->body = body;
    n->else_ = else_;
    return AS_NODE(n);
}

ASTNode *astNewLoopNode(Allocator *a, Location loc, ASTNode *init, ASTNode *cond, ASTNode *inc, ASTNode *body) {
    ASTLoopNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(ND_WHILE_LOOP, loc);
    n->initializer = init;
    n->condition = cond;
    n->increment = inc;
    n->body = body;
    return AS_NODE(n);
}

ASTNode *astNewLiteralValueNode(Allocator *a, ASTNodeType type, Location loc, LiteralValue value, Type *ty) {
    ASTLiteralValueNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->value = value;
    n->ty = ty;
    return AS_NODE(n);
}

ASTNode *astNewObjNode(Allocator *a, ASTNodeType type, Location loc, ASTObj *obj) {
    ASTObjNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->obj = obj;
    return AS_NODE(n);
}

ASTNode *astNewIdentifierNode(Allocator *a, Location loc, ASTString str) {
    ASTIdentifierNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(ND_IDENTIFIER, loc);
    n->identifier = str;
    return AS_NODE(n);
}

ASTNode *astNewListNode(Allocator *a, ASTNodeType type, Location loc, ScopeID scope, size_t node_count) {
    ASTListNode *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_header(type, loc);
    n->scope = scope;
    n->control_flow = CF_NONE;
    arrayInitAllocatorSized(&n->nodes, *a, node_count);

    return AS_NODE(n);
}

static const char *node_name(ASTNodeType type) {
    switch(type) {
        // literal nodes
        case ND_NUMBER_LITERAL:
        case ND_STRING_LITERAL:
            return "ASTLiteralValueNode";
        // object nodes
        case ND_VAR_DECL:
        case ND_VARIABLE:
        case ND_FUNCTION:
            return "ASTObjNode";
        // binary nodes
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
        // conditional nodes
        case ND_IF:
            return "ASTConditionalNode";
        // loop nodes
        case ND_WHILE_LOOP:
            return "ASTLoopNode";
        // unary nodes
        case ND_NEGATE:
        case ND_RETURN:
        case ND_DEFER:
        case ND_ADDROF:
        case ND_DEREF:
            return "ASTUnaryNode";
        // identifier nodes
        case ND_IDENTIFIER:
            return "ASTIdentifierNode";
        // list nodes
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
        [ND_STRING_LITERAL]  = "ND_STRING_LITERAL",
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
        [ND_DEFER]           = "ND_DEFER",
        [ND_ADDROF]          = "ND_ADDROF",
        [ND_DEREF]           = "ND_DEREF",
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
        // literal nodes
        case ND_NUMBER_LITERAL:
        case ND_STRING_LITERAL:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            literalValuePrint(to, AS_LITERAL_NODE(n)->value);
            break;
        // object nodes
        case ND_VAR_DECL:
        case ND_VARIABLE:
        case ND_FUNCTION:
            fputs(", \x1b[1mobj:\x1b[0m ", to);
            astObjPrintCompact(to, AS_OBJ_NODE(n)->obj);
            break;
        // binary nodes
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
        // conditional nods
        case ND_IF:
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astNodePrint(to, AS_CONDITIONAL_NODE(n)->condition);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astNodePrint(to, AS_CONDITIONAL_NODE(n)->body);
            fputs(", \x1b[1melse:\x1b[0m ", to);
            astNodePrint(to, AS_CONDITIONAL_NODE(n)->else_);
            break;
        // loop nodes
        case ND_WHILE_LOOP:
            // NOTE: ND_FOR_LOOP and ND_FOR_ITERATOR_LOOP
            //       should be printed separately because they use
            //       the rest of the clauses (initializer, increment).
            fputs(", \x1b[1mcondition:\x1b[0m", to);
            astNodePrint(to, AS_LOOP_NODE(n)->condition);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astNodePrint(to, AS_LOOP_NODE(n)->body);
            break;
        // unary nodes
        case ND_NEGATE:
        case ND_RETURN:
        case ND_DEFER:
        case ND_ADDROF:
        case ND_DEREF:
            fputs(", \x1b[1moperand:\x1b[0m ", to);
            astNodePrint(to, AS_UNARY_NODE(n)->operand);
            break;
        // identifier nodse
        case ND_IDENTIFIER:
            fprintf(to, ", \x1b[1midentifier:\x1b[0m '%s'", AS_IDENTIFIER_NODE(n)->identifier);
            break;
        // list nodes
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


/* Attribute */

Attribute *attributeNew(AttributeType type, Location loc) {
    Attribute *attr;
    NEW0(attr);
    attr->type = type;
    attr->location = loc;
    return attr;
}

void attributeFree(Attribute *a) {
    FREE(a);
}

static const char *attribute_type_name(AttributeType type) {
    static const char *types[] = {
        [ATTR_SOURCE] = "ATTR_SOURCE",
    };
    return types[(u32)type];
}

void attributePrint(FILE *to, Attribute *a) {
    fprintf(to, "Attribute{\x1b[1mtype: \x1b[0;35m%s\x1b[0m, \x1b[1mlocation:\x1b[0m ", attribute_type_name(a->type));
    locationPrint(to, a->location, true);
    switch(a->type) {
        case ATTR_SOURCE:
            fprintf(to, ", '%s'", a->as.source);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

const char *attributeTypeString(AttributeType type) {
    static const char *types[] = {
        [ATTR_SOURCE] = "source",
    };
    return types[(u32)type];
}


/* ASTObj */

ASTObj *astNewObj(ASTObjType type, Location loc, Location name_loc, ASTString name, Type *data_type) {
    ASTObj *o;
    NEW0(o);
    o->type = type;
    o->location = loc;
    o->name_location = name_loc;
    o->name = name;
    o->data_type = data_type;
    switch(type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            arrayInit(&o->as.fn.parameters);
            arrayInit(&o->as.fn.defers);
            break;
        case OBJ_STRUCT:
            // FIXME: Should the scope somehow be initialized here?
            o->as.structure.scope = EMPTY_SCOPE_ID();
            break;
        case OBJ_EXTERN_FN:
            arrayInit(&o->as.fn.parameters);
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
            arrayFree(&obj->as.fn.defers); // The nodes are owned by the parent module.
            // Note: The body is owned by the parent module,
            // and variables are owned by the scopes which
            // which are owned by the parent module as well.
            break;
        case OBJ_STRUCT:
            obj->as.structure.scope = EMPTY_SCOPE_ID();
            break;
        case OBJ_EXTERN_FN:
            arrayMap(&obj->as.extern_fn.parameters, free_object_callback, NULL);
            arrayFree(&obj->as.extern_fn.parameters);
            attributeFree(obj->as.extern_fn.source_attr);
            break;
        default:
            UNREACHABLE();
    }
    FREE(obj);
}

static const char *obj_type_name(ASTObjType type) {
    static const char *names[] = {
        [OBJ_VAR]       = "OBJ_VAR",
        [OBJ_FN]        = "OBJ_FN",
        [OBJ_STRUCT]    = "OBJ_STRUCT",
        [OBJ_EXTERN_FN] = "OBJ_EXTERN_FN"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == OBJ_TYPE_COUNT, "Missing type(s) in obj_type_name()");
    return names[type];
}

void astObjPrintCompact(FILE *to, ASTObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }
    fprintf(to, "ASTObj{\x1b[1;36m%s\x1b[0m, ", obj_type_name(obj->type));
    locationPrint(to, obj->location, true);
    fprintf(to, ", '%s', ", obj->name);
    typePrint(to, obj->data_type, true);
    fputc('}', to);
}

void astObjPrint(FILE *to, ASTObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }

    fprintf(to, "ASTObj{\x1b[1mtype: \x1b[36m%s\x1b[0m", obj_type_name(obj->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, obj->location, true);
    // Note: name loc is not printed to declutter the output.

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
            fputs(", \x1b[1mparameters:\x1b[0m [", to);
            PRINT_ARRAY(ASTObj *, astObjPrint, to, obj->as.fn.parameters);
            fputs("], \x1b[1mdefers:\x1b[0m [", to);
            PRINT_ARRAY(ASTObj *, astObjPrint, to, obj->as.fn.defers);
            fputs("], \x1b[1mbody:\x1b[0m ", to);
            astNodePrint(to, AS_NODE(obj->as.fn.body));
            break;
        case OBJ_STRUCT:
            fputs(", \x1b[1mscope:\x1b[0m ", to);
            // FIXME: Ideally the actual scope should be printed.
            scopeIDPrint(to, obj->as.structure.scope, false);
            break;
        case OBJ_EXTERN_FN:
            fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
            typePrint(to, obj->as.fn.return_type, true);
            fputs(", \x1b[1mparameters:\x1b[0m [", to);
            PRINT_ARRAY(ASTObj *, astObjPrint, to, obj->as.fn.parameters);
            fputs("], \x1b[1msource:\x1b[0m ", to);
            attributePrint(to, obj->as.extern_fn.source_attr);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

#undef PRINT_ARRAY
