#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Types/ParsedType.h"
#include "Ast/ast_common.h"
#include "Ast/ParsedAst.h"

/* callbacks */

static void free_object_callback(void *object, void *cl) {
    UNUSED(cl);
    astFreeParsedObj((ASTParsedObj *)object);
}

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astFreeParsedModule((ASTParsedModule *)module);
}

static void free_scope_callback(void *scope, void *cl) {
    UNUSED(cl);
    parsedScopeFree((ParsedScope *)scope);
}

static void free_type_table_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
    UNUSED(cl);
    ParsedType *ty = (ParsedType *)item->key;
    parsedTypeFree(ty);
    FREE(ty);
}

static unsigned hash_type(void *type) {
    ParsedType *ty = (ParsedType *)type;
    return parsedTypeHash(ty);
}

static bool compare_type(void *a_type, void *b_type) {
    ParsedType *a = (ParsedType *)a_type;
    ParsedType *b = (ParsedType *)b_type;
    return parsedTypeEqual(a, b);
}

static void print_type_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    ParsedType *ty = (ParsedType *)item->key;
    parsedTypePrint(to, ty, false);
    if(!is_last) {
        fputs(", ", to);
    }
}

static void print_object_table_callback(TableItem *item, bool is_last, void *stream) {
    FILE *to = (FILE *)stream;
    ASTParsedObj *obj = (ASTParsedObj *)item->value;
    astPrintParsedObj(to, obj);
    if(!is_last) {
        fputs(", ", to);
    }
}


/* ParsedScope */

ParsedScope *parsedScopeNew(ScopeID parent_scope, bool is_block_scope) {
    ParsedScope *sc;
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

void parsedScopeAddChild(ParsedScope *parent, ScopeID child_id) {
    if(parent->children.length + 1 > parent->children.capacity) {
        // Note: The initial capacity of 4 is pretty random. 4 just seemed like a good number
        //       that will be enough most of the time.
        parent->children.capacity = parent->children.capacity == 0 ? 4 : parent->children.capacity * 2;
        parent->children.children_scope_ids = REALLOC(parent->children.children_scope_ids, sizeof(*parent->children.children_scope_ids) * parent->children.capacity);
    }
    parent->children.children_scope_ids[parent->children.length++] = child_id;
}

ParsedType *parsedScopeAddType(ParsedScope *scope, ParsedType *type) {
    TableItem *existing_item = NULL;
    if((existing_item = tableGet(&scope->types, (void *)type)) != NULL) {
        FREE(type);
        return (ParsedType *)existing_item->key;
    }
    tableSet(&scope->types, (void *)type, NULL);
    return type;
}

void parsedScopeFree(ParsedScope *scope_list) {
    if(scope_list == NULL) {
        return;
    }
    // No need to free the strings in the table as they are
    // ASTString's which are owned by the ASTParsedProgram
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

void parsedScopePrint(FILE *to, ParsedScope *scope) {
    fprintf(to, "ParsedScope{\x1b[1mis_block_scope: \x1b[31m%s\x1b[0m", scope->is_block_scope ? "true" : "false");
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


/* AST node - ASTParsedExprNode */

static const char *expr_node_name(ASTParsedExprNodeType type) {
    switch(type) {
        // Constant nodes
        case PARSED_EXPR_NUMBER_CONSTANT:
        case PARSED_EXPR_STRING_CONSTANT:
            return "ASTParsedConstantValueExpr";
        // Object nodes
        case PARSED_EXPR_VARIABLE:
        case PARSED_EXPR_FUNCTION:
            return "ASTParsedObjExpr";
        // Binary nodes
        case PARSED_EXPR_ASSIGN:
        case PARSED_EXPR_PROPERTY_ACCESS:
        case PARSED_EXPR_ADD:
        case PARSED_EXPR_SUBTRACT:
        case PARSED_EXPR_MULTIPLY:
        case PARSED_EXPR_DIVIDE:
        case PARSED_EXPR_EQ:
        case PARSED_EXPR_NE:
        case PARSED_EXPR_LT:
        case PARSED_EXPR_LE:
        case PARSED_EXPR_GT:
        case PARSED_EXPR_GE:
            return "ASTParsedBinaryExpr";
        // Unary nodes
        case PARSED_EXPR_NEGATE:
        case PARSED_EXPR_ADDROF:
        case PARSED_EXPR_DEREF:
            return "ASTUnaryNode";
        case PARSED_EXPR_CALL:
            return "ASTParsedCallExpr";
        // identifier nodes
        case PARSED_EXPR_IDENTIFIER:
            return "ASTParsedIdentifierExpr";
        default:
            UNREACHABLE();
    }
}

static const char *expr_type_name(ASTParsedExprNodeType type) {
    static const char *names[] = {
        [PARSED_EXPR_NUMBER_CONSTANT]  = "PARSED_EXPR_NUMBER_CONSTANT",
        [PARSED_EXPR_STRING_CONSTANT]  = "PARSED_EXPR_STRING_CONSTANT",
        [PARSED_EXPR_VARIABLE]         = "PARSED_EXPR_VARIABLE",
        [PARSED_EXPR_FUNCTION]         = "PARSED_EXPR_FUNCTION",
        [PARSED_EXPR_ASSIGN]           = "PARSED_EXPR_ASSIGN",
        [PARSED_EXPR_PROPERTY_ACCESS]  = "PARSED_EXPR_PROPERTY_ACCESS",
        [PARSED_EXPR_ADD]              = "PARSED_EXPR_ADD",
        [PARSED_EXPR_SUBTRACT]         = "PARSED_EXPR_SUBTRACT",
        [PARSED_EXPR_MULTIPLY]         = "PARSED_EXPR_MULTIPLY",
        [PARSED_EXPR_DIVIDE]           = "PARSED_EXPR_DIVIDE",
        [PARSED_EXPR_EQ]               = "PARSED_EXPR_EQ",
        [PARSED_EXPR_NE]               = "PARSED_EXPR_NE",
        [PARSED_EXPR_LT]               = "PARSED_EXPR_LT",
        [PARSED_EXPR_LE]               = "PARSED_EXPR_LE",
        [PARSED_EXPR_GT]               = "PARSED_EXPR_GT",
        [PARSED_EXPR_GE]               = "PARSED_EXPR_GE",
        [PARSED_EXPR_NEGATE]           = "PARSED_EXPR_NEGATE",
        [PARSED_EXPR_ADDROF]           = "PARSED_EXPR_ADDROF",
        [PARSED_EXPR_DEREF]            = "PARSED_EXPR_DEREF",
        [PARSED_EXPR_CALL]             = "PARSED_EXPR_CALL",
        [PARSED_EXPR_IDENTIFIER]       = "PARSED_EXPR_IDENTIFIER"
    };
    return names[type];
}

void astParsedExprNodePrint(FILE *to, ASTParsedExprNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m", expr_node_name(n->type), expr_type_name(n->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location, true);
    fputs(", \x1b[1mdata_type:\x1b[0m ", to);
    parsedTypePrint(to, n->data_type, true);
    switch(n->type) {
        // Constant nodes
        case PARSED_EXPR_NUMBER_CONSTANT:
        case PARSED_EXPR_STRING_CONSTANT:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            valuePrint(to, NODE_AS(ASTParsedConstantValueExpr, n)->value);
            break;
        // Obj nodes
        case PARSED_EXPR_VARIABLE:
        case PARSED_EXPR_FUNCTION:
            fputs(", \x1b[1mobj:\x1b[0m ", to);
            astParsedObjPrintCompact(to, NODE_AS(ASTParsedObjExpr, n)->obj);
            break;
        // binary nodes
        case PARSED_EXPR_ASSIGN:
        case PARSED_EXPR_PROPERTY_ACCESS:
        case PARSED_EXPR_ADD:
        case PARSED_EXPR_SUBTRACT:
        case PARSED_EXPR_MULTIPLY:
        case PARSED_EXPR_DIVIDE:
        case PARSED_EXPR_EQ:
        case PARSED_EXPR_NE:
        case PARSED_EXPR_LT:
        case PARSED_EXPR_LE:
        case PARSED_EXPR_GT:
        case PARSED_EXPR_GE:
            fputs(", \x1b[1mlhs:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedBinaryExpr, n)->lhs);
            fputs(", \x1b[1mrhs:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedBinaryExpr, n)->rhs);
            break;
        // Unary nodes
        case PARSED_EXPR_NEGATE:
        case PARSED_EXPR_ADDROF:
        case PARSED_EXPR_DEREF:
            fputs(", \x1b[1moperand:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedUnaryExpr, n)->operand);
            break;
        // Call node
        case PARSED_EXPR_CALL:
            fputs(", \x1b[1mcallee:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedCallExpr, n)->callee);
            fputs(", \x1b[1marguments:\x1b[0m [", to);
            PRINT_ARRAY(ASTParsedExprNode *, astParsedExprNodePrint, to, NODE_AS(ASTParsedCallExpr, n)->arguments);
            fputc(']', to);
            break;
        // identifier nodes
        case PARSED_EXPR_IDENTIFIER:
            fputs(", \x1b[1midentifier:\x1b[0m ", to);
            astStringPrint(to, &NODE_AS(ASTParsedIdentifierExpr, n)->id);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

static inline ASTParsedExprNode make_expr_node_header(ASTParsedExprNodeType type, Location loc, ParsedType *ty) {
    return (ASTParsedExprNode){
        .type = type,
        .location = loc,
        .data_type = ty
    };
}

ASTParsedExprNode *astNewParsedConstantValueExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, Value value, ParsedType *value_ty) {
    ASTParsedConstantValueExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, value_ty);
    n->value = value;
    return NODE_AS(ASTParsedExprNode, n);
}

ASTParsedExprNode *astNewParsedObjExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, ASTParsedObj *obj) {
    ASTParsedObjExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, obj->data_type);
    n->obj = obj;
    return NODE_AS(ASTParsedExprNode, n);
}

ASTParsedExprNode *astNewParsedUnaryExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, ASTParsedExprNode *operand) {
    ASTParsedUnaryExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, NULL);
    n->operand = operand;
    return NODE_AS(ASTParsedExprNode, n);
}

ASTParsedExprNode *astNewParsedBinaryExpr(Allocator *a, ASTParsedExprNodeType type, Location loc, ASTParsedExprNode *lhs, ASTParsedExprNode *rhs) {
    ASTParsedBinaryExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(type, loc, NULL);
    n->lhs = lhs;
    n->rhs = rhs;
    return NODE_AS(ASTParsedExprNode, n);
}

ASTParsedExprNode *astNewParsedCallExpr(Allocator *a, Location loc, ASTParsedExprNode *callee, Array *arguments) {
    ASTParsedCallExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(PARSED_EXPR_CALL, loc, NULL);
    n->callee = callee;
    arrayInitAllocatorSized(&n->arguments, *a, arrayLength(arguments));
    arrayCopy(&n->arguments, arguments);
    return NODE_AS(ASTParsedExprNode, n);
}

ASTParsedExprNode *astNewParsedIdentifierExpr(Allocator *a, Location loc, ASTString id) {
    ASTParsedIdentifierExpr *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_expr_node_header(PARSED_EXPR_IDENTIFIER, loc, NULL);
    n->id = id;
    return NODE_AS(ASTParsedExprNode, n);
}


/* AST node - ASTParsedStmtNode */

static const char *stmt_node_name(ASTParsedStmtNodeType type) {
    switch(type) {
        // VarDecl node
        case PARSED_STMT_VAR_DECL:
            return "ASTParsedVarDeclStmt";
        // Block node
        case PARSED_STMT_BLOCK:
            return "ASTParsedBlockStmt";
        // Conditional node
        case PARSED_STMT_IF:
            return "ASTParsedConditionalStmt";
        // Loop node
        case PARSED_STMT_WHILE_LOOP:
            return "ASTParsedLoopStmt";
        // Expr nodes
        case PARSED_STMT_RETURN:
        case PARSED_STMT_DEFER:
        case PARSED_STMT_EXPR:
            return "ASTParsedExprStmt";
        default:
            UNREACHABLE();
    }
}

static const char *stmt_type_name(ASTParsedStmtNodeType type) {
    static const char *names[] = {
        [PARSED_STMT_VAR_DECL]      = "PARSED_STMT_VAR_DECL",
        [PARSED_STMT_BLOCK]         = "PARSED_STMT_BLOCK",
        [PARSED_STMT_IF]            = "PARSED_STMT_IF",
        [PARSED_STMT_WHILE_LOOP]    = "PARSED_STMT_WHILE_LOOP",
        [PARSED_STMT_RETURN]        = "PARSED_STMT_RETURN",
        [PARSED_STMT_DEFER]         = "PARSED_STMT_DEFER",
        [PARSED_STMT_EXPR]          = "PARSED_STMT_EXPR",
    };
    return names[type];
}

void astParsedStmtNodePrint(FILE *to, ASTParsedStmtNode *n) {
    if(n == NULL) {
        fputs("(null)", to);
        return;
    }
    fprintf(to, "%s{\x1b[1mtype: \x1b[33m%s\x1b[0m", stmt_node_name(n->type), stmt_type_name(n->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, n->location, true);
    switch(n->type) {
        // VarDeck node
        case PARSED_STMT_VAR_DECL:
            fputs(", \x1b[1mvariable:\x1b[0m ", to);
            astParsedObjPrintCompact(to, NODE_AS(ASTParsedVarDeclStmt, n)->variable);
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedVarDeclStmt, n)->initializer);
            break;
        // Block nodes
        case PARSED_STMT_BLOCK:
            fputs(", \x1b[1mscope:\x1b[0m ", to);
            scopeIDPrint(to, NODE_AS(ASTParsedBlockStmt, n)->scope, true);
            fputs(", \x1b[1mcontrol_flow:\x1b[0m ", to);
            controlFlowPrint(to, NODE_AS(ASTParsedBlockStmt, n)->control_flow);
            fputs(", \x1b[1mnodes:\x1b[0m [", to);
            PRINT_ARRAY(ASTParsedStmtNode *, astParsedStmtNodePrint, to, NODE_AS(ASTParsedBlockStmt, n)->nodes);
            fputc(']', to);
            break;
        // Conditional nodes
        case PARSED_STMT_IF:
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedConditionalStmt, n)->condition);
            fputs(", \x1b[1mthen:\x1b[0m ", to);
            astParsedStmtNodePrint(to, (ASTParsedStmtNode *)(NODE_AS(ASTParsedConditionalStmt, n)->then));
            fputs(", \x1b[1melse:\x1b[0m ", to);
            astParsedStmtNodePrint(to, (ASTParsedStmtNode *)(NODE_AS(ASTParsedConditionalStmt, n)->else_));
            break;
        // Loop node
        case PARSED_STMT_WHILE_LOOP:
            fputs(", \x1b[1minitializer:\x1b[0m ", to);
            astParsedStmtNodePrint(to, NODE_AS(ASTParsedLoopStmt, n)->initializer);
            fputs(", \x1b[1mcondition:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedLoopStmt, n)->condition);
            fputs(", \x1b[1mincrement:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedLoopStmt, n)->increment);
            fputs(", \x1b[1mbody:\x1b[0m ", to);
            astParsedStmtNodePrint(to, (ASTParsedStmtNode *)(NODE_AS(ASTParsedLoopStmt, n)->body));
            break;
        // Expr nodes
        case PARSED_STMT_RETURN:
        case PARSED_STMT_DEFER:
        case PARSED_STMT_EXPR:
            fputs(", \x1b[1mexpr:\x1b[0m ", to);
            astParsedExprNodePrint(to, NODE_AS(ASTParsedExprStmt, n)->expr);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

static inline ASTParsedStmtNode make_stmt_mode_header(ASTParsedStmtNodeType type, Location loc) {
    return (ASTParsedStmtNode){
        .type = type,
        .location = loc
    };
}

ASTParsedStmtNode *astNewParsedVarDeclStmt(Allocator *a, Location loc, ASTParsedObj *variable, ASTParsedExprNode *initializer) {
    ASTParsedVarDeclStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(PARSED_STMT_VAR_DECL, loc);
    n->variable = variable;
    n->initializer = initializer;
    return NODE_AS(ASTParsedStmtNode, n);
}

ASTParsedStmtNode *astNewParsedBlockStmt(Allocator *a, Location loc, ScopeID scope, ControlFlow control_flow, Array *nodes) {
    ASTParsedBlockStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(PARSED_STMT_BLOCK, loc);
    n->scope = scope;
    n->control_flow = control_flow;
    arrayInitAllocatorSized(&n->nodes, *a, arrayLength(nodes));
    arrayCopy(&n->nodes, nodes);
    return NODE_AS(ASTParsedStmtNode, n);
}

ASTParsedStmtNode *astNewParsedConditionalStmt(Allocator *a, ASTParsedStmtNodeType type, Location loc, ASTParsedExprNode *condition, ASTParsedBlockStmt *then, ASTParsedStmtNode *else_) {
    ASTParsedConditionalStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(type, loc);
    n->condition = condition;
    n->then = then;
    n->else_ = else_;
    return NODE_AS(ASTParsedStmtNode, n);
}

ASTParsedStmtNode *astNewParsedLoopStmt(Allocator *a, ASTParsedStmtNodeType type, Location loc, ASTParsedStmtNode *initializer, ASTParsedExprNode *condition, ASTParsedExprNode *increment, ASTParsedBlockStmt *body) {
    ASTParsedLoopStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(type, loc);
    n->initializer = initializer;
    n->condition = condition;
    n->increment = increment;
    n->body = body;
    return NODE_AS(ASTParsedStmtNode, n);
}

ASTParsedStmtNode *astNewParsedExprStmt(Allocator *a, ASTParsedStmtNodeType type, Location loc, ASTParsedExprNode *expr) {
    ASTParsedExprStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(type, loc);
    n->expr = expr;
    return NODE_AS(ASTParsedStmtNode, n);
}

ASTParsedStmtNode *astNewParsedDeferStmt(Allocator *a, Location loc, ASTParsedStmtNode *body) {
    ASTParsedDeferStmt *n = allocatorAllocate(a, sizeof(*n));
    n->header = make_stmt_mode_header(PARSED_STMT_DEFER, loc);
    n->body = body;
    return NODE_AS(ASTParsedStmtNode, n);
}


/* ASTParsedObj */

ASTParsedObj *astNewParsedObj(ASTObjType type, Location loc, ASTString name, ParsedType *data_type) {
    ASTParsedObj *o;
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

void astFreeParsedObj(ASTParsedObj *obj) {
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

static const char *obj_type_name(ASTObjType type) {
    static const char *names[] = {
        [OBJ_VAR]       = "OBJ_VAR",
        [OBJ_FN]        = "OBJ_FN",
        [OBJ_STRUCT]    = "OBJ_STRUCT",
        [OBJ_EXTERN_FN] = "OBJ_EXTERN_FN"
    };
    return names[type];
}

void astParsedObjPrintCompact(FILE *to, ASTParsedObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }
    fprintf(to, "ASTParsedObj{\x1b[1;36m%s\x1b[0m, ", obj_type_name(obj->type));
    locationPrint(to, obj->location, true);
    fputs(", ", to);
    astStringPrint(to, &obj->name);
    fputs(", ", to);
    parsedTypePrint(to, obj->data_type, true);
    fputc('}', to);
}

void astPrintParsedObj(FILE *to, ASTParsedObj *obj) {
    if(obj == NULL) {
        fprintf(to, "(null)");
        return;
    }

    fprintf(to, "ASTParsedObj{\x1b[1mtype: \x1b[36m%s\x1b[0m", obj_type_name(obj->type));
    fputs(", \x1b[1mlocation:\x1b[0m ", to);
    locationPrint(to, obj->location, true);

    fputs(", \x1b[1mname:\x1b[0m ", to);
    astStringPrint(to, &obj->name);
    fputs(", \x1b[1mdata_type:\x1b[0m ", to);
    parsedTypePrint(to, obj->data_type, true);
    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
            parsedTypePrint(to, obj->as.fn.return_type, true);
            fputs(", \x1b[1mparameters:\x1b[0m [", to);
            PRINT_ARRAY(ASTParsedObj *, astPrintParsedObj, to, obj->as.fn.parameters);
            fputs("], \x1b[1mbody:\x1b[0m ", to);
            astParsedStmtNodePrint(to, (ASTParsedStmtNode *)obj->as.fn.body);
            break;
        case OBJ_STRUCT:
            fputs(", \x1b[1mscope:\x1b[0m ", to);
            // FIXME: Ideally the actual scope should be printed.
            scopeIDPrint(to, obj->as.structure.scope, false);
            break;
        case OBJ_EXTERN_FN:
            fputs(", \x1b[1mreturn_type:\x1b[0m ", to);
            parsedTypePrint(to, obj->as.fn.return_type, true);
            fputs(", \x1b[1mparameters:\x1b[0m [", to);
            PRINT_ARRAY(ASTParsedObj *, astPrintParsedObj, to, obj->as.fn.parameters);
            fputs("], \x1b[1msource:\x1b[0m ", to);
            attributePrint(to, obj->as.extern_fn.source_attr);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}


/* ASTParsedModule */

ASTParsedModule *astNewParsedModule(ASTString name) {
    ASTParsedModule *m;
    NEW0(m);
    m->name = name;
    arenaInit(&m->ast_allocator.storage);
    m->ast_allocator.alloc = arenaMakeAllocator(&m->ast_allocator.storage);
    arrayInit(&m->scopes);
    m->module_scope = parsedScopeNew(EMPTY_SCOPE_ID, false);
    // Note: if module_scope is not the first scope, change astParsedModuleGetModuleScopeID().
    arrayPush(&m->scopes, m->module_scope);
    arrayInit(&m->globals);
    return m;
}

void astFreeParsedModule(ASTParsedModule *module) {
    module->module_scope = NULL;
    arrayMap(&module->scopes, free_scope_callback, NULL);
    arrayFree(&module->scopes);
    arrayFree(&module->globals);
    arenaFree(&module->ast_allocator.storage);
    FREE(module);
}

void astPrintParsedModule(FILE *to, ASTParsedModule *module) {
    fputs("ASTParsedModule{\x1b[1mname:\x1b[0m ", to);
    astStringPrint(to, &module->name);
    fputs(", \x1b[1mscopes:\x1b[0m [", to);
    PRINT_ARRAY(ParsedScope *, parsedScopePrint, to, module->scopes);
    fputs("], \x1b[1mglobals:\x1b[0m [", to);
    PRINT_ARRAY(ASTParsedStmtNode *, astParsedStmtNodePrint, to, module->globals);
    fputs("]}", to);
}

ScopeID astParsedModuleAddScope(ASTParsedModule *module, ParsedScope *scope) {
    ScopeID id;
    id.module = module->id;
    id.index = (ModuleID)arrayPush(&module->scopes, (void *)scope);
    return id;
}

ParsedScope *astParsedModuleGetScope(ASTParsedModule *module, ScopeID id) {
    VERIFY(module->id == id.module);
    VERIFY(id.index < arrayLength(&module->scopes));
    ParsedScope *scope = ARRAY_GET_AS(ParsedScope *, &module->scopes, id.index);
    VERIFY(scope != NULL);
    return scope;
}

ScopeID astParsedModuleGetModuleScopeID(ASTParsedModule *module) {
    return (ScopeID){
        .module = module->id,
        .index = 0
    };
}


/* ASTParsedProgram */

void astParsedProgramInit(ASTParsedProgram *prog) {
    astStringTableInit(&prog->strings);
    arrayInit(&prog->modules);
#define NULL_TY(name) prog->primitives.name = NULL;
    NULL_TY(void_);
    NULL_TY(int32);
    NULL_TY(uint32);
    NULL_TY(str);
#undef NULL_TY
}

void astParsedProgramFree(ASTParsedProgram *prog) {
    astStringTableFree(&prog->strings);
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
}

void astParsedProgramPrint(FILE *to, ASTParsedProgram *prog) {
    fputs("ASTParsedProgram{\x1b[1mmodules:\x1b[0m [", to);
    PRINT_ARRAY(ASTParsedModule *, astPrintParsedModule, to, prog->modules);
    fputc(']', to);

    fputs(", \x1b[1mstrings:\x1b[0m ", to);
    astStringTablePrint(to, &prog->strings);
    fputs("}", to);
}

ModuleID astParsedProgramAddModule(ASTParsedProgram *prog, ASTParsedModule *module) {
    return (ModuleID)arrayPush(&prog->modules, (void *)module);
}

ASTParsedModule *astParsedProgramGetModule(ASTParsedProgram *prog, ModuleID id) {
    // arrayGet() returns NULL on invalid indexes.
    ASTParsedModule *m = ARRAY_GET_AS(ASTParsedModule *, &prog->modules, id);
    VERIFY(m != NULL);
    return m;
}
