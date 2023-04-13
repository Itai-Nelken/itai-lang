#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Types.h"
#include "Ast.h"

/* callbacks */

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

static void free_type_table_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    Type *ty = (Type *)item->key;
    typeFree(ty);
    FREE(ty);
}

static unsigned hash_type(void *type) {
    Type *ty = (Type *)type;
    return typeHash(ty);
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


/* Utility macros */

#define PRINT_ARRAY(type, print_fn, stream, array) ARRAY_FOR(i, (array)) { \
    print_fn((stream), ARRAY_GET_AS(type, &(array), i)); \
    if(i + 1 < arrayLength(&(array))) { \
        fputs(", ", (stream)); \
        } \
    }

/* ASTString */

void astStringPrint(FILE *to, ASTString *str) {
    fputs("ASTString{", to);
    locationPrint(to, str->location, true);
    fprintf(to, ", '%s'}", str->data);
}


/* Value */

void valuePrint(FILE *to, Value value) {
    fprintf(to, "Value{\x1b[1mvalue:\x1b[0m ");
    switch(value.type) {
        case VAL_NUMBER:
            fprintf(to, "\x1b[34m%lu\x1b[0m", value.as.number);
            break;
        case VAL_STRING:
            astStringPrint(to, &value.as.string);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


/* Scope */

Scope *scopeNew(ScopeID parent_scope, bool is_block_scope) {
    UNUSED(is_block_scope);
    Scope *sc;
    NEW0(sc);
    //sc->is_block_scope = is_block_scope;
    //sc->depth = 0;
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
    tableMap(&scope_list->types, free_type_table_callback, NULL);
    tableFree(&scope_list->types);
    scope_list->children.capacity = 0;
    scope_list->children.length = 0;
    if(scope_list->children.children_scope_ids != NULL) {
        FREE(scope_list->children.children_scope_ids);
        scope_list->children.children_scope_ids = NULL;
    }
    FREE(scope_list);
}

void scopePrint(FILE *to, Scope *scope) {
    fputs("Scope{", to);
    //fprintf(to, "Scope{\x1b[1mis_block_scope: \x1b[31m%s\x1b[0m", scope->is_block_scope ? "true" : "false");
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

void scopeIDPrint(FILE *to, ScopeID scope_id, bool compact) {
    if(compact) {
        fprintf(to, "ScopeID{\x1b[1mm:\x1b[0;34m%zu\x1b[0m,\x1b[1mi:\x1b[0;34m%zu\x1b[0m}", scope_id.module, scope_id.index);
    } else {
        fprintf(to, "ScopeID{\x1b[1mmodule:\x1b[0;34m %zu\x1b[0m, \x1b[1mindex:\x1b[0;34m %zu\x1b[0m}", scope_id.module, scope_id.index);
    }
}

ASTObj *scopeGetStruct(Scope *sc, ASTString name) {
    TableItem *item = tableGet(&sc->structures, (void *)name.data);
    return item == NULL ? NULL : (ASTObj *)item->value;
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

void controlFlowPrint(FILE *to, ControlFlow cf) {
    fprintf(to, "ControlFlow{\x1b[1;34m%s\x1b[0m}", ((const char *[__CF_STATE_COUNT]){
        [CF_NONE]           = "NONE",
        [CF_NEVER_RETURNS]  = "NEVER_RETURNS",
        [CF_MAY_RETURN]     = "MAY_RETURN",
        [CF_ALWAYS_RETURNS] = "ALWAYS_RETURNS",
    }[cf]));
}


/* AST node - ASTExprNode */

static const char *expr_node_name(ASTExprNodeType type) {
    switch(type) {
        // Constant nodes
        case EXPR_NUMBER_CONSTANT:
        case EXPR_STRING_CONSTANT:
            return "ASTConstantValueExpr";
        // Object nodes
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            return "ASTObjExpr";
        // Binary nodes
        case EXPR_ASSIGN:
        case EXPR_PROPERTY_ACCESS:
        case EXPR_ADD:
        case EXPR_SUBTRACT:
        case EXPR_MULTIPLY:
        case EXPR_DIVIDE:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
            return "ASTBinaryExpr";
        // Unary nodes
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF:
            return "ASTUnaryNode";
        case EXPR_CALL:
            return "ASTCallExpr";
        // identifier nodes
        case EXPR_IDENTIFIER:
            return "ASTIdentifierExpr";
        default:
            UNREACHABLE();
    }
}

static const char *expr_type_name(ASTExprNodeType type) {
    static const char *names[] = {
        [EXPR_NUMBER_CONSTANT]  = "EXPR_NUMBER_CONSTANT",
        [EXPR_STRING_CONSTANT]  = "EXPR_STRING_CONSTANT",
        [EXPR_VARIABLE]         = "EXPR_VARIABLE",
        [EXPR_FUNCTION]         = "EXPR_FUNCTION",
        [EXPR_ASSIGN]           = "EXPR_ASSIGN",
        [EXPR_PROPERTY_ACCESS]  = "EXPR_PROPERTY_ACCESS",
        [EXPR_ADD]              = "EXPR_ADD",
        [EXPR_SUBTRACT]         = "EXPR_SUBTRACT",
        [EXPR_MULTIPLY]         = "EXPR_MULTIPLY",
        [EXPR_DIVIDE]           = "EXPR_DIVIDE",
        [EXPR_EQ]               = "EXPR_EQ",
        [EXPR_NE]               = "EXPR_NE",
        [EXPR_LT]               = "EXPR_LT",
        [EXPR_LE]               = "EXPR_LE",
        [EXPR_GT]               = "EXPR_GT",
        [EXPR_GE]               = "EXPR_GE",
        [EXPR_NEGATE]           = "EXPR_NEGATE",
        [EXPR_ADDROF]           = "EXPR_ADDROF",
        [EXPR_DEREF]            = "EXPR_DEREF",
        [EXPR_CALL]             = "EXPR_CALL",
        [EXPR_IDENTIFIER]       = "EXPR_IDENTIFIER"
    };
    return names[type];
}

void astExprNodePrint(FILE *to, ASTExprNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m", expr_node_name(n->type), expr_type_name(n->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location, true);
    fputs(", \x1b[1mdata_type:\x1b[0m ", to);
    typePrint(to, n->data_type, true);
    switch(n->type) {
        // Constant nodes
        case EXPR_NUMBER_CONSTANT:
        case EXPR_STRING_CONSTANT:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            valuePrint(to, AS_CONSTANT_VALUE_EXPR(n)->value);
            break;
        // Obj nodes
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            fputs(", \x1b[1mobj:\x1b[0m ", to);
            astObjPrintCompact(to, AS_OBJ_EXPR(n)->obj);
            break;
        // binary nodes
        case EXPR_ASSIGN:
        case EXPR_PROPERTY_ACCESS:
        case EXPR_ADD:
        case EXPR_SUBTRACT:
        case EXPR_MULTIPLY:
        case EXPR_DIVIDE:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
            fputs(", \x1b[1mlhs:\x1b[0m ", to);
            astExprNodePrint(to, AS_BINARY_EXPR(n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astExprNodePrint(to, AS_BINARY_EXPR(n)->rhs);
            break;
        // Unary nodes
        case EXPR_NEGATE:
        case EXPR_ADDROF:
        case EXPR_DEREF:
            fputs(", \x1b[1moperand:\x1b[0m ", to);
            astExprNodePrint(to, AS_UNARY_EXPR(n)->operand);
            break;
        // Call node
        case EXPR_CALL:
            fputs(", \x1b[1mcallee:\x1b[0m ", to);
            astExprNodePrint(to, AS_CALL_EXPR(n)->callee);
            fputs(", \x1b[1marguments:\x1b[0m [", to);
            PRINT_ARRAY(ASTExprNode *, astExprNodePrint, to, AS_CALL_EXPR(n)->arguments);
            fputc(']', to);
            break;
        // identifier nodes
        case EXPR_IDENTIFIER:
            fputs(", \x1b[1midentifier:\x1b[0m ", to);
            astStringPrint(to, &AS_IDENTIFIER_EXPR(n)->id);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

static inline ASTExprNode make_expr_node_header(ASTExprNodeType type, Location loc, Type *ty) {
    return (ASTExprNode){
        .type = type,
        .location = loc,
        .data_type = ty
    };
}

ASTExprNode *astNewConstantValueExpr(Allocator *a, ASTExprNodeType type, Location loc, Value value, Type *value_ty) {
    ASTConstantValueExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, value_ty);
    n->value = value;
    return (ASTExprNode *)n;
}

ASTExprNode *astNewObjExpr(Allocator *a, ASTExprNodeType type, Location loc, ASTObj *obj) {
    ASTObjExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, obj->data_type);
    n->obj = obj;
    return (ASTExprNode *)n;
}

ASTExprNode *astNewUnaryExpr(Allocator *a, ASTExprNodeType type, Location loc, ASTExprNode *operand) {
    ASTUnaryExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, NULL);
    n->operand = operand;
    return (ASTExprNode *)n;
}

ASTExprNode *astNewBinaryExpr(Allocator *a, ASTExprNodeType type, Location loc, ASTExprNode *lhs, ASTExprNode *rhs) {
    ASTBinaryExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, NULL);
    n->lhs = lhs;
    n->rhs = rhs;
    return (ASTExprNode *)n;
}

ASTExprNode *astNewCallExpr(Allocator *a, Location loc, ASTExprNode *callee, Array *arguments) {
    ASTCallExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(EXPR_CALL, loc, NULL);
    n->callee = callee;
    arrayInitAllocatorSized(&n->arguments, *a, arrayLength(arguments));
    arrayCopy(&n->arguments, arguments);
    return (ASTExprNode *)n;
}

ASTExprNode *astNewIdentifierExpr(Allocator *a, Location loc, ASTString id) {
    ASTIdentifierExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(EXPR_IDENTIFIER, loc, NULL);
    n->id = id;
    return (ASTExprNode *)n;
}


/* AST node - ASTStmtNode */

static const char *stmt_node_name(ASTStmtNodeType type) {
    switch(type) {
        // VarDecl node
        case STMT_VAR_DECL:
            return "ASTVarDeclStmt";
        // Block node
        case STMT_BLOCK:
            return "ASTBlockStmt";
        // Conditional node
        case STMT_IF:
            return "ASTConditionalStmt";
        // Loop node
        case STMT_WHILE_LOOP:
            return "ASTLoopStmt";
        // Expr nodes
        case STMT_RETURN:
        case STMT_DEFER:
        case STMT_EXPR:
            return "ASTExprStmt";
        default:
            UNREACHABLE();
    }
}

static const char *stmt_type_name(ASTStmtNodeType type) {
    static const char *names[] = {
        [STMT_VAR_DECL]      = "STMT_VAR_DECL",
        [STMT_BLOCK]         = "STMT_BLOCK",
        [STMT_IF]            = "STMT_IF",
        [STMT_WHILE_LOOP]    = "STMT_WHILE_LOOP",
        [STMT_RETURN]        = "STMT_RETURN",
        [STMT_DEFER]         = "STMT_DEFER",
        [STMT_EXPR]          = "STMT_EXPR",
    };
    return names[type];
}

void astStmtNodePrint(FILE *to, ASTStmtNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m", stmt_node_name(n->type), stmt_type_name(n->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location, true);
    switch(n->type) {
        // VarDeck node
        case STMT_VAR_DECL:
            fputs(", \x1b[1mvariable:\x1b[0m ", to);
            astObjPrintCompact(to, AS_VAR_DECL_STMT(n)->variable);
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astExprNodePrint(to, AS_VAR_DECL_STMT(n)->initializer);
            break;
        // Block nodes
        case STMT_BLOCK:
            fputs(", \x1b[1mscope:\x1b[0m ", to);
            scopeIDPrint(to, AS_BLOCK_STMT(n)->scope, true);
            fputs(", \x1b[1mcontrol_flow:\x1b[0m ", to);
            controlFlowPrint(to, AS_BLOCK_STMT(n)->control_flow);
            fputs(", \x1b[1mnodes:\x1b[0m [", to);
            PRINT_ARRAY(ASTStmtNode *, astStmtNodePrint, to, AS_BLOCK_STMT(n)->nodes);
            fputc(']', to);
            break;
        // Conditional nodes
        case STMT_IF:
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astExprNodePrint(to, AS_CONDITIONAL_STMT(n)->condition);
            fputs(", \x1b[1mthen:\x1b[0m ", to);
            astStmtNodePrint(to, (ASTStmtNode *)(AS_CONDITIONAL_STMT(n)->then));
            fputs(", \x1b[1melse:\x1b[0m ", to);
            astStmtNodePrint(to, (ASTStmtNode *)(AS_CONDITIONAL_STMT(n)->else_));
            break;
        // Loop node
        case STMT_WHILE_LOOP:
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astStmtNodePrint(to, AS_LOOP_STMT(n)->initializer);
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astExprNodePrint(to, AS_LOOP_STMT(n)->condition);
            fputs(", \x1b[1mincrement:\x1b[0m ", to);
            astExprNodePrint(to, AS_LOOP_STMT(n)->increment);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astStmtNodePrint(to, (ASTStmtNode *)(AS_LOOP_STMT(n)->body));
            break;
        // Expr nodes
        case STMT_RETURN:
        case STMT_DEFER:
        case STMT_EXPR:
            fputs(", \x1b[1mexpr:\x1b[0m", to);
            astExprNodePrint(to, AS_EXPR_STMT(n)->expr);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

static inline ASTStmtNode make_stmt_mode_header(ASTStmtNodeType type, Location loc) {
    return (ASTStmtNode){
        .type = type,
        .location = loc
    };
}

ASTStmtNode *astNewVarDeclStmt(Allocator *a, Location loc, ASTObj *variable, ASTExprNode *initializer) {
    ASTVarDeclStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(STMT_VAR_DECL, loc);
    n->variable = variable;
    n->initializer = initializer;
    return (ASTStmtNode *)n;
}

ASTStmtNode *astNewBlockStmt(Allocator *a, Location loc, ScopeID scope, ControlFlow control_flow, Array *nodes) {
    ASTBlockStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(STMT_BLOCK, loc);
    n->scope = scope;
    n->control_flow = control_flow;
    arrayInitAllocatorSized(&n->nodes, *a, arrayLength(nodes));
    arrayCopy(&n->nodes, nodes);
    return (ASTStmtNode *)n;
}

ASTStmtNode *astNewConditionalStmt(Allocator *a, Location loc, ASTExprNode *condition, ASTBlockStmt *then, ASTStmtNode *else_) {
    ASTConditionalStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(STMT_IF, loc);
    n->condition = condition;
    n->then = then;
    n->else_ = else_;
    return (ASTStmtNode *)n;
}

ASTStmtNode *astNewLoopStmt(Allocator *a, ASTStmtNodeType type, Location loc, ASTStmtNode *initializer, ASTExprNode *condition, ASTExprNode *increment, ASTBlockStmt *body) {
    ASTLoopStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(type, loc);
    n->initializer = initializer;
    n->condition = condition;
    n->increment = increment;
    n->body = body;
    return (ASTStmtNode *)n;
}

ASTStmtNode *astNewExprStmt(Allocator *a, ASTStmtNodeType type, Location loc, ASTExprNode *expr) {
    ASTExprStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(type, loc);
    n->expr = expr;
    return (ASTStmtNode *)n;
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
            fputs(", ", to);
            astStringPrint(to, &a->as.source);
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
            o->as.structure.scope = EMPTY_SCOPE_ID;
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
            obj->as.structure.scope = EMPTY_SCOPE_ID;
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
    return names[type];
}

void astObjPrintCompact(FILE *to, ASTObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }
    fprintf(to, "ASTObj{\x1b[1;36m%s\x1b[0m, ", obj_type_name(obj->type));
    locationPrint(to, obj->location, true);
    fputs(", ", to);
    astStringPrint(to, &obj->name);
    fputs(", ", to);
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

    fputs(", \x1b[1mname:\x1b[0m ", to);
    astStringPrint(to, &obj->name);
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
            PRINT_ARRAY(ASTExprNode *, astExprNodePrint, to, obj->as.fn.defers);
            fputs("], \x1b[1mbody:\x1b[0m ", to);
            astStmtNodePrint(to, (ASTStmtNode *)obj->as.fn.body);
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


/* ASTModule */

ASTModule *astModuleNew(ASTString name) {
    ASTModule *m;
    NEW0(m);
    m->name = name;
    arenaInit(&m->ast_allocator.storage);
    m->ast_allocator.alloc = arenaMakeAllocator(&m->ast_allocator.storage);
    arrayInit(&m->scopes);
    m->module_scope = scopeNew(EMPTY_SCOPE_ID, false);
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
    fputs("ASTModule{\x1b[1mname:\x1b[0m ", to);
    astStringPrint(to, &module->name);
    fputs(", \x1b[1mscopes:\x1b[0m [", to);
    PRINT_ARRAY(Scope *, scopePrint, to, module->scopes);
    fputs("], \x1b[1mglobals:\x1b[0m [", to);
    PRINT_ARRAY(ASTStmtNode *, astStmtNodePrint, to, module->globals);
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

ASTInternedString astProgramAddString(ASTProgram *prog, char *str) {
    String string = stringCopy(str);

    TableItem *existing_str = tableGet(&prog->strings, (void *)string);
    if(existing_str != NULL) {
        stringFree(string);
        return (ASTInternedString)existing_str->value;
    }
    tableSet(&prog->strings, (void *)string, (void *)string);
    return (ASTInternedString)string;
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
