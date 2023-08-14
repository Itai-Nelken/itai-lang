#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Table.h"
#include "Types/CheckedType.h"
#include "Ast/ast_common.h"
#include "Ast/CheckedAst.h"


/* callbacks - FIXME: unduplicate from ParsedAST.c where possible */

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astFreeCheckedObj((ASTCheckedObj *)object);
}

static void free_type_table_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    CheckedType *ty = (CheckedType *)item->key;
    checkedTypeFree(ty);
    FREE(ty);
}

static void free_scope_callback(void *scope, void *cl) {
    UNUSED(cl);
    checkedScopeFree((CheckedScope *)scope);
}

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astFreeCheckedModule((ASTCheckedModule *)module);
}

static void print_type_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    CheckedType *ty = (CheckedType *)item->key;
    checkedTypePrint(to, ty, false);
    if(!is_last) {
        fputs(", ", to);
    }
}

static void print_object_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    ASTCheckedObj *obj = (ASTCheckedObj *)item->value;
    astPrintCheckedObj(to, obj);
    if(!is_last) {
        fputs(", ", to);
    }
}

static unsigned hash_type(void *type) {
    CheckedType *ty = (CheckedType *)type;
    return checkedTypeHash(ty);
}

static bool compare_type(void *a_type, void *b_type) {
    CheckedType *a = (CheckedType *)a_type;
    CheckedType *b = (CheckedType *)b_type;
    return checkedTypeEqual(a, b);
}


/* CheckedScope */

CheckedScope *checkedScopeNew(ScopeID parent_scope, bool is_block_scope) {
    CheckedScope *sc;
    NEW0(sc);
    sc->is_block_scope = is_block_scope;
    //sc->depth = 0;
    arrayInit(&sc->objects);
    tableInit(&sc->variables, NULL, NULL);
    tableInit(&sc->functions, NULL, NULL);
    tableInit(&sc->structures, NULL, NULL);
    tableInit(&sc->types, hash_type, compare_type);
    // sc.children is lazy-allocated. see scopeAddChild().
    sc->parent = parent_scope;
    return sc;
}

void checkedScopeAddChild(CheckedScope *parent, ScopeID child_id) {
    if(parent->children.length + 1 > parent->children.capacity) {
        // Note: The initial capacity of 4 is pretty random. 4 just seemed like a good number
        //       that will be enough most of the time.
        parent->children.capacity = parent->children.capacity == 0 ? 4 : parent->children.capacity * 2;
        parent->children.children_scope_ids = REALLOC(parent->children.children_scope_ids, sizeof(*parent->children.children_scope_ids) * parent->children.capacity);
    }
    parent->children.children_scope_ids[parent->children.length++] = child_id;
}

CheckedType *checkedScopeAddType(CheckedScope *scope, CheckedType *type) {
    TableItem *existing_item = NULL;
    if((existing_item = tableGet(&scope->types, (void *)type)) != NULL) {
        FREE(type);
        return (CheckedType *)existing_item->key;
    }
    tableSet(&scope->types, (void *)type, NULL);
    return type;
}

void checkedScopeFree(CheckedScope *scope_list) {
    if(scope_list == NULL) {
        return;
    }
    // No need to free the strings in the table as they are
    // ASTString's which are owned by the ASTCheckedProgram
    // being used for the compilation.
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

void checkedScopePrint(FILE *to, CheckedScope *scope) {
    fprintf(to, "CheckedScope{\x1b[1mis_block_scope: \x1b[31m%s\x1b[0m", scope->is_block_scope ? "true" : "false");
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

ASTCheckedObj *checkedScopeGetStruct(CheckedScope *sc, ASTString name) {
    TableItem *item = tableGet(&sc->structures, (void *)name.data);
    return item == NULL ? NULL : (ASTCheckedObj *)item->value;
}


/* AST node - ASTCheckedExprNode */

static const char *expr_node_name(ASTCheckedExprNodeType type) {
    switch(type) {
        // Constant nodes
        case CHECKED_EXPR_NUMBER_CONSTANT:
        case CHECKED_EXPR_STRING_CONSTANT:
            return "ASTCheckedConstantValueExpr";
        // Object nodes
        case CHECKED_EXPR_VARIABLE:
        case CHECKED_EXPR_FUNCTION:
            return "ASTCheckedObjExpr";
        // Binary nodes
        case CHECKED_EXPR_ASSIGN:
        case CHECKED_EXPR_PROPERTY_ACCESS:
        case CHECKED_EXPR_ADD:
        case CHECKED_EXPR_SUBTRACT:
        case CHECKED_EXPR_MULTIPLY:
        case CHECKED_EXPR_DIVIDE:
        case CHECKED_EXPR_EQ:
        case CHECKED_EXPR_NE:
        case CHECKED_EXPR_LT:
        case CHECKED_EXPR_LE:
        case CHECKED_EXPR_GT:
        case CHECKED_EXPR_GE:
            return "ASTCheckedBinaryExpr";
        // Unary nodes
        case CHECKED_EXPR_NEGATE:
        case CHECKED_EXPR_ADDROF:
        case CHECKED_EXPR_DEREF:
            return "ASCheckedNode";
        case CHECKED_EXPR_CALL:
            return "ASTCheckedCallExpr";
        // identifier nodes
        case CHECKED_EXPR_IDENTIFIER:
            return "ASTCheckedIdentifierExpr";
        default:
            UNREACHABLE();
    }
}

static const char *expr_type_name(ASTCheckedExprNodeType type) {
    static const char *names[] = {
        [CHECKED_EXPR_NUMBER_CONSTANT]  = "CHECKED_EXPR_NUMBER_CONSTANT",
        [CHECKED_EXPR_STRING_CONSTANT]  = "CHECKED_EXPR_STRING_CONSTANT",
        [CHECKED_EXPR_VARIABLE]         = "CHECKED_EXPR_VARIABLE",
        [CHECKED_EXPR_FUNCTION]         = "CHECKED_EXPR_FUNCTION",
        [CHECKED_EXPR_ASSIGN]           = "CHECKED_EXPR_ASSIGN",
        [CHECKED_EXPR_PROPERTY_ACCESS]  = "CHECKED_EXPR_PROPERTY_ACCESS",
        [CHECKED_EXPR_ADD]              = "CHECKED_EXPR_ADD",
        [CHECKED_EXPR_SUBTRACT]         = "CHECKED_EXPR_SUBTRACT",
        [CHECKED_EXPR_MULTIPLY]         = "CHECKED_EXPR_MULTIPLY",
        [CHECKED_EXPR_DIVIDE]           = "CHECKED_EXPR_DIVIDE",
        [CHECKED_EXPR_EQ]               = "CHECKED_EXPR_EQ",
        [CHECKED_EXPR_NE]               = "CHECKED_EXPR_NE",
        [CHECKED_EXPR_LT]               = "CHECKED_EXPR_LT",
        [CHECKED_EXPR_LE]               = "CHECKED_EXPR_LE",
        [CHECKED_EXPR_GT]               = "CHECKED_EXPR_GT",
        [CHECKED_EXPR_GE]               = "CHECKED_EXPR_GE",
        [CHECKED_EXPR_NEGATE]           = "CHECKED_EXPR_NEGATE",
        [CHECKED_EXPR_ADDROF]           = "CHECKED_EXPR_ADDROF",
        [CHECKED_EXPR_DEREF]            = "CHECKED_EXPR_DEREF",
        [CHECKED_EXPR_CALL]             = "CHECKED_EXPR_CALL",
        [CHECKED_EXPR_IDENTIFIER]       = "CHECKED_EXPR_IDENTIFIER"
    };
    return names[type];
}

void astCheckedExprNodePrint(FILE *to, ASTCheckedExprNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m", expr_node_name(n->type), expr_type_name(n->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location, true);
    fputs(", \x1b[1mdata_type:\x1b[0m ", to);
    checkedTypePrint(to, n->data_type, true);
    switch(n->type) {
        // Constant nodes
        case CHECKED_EXPR_NUMBER_CONSTANT:
        case CHECKED_EXPR_STRING_CONSTANT:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            valuePrint(to, NODE_AS(ASTCheckedConstantValueExpr, n)->value);
            break;
        // Obj nodes
        case CHECKED_EXPR_VARIABLE:
        case CHECKED_EXPR_FUNCTION:
            fputs(", \x1b[1mobj:\x1b[0m ", to);
            astCheckedObjPrintCompact(to, NODE_AS(ASTCheckedObjExpr, n)->obj);
            break;
        // binary nodes
        case CHECKED_EXPR_ASSIGN:
        case CHECKED_EXPR_PROPERTY_ACCESS:
        case CHECKED_EXPR_ADD:
        case CHECKED_EXPR_SUBTRACT:
        case CHECKED_EXPR_MULTIPLY:
        case CHECKED_EXPR_DIVIDE:
        case CHECKED_EXPR_EQ:
        case CHECKED_EXPR_NE:
        case CHECKED_EXPR_LT:
        case CHECKED_EXPR_LE:
        case CHECKED_EXPR_GT:
        case CHECKED_EXPR_GE:
            fputs(", \x1b[1mlhs:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedBinaryExpr, n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedBinaryExpr, n)->rhs);
            break;
        // Unary nodes
        case CHECKED_EXPR_NEGATE:
        case CHECKED_EXPR_ADDROF:
        case CHECKED_EXPR_DEREF:
            fputs(", \x1b[1moperand:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedUnaryExpr, n)->operand);
            break;
        // Call node
        case CHECKED_EXPR_CALL:
            fputs(", \x1b[1mcallee:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedCallExpr, n)->callee);
            fputs(", \x1b[1marguments:\x1b[0m [", to);
            PRINT_ARRAY(ASTCheckedExprNode *, astCheckedExprNodePrint, to, NODE_AS(ASTCheckedCallExpr, n)->arguments);
            fputc(']', to);
            break;
        // identifier nodes
        case CHECKED_EXPR_IDENTIFIER:
            fputs(", \x1b[1midentifier:\x1b[0m ", to);
            astStringPrint(to, &NODE_AS(ASTCheckedIdentifierExpr, n)->id);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

static inline ASTCheckedExprNode make_expr_node_header(ASTCheckedExprNodeType type, Location loc, CheckedType *ty) {
    return (ASTCheckedExprNode){
        .type = type,
        .location = loc,
        .data_type = ty
    };
}

ASTCheckedExprNode *astNewCheckedConstantValueExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, Value value, CheckedType *value_ty) {
    ASTCheckedConstantValueExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, value_ty);
    n->value = value;
    return NODE_AS(ASTCheckedExprNode, n);
}

ASTCheckedExprNode *astNewCheckedObjExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, ASTCheckedObj *obj) {
    ASTCheckedObjExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, obj->data_type);
    n->obj = obj;
    return NODE_AS(ASTCheckedExprNode, n);
}

ASTCheckedExprNode *astNewCheckedUnaryExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, ASTCheckedExprNode *operand) {
    ASTCheckedUnaryExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, NULL);
    n->operand = operand;
    return NODE_AS(ASTCheckedExprNode, n);
}

ASTCheckedExprNode *astNewCheckedBinaryExpr(Allocator *a, ASTCheckedExprNodeType type, Location loc, ASTCheckedExprNode *lhs, ASTCheckedExprNode *rhs) {
    ASTCheckedBinaryExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, NULL);
    n->lhs = lhs;
    n->rhs = rhs;
    return NODE_AS(ASTCheckedExprNode, n);
}

ASTCheckedExprNode *astNewCheckedCallExpr(Allocator *a, Location loc, ASTCheckedExprNode *callee, Array *arguments) {
    ASTCheckedCallExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(CHECKED_EXPR_CALL, loc, NULL);
    n->callee = callee;
    arrayInitAllocatorSized(&n->arguments, *a, arrayLength(arguments));
    arrayCopy(&n->arguments, arguments);
    return NODE_AS(ASTCheckedExprNode, n);
}

ASTCheckedExprNode *astNewCheckedIdentifierExpr(Allocator *a, Location loc, ASTString id) {
    ASTCheckedIdentifierExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(CHECKED_EXPR_IDENTIFIER, loc, NULL);
    n->id = id;
    return NODE_AS(ASTCheckedExprNode, n);
}


/* AST node - ASTCheckedStmtNode */

static const char *stmt_node_name(ASTCheckedStmtNodeType type) {
    switch(type) {
        // VarDecl node
        case CHECKED_STMT_VAR_DECL:
            return "ASTCheckedVarDeclStmt";
        // Block node
        case CHECKED_STMT_BLOCK:
            return "ASTCheckedBlockStmt";
        // Conditional node
        case CHECKED_STMT_IF:
            return "ASTCheckedConditionalStmt";
        // Loop node
        case CHECKED_STMT_WHILE_LOOP:
            return "ASTCheckedLoopStmt";
        // Expr nodes
        case CHECKED_STMT_RETURN:
        case CHECKED_STMT_DEFER:
        case CHECKED_STMT_EXPR:
            return "ASTCheckedExprStmt";
        default:
            UNREACHABLE();
    }
}

static const char *stmt_type_name(ASTCheckedStmtNodeType type) {
    static const char *names[] = {
        [CHECKED_STMT_VAR_DECL]      = "CHECKED_STMT_VAR_DECL",
        [CHECKED_STMT_BLOCK]         = "CHECKED_STMT_BLOCK",
        [CHECKED_STMT_IF]            = "CHECKED_STMT_IF",
        [CHECKED_STMT_WHILE_LOOP]    = "CHECKED_STMT_WHILE_LOOP",
        [CHECKED_STMT_RETURN]        = "CHECKED_STMT_RETURN",
        [CHECKED_STMT_DEFER]         = "CHECKED_STMT_DEFER",
        [CHECKED_STMT_EXPR]          = "CHECKED_STMT_EXPR",
    };
    return names[type];
}

void astCheckedStmtNodePrint(FILE *to, ASTCheckedStmtNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m", stmt_node_name(n->type), stmt_type_name(n->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location, true);
    switch(n->type) {
        // VarDeck node
        case CHECKED_STMT_VAR_DECL:
            fputs(", \x1b[1mvariable:\x1b[0m ", to);
            astCheckedObjPrintCompact(to, NODE_AS(ASTCheckedVarDeclStmt, n)->variable);
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedVarDeclStmt, n)->initializer);
            break;
        // Block nodes
        case CHECKED_STMT_BLOCK:
            fputs(", \x1b[1mscope:\x1b[0m ", to);
            scopeIDPrint(to, NODE_AS(ASTCheckedBlockStmt, n)->scope, true);
            fputs(", \x1b[1mcontrol_flow:\x1b[0m ", to);
            controlFlowPrint(to, NODE_AS(ASTCheckedBlockStmt, n)->control_flow);
            fputs(", \x1b[1mnodes:\x1b[0m [", to);
            PRINT_ARRAY(ASTCheckedStmtNode *, astCheckedStmtNodePrint, to, NODE_AS(ASTCheckedBlockStmt, n)->nodes);
            fputc(']', to);
            break;
        // Conditional nodes
        case CHECKED_STMT_IF:
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedConditionalStmt, n)->condition);
            fputs(", \x1b[1mthen:\x1b[0m ", to);
            astCheckedStmtNodePrint(to, (ASTCheckedStmtNode *)(NODE_AS(ASTCheckedConditionalStmt, n)->then));
            fputs(", \x1b[1melse:\x1b[0m ", to);
            astCheckedStmtNodePrint(to, (ASTCheckedStmtNode *)(NODE_AS(ASTCheckedConditionalStmt, n)->else_));
            break;
        // Loop node
        case CHECKED_STMT_WHILE_LOOP:
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astCheckedStmtNodePrint(to, NODE_AS(ASTCheckedLoopStmt, n)->initializer);
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedLoopStmt, n)->condition);
            fputs(", \x1b[1mincrement:\x1b[0m ", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedLoopStmt, n)->increment);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astCheckedStmtNodePrint(to, (ASTCheckedStmtNode *)(NODE_AS(ASTCheckedLoopStmt, n)->body));
            break;
        // Expr nodes
        case CHECKED_STMT_RETURN:
        case CHECKED_STMT_EXPR:
            fputs(", \x1b[1mexpr:\x1b[0m", to);
            astCheckedExprNodePrint(to, NODE_AS(ASTCheckedExprStmt, n)->expr);
            break;
        // Defer node
        case CHECKED_STMT_DEFER:
            fputs(", \x1b[1mbody:\x1n[0m ", to);
            astCheckedStmtNodePrint(to, NODE_AS(ASTCheckedDeferStmt, n)->body);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

static inline ASTCheckedStmtNode make_stmt_mode_header(ASTCheckedStmtNodeType type, Location loc) {
    return (ASTCheckedStmtNode){
        .type = type,
        .location = loc
    };
}

ASTCheckedStmtNode *astNewCheckedVarDeclStmt(Allocator *a, Location loc, ASTCheckedObj *variable, ASTCheckedExprNode *initializer) {
    ASTCheckedVarDeclStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(CHECKED_STMT_VAR_DECL, loc);
    n->variable = variable;
    n->initializer = initializer;
    return NODE_AS(ASTCheckedStmtNode, n);
}

ASTCheckedStmtNode *astNewCheckedBlockStmt(Allocator *a, Location loc, ScopeID scope, ControlFlow control_flow, Array *nodes) {
    ASTCheckedBlockStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(CHECKED_STMT_BLOCK, loc);
    n->scope = scope;
    n->control_flow = control_flow;
    arrayInitAllocatorSized(&n->nodes, *a, arrayLength(nodes));
    arrayCopy(&n->nodes, nodes);
    return NODE_AS(ASTCheckedStmtNode, n);
}

ASTCheckedStmtNode *astNewCheckedConditionalStmt(Allocator *a, Location loc, ASTCheckedExprNode *condition, ASTCheckedBlockStmt *then, ASTCheckedStmtNode *else_) {
    ASTCheckedConditionalStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(CHECKED_STMT_IF, loc);
    n->condition = condition;
    n->then = then;
    n->else_ = else_;
    return NODE_AS(ASTCheckedStmtNode, n);
}

ASTCheckedStmtNode *astNewCheckedLoopStmt(Allocator *a, ASTCheckedStmtNodeType type, Location loc, ASTCheckedStmtNode *initializer, ASTCheckedExprNode *condition, ASTCheckedExprNode *increment, ASTCheckedBlockStmt *body) {
    ASTCheckedLoopStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(type, loc);
    n->initializer = initializer;
    n->condition = condition;
    n->increment = increment;
    n->body = body;
    return NODE_AS(ASTCheckedStmtNode, n);
}

ASTCheckedStmtNode *astNewCheckedExprStmt(Allocator *a, ASTCheckedStmtNodeType type, Location loc, ASTCheckedExprNode *expr) {
    ASTCheckedExprStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(type, loc);
    n->expr = expr;
    return NODE_AS(ASTCheckedStmtNode, n);
}

ASTCheckedStmtNode *astNewCheckedDeferStmt(Allocator *a, Location loc, ASTCheckedStmtNode *body) {
    ASTCheckedDeferStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(CHECKED_STMT_DEFER, loc);
    n->body = body;
    return NODE_AS(ASTCheckedStmtNode, n);
}


/* ASTCheckedObj */

ASTCheckedObj *astNewCheckedObj(ASTObjType type, Location loc, ASTString name, CheckedType *data_type) {
    ASTCheckedObj *o;
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

void astFreeCheckedObj(ASTCheckedObj *obj) {
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

void astCheckedObjPrintCompact(FILE *to, ASTCheckedObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }
    fprintf(to, "ASTCheckedObj{\x1b[1;36m%s\x1b[0m, ", __ast_obj_type_name(obj->type));
    locationPrint(to, obj->location, true);
    fputs(", ", to);
    astStringPrint(to, &obj->name);
    fputs(", ", to);
    checkedTypePrint(to, obj->data_type, true);
    fputc('}', to);
}

void astPrintCheckedObj(FILE *to, ASTCheckedObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }

    fprintf(to, "ASTCheckedObj{\x1b[1mtype: \x1b[36m%s\x1b[0m", __ast_obj_type_name(obj->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, obj->location, true);
    // Note: name loc is not printed to declutter the output.

    fputs(", \x1b[1mname:\x1b[0m ", to);
    astStringPrint(to, &obj->name);
    fputs(", \x1b[1mdata_type:\x1b[0m ", to);
    checkedTypePrint(to, obj->data_type, true);
    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
            checkedTypePrint(to, obj->as.fn.return_type, true);
            fputs(", \x1b[1mparameters:\x1b[0m [", to);
            PRINT_ARRAY(ASTCheckedObj *, astPrintCheckedObj, to, obj->as.fn.parameters);
            fputs("], \x1b[1mbody:\x1b[0m ", to);
            astCheckedStmtNodePrint(to, (ASTCheckedStmtNode *)obj->as.fn.body);
            break;
        case OBJ_STRUCT:
            fputs(", \x1b[1mscope:\x1b[0m ", to);
            // FIXME: Ideally the actual scope should be printed.
            scopeIDPrint(to, obj->as.structure.scope, false);
            break;
        case OBJ_EXTERN_FN:
            fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
            checkedTypePrint(to, obj->as.fn.return_type, true);
            fputs(", \x1b[1mparameters:\x1b[0m [", to);
            PRINT_ARRAY(ASTCheckedObj *, astPrintCheckedObj, to, obj->as.fn.parameters);
            fputs("], \x1b[1msource:\x1b[0m ", to);
            attributePrint(to, obj->as.extern_fn.source_attr);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


/* ASTCheckedModule */

ASTCheckedModule *astNewCheckedModule(ASTString name) {
    ASTCheckedModule *m;
    NEW0(m);
    m->name = name;
    arenaInit(&m->ast_allocator.storage);
    m->ast_allocator.alloc = arenaMakeAllocator(&m->ast_allocator.storage);
    arrayInit(&m->scopes);
    m->module_scope = checkedScopeNew(EMPTY_SCOPE_ID, false);
    // Note: if module_scope is not the first scope, change astCheckedModuleGetModuleScopeID().
    arrayPush(&m->scopes, m->module_scope);
    arrayInit(&m->globals);
    return m;
}

void astFreeCheckedModule(ASTCheckedModule *module) {
    module->module_scope = NULL;
    arrayMap(&module->scopes, free_scope_callback, NULL);
    arrayFree(&module->scopes);
    arrayFree(&module->globals);
    arenaFree(&module->ast_allocator.storage);
    FREE(module);
}

void astPrintCheckedModule(FILE *to, ASTCheckedModule *module) {
    fputs("ASTCheckedModule{\x1b[1mname:\x1b[0m ", to);
    astStringPrint(to, &module->name);
    fputs(", \x1b[1mscopes:\x1b[0m [", to);
    PRINT_ARRAY(CheckedScope *, checkedScopePrint, to, module->scopes);
    fputs("], \x1b[1mglobals:\x1b[0m [", to);
    PRINT_ARRAY(ASTCheckedStmtNode *, astCheckedStmtNodePrint, to, module->globals);
    fputs("]}", to);
}

ScopeID astCheckedModuleAddScope(ASTCheckedModule *module, CheckedScope *scope) {
    ScopeID id;
    id.module = module->id;
    id.index = (ModuleID)arrayPush(&module->scopes, (void *)scope);
    return id;
}

CheckedScope *astCheckedModuleGetScope(ASTCheckedModule *module, ScopeID id) {
    VERIFY(module->id == id.module);
    VERIFY(id.index < arrayLength(&module->scopes));
    CheckedScope *scope = ARRAY_GET_AS(CheckedScope *, &module->scopes, id.index);
    VERIFY(scope != NULL);
    return scope;
}

ScopeID astCheckedModuleGetModuleScopeID(ASTCheckedModule *module) {
    return (ScopeID){
        .module = module->id,
        .index = 0
    };
}


/* ASTCheckedProgram */

void astCheckedProgramInit(ASTCheckedProgram *prog) {
    astStringTableInit(&prog->strings);
    arrayInit(&prog->modules);
#define NULL_TY(name) prog->primitives.name = NULL;
    NULL_TY(void_);
    NULL_TY(int32);
    NULL_TY(uint32);
    NULL_TY(str);
#undef NULL_TY
}

void astCheckedProgramFree(ASTCheckedProgram *prog) {
    astStringTableFree(&prog->strings);
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
}

void astCheckedProgramPrint(FILE *to, ASTCheckedProgram *prog) {
    fputs("ASTCheckedProgram{\x1b[1mmodules:\x1b[0m [", to);
    PRINT_ARRAY(ASTCheckedModule *, astPrintCheckedModule, to, prog->modules);
    fputc(']', to);

    fputs(", \x1b[1mstrings:\x1b[0m ", to);
    astStringTablePrint(to, &prog->strings);
    fputs("}", to);
}

ModuleID astCheckedProgramAddModule(ASTCheckedProgram *prog, ASTCheckedModule *module) {
    return (ModuleID)arrayPush(&prog->modules, (void *)module);
}

ASTCheckedModule *astCheckedProgramGetModule(ASTCheckedProgram *prog, ModuleID id) {
    // arrayGet() returns NULL on invalid indexes.
    ASTCheckedModule *m = ARRAY_GET_AS(ASTCheckedModule *, &prog->modules, id);
    VERIFY(m != NULL);
    return m;
}
