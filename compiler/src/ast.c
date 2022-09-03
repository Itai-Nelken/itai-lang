#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Token.h"
#include "Symbols.h"
#include "ast.h"

// callbacks
static void free_object_callback(void *obj, void *cl) {
    UNUSED(cl);
    astFreeObj(AS_OBJ(obj));
}

static void free_module_callback(void *module, void *cl) {
    UNUSED(cl);
    astFreeModule((ASTModule *)module);
}

ASTIdentifier *astNewIdentifier(Location loc, SymbolID id) {
    ASTIdentifier *ident;
    NEW0(ident);
    ident->location = loc;
    ident->id = id;
    return ident;
}

void astFreeIdentifier(ASTIdentifier *id) {
    FREE(id);
}

void astPrintIdentifier(FILE *to, ASTIdentifier *id) {
    fprintf(to, "ASTIdentifier{\x1b[1mlocation:\x1b[0m ");
    printLocation(to, id->location);
    fprintf(to, ", \x1b[1mid:\x1b[0m ");
    symbolIDPrint(to, id->id);
    fputc('}', to);
}

ASTObj *astNewFunctionObj(Location loc, ASTIdentifier *name, SymbolID return_type_id, ASTListNode *body) {
    ASTFunctionObj *fn;
    NEW0(fn);
    fn->header = (ASTObj){
        .type = OBJ_FUNCTION,
        .location = loc,
        .name = name,
    };
    fn->return_type = return_type_id;
    fn->body = body;
    return AS_OBJ(fn);
}

void astFreeObj(ASTObj *obj) {
    if(!obj) {
        return;
    }
    if(obj->name) {
        astFreeIdentifier(obj->name);
    }
    switch(obj->type) {
        case OBJ_FUNCTION:
            astFree(AS_NODE(AS_FUNCTION_OBJ(obj)->body));
            break;
        default:
            UNREACHABLE();
    }
    FREE(obj);
}

static const char *obj_name(ASTObjType type) {
    static const char *names[] = {
        [OBJ_FUNCTION] = "ASTFunctionObj"
    };
    return names[(i32)type];
}

void astPrintObj(FILE *to, ASTObj *obj) {
    fprintf(to, "%s{\x1b[1mlocation:\x1b[0m ", obj_name(obj->type));
    printLocation(to, obj->location);
    fprintf(to, ", \x1b[1mname:\x1b[0m ");
    astPrintIdentifier(to, obj->name);
    switch(obj->type) {
        case OBJ_FUNCTION:
            fprintf(to, ", \x1b[1mreturn_type:\x1b[0m ");
            symbolIDPrint(to, AS_FUNCTION_OBJ(obj)->return_type);
            fprintf(to, ", \x1b[1mbody:\x1b[0m ");
            astPrint(to, AS_NODE(AS_FUNCTION_OBJ(obj)->body));
            break;
        default:
            break;
    }
    fputc('}', to);
}

ASTModule *astNewModule(ASTIdentifier *name) {
    ASTModule *m;
    NEW0(m);
    m->name = name;
    arrayInit(&m->objects);
    return m;
}

void astFreeModule(ASTModule *m) {
    astFreeIdentifier(m->name);
    arrayMap(&m->objects, free_object_callback, NULL);
    arrayFree(&m->objects);
    FREE(m);
}

void astPrintModule(FILE *to, ASTModule *m) {
    fputs("ASTModule{\x1b[1mname:\x1b[0m ", to);
    astPrintIdentifier(to, m->name);
    fputs(", \x1b[1mobjects:\x1b[0m [", to);
    for(usize i = 0; i < m->objects.used; ++i) {
        astPrintObj(to, ARRAY_GET_AS(ASTObj *, &m->objects, i));
        if(i + 1 < m->objects.used) {
            fputs(", ", to);
        }
    }
    fputs("]}", to);
}

static void init_primitives(SymbolID ids[TY_COUNT], SymbolTable *syms) {
    SymbolID name;
    DataType ty;

    // void
    name = symbolTableAddIdentifier(syms, "void", 4);
    ty = dataTypeNew(name, 0, false);
    ids[TY_VOID] = symbolTableAddType(syms, ty);

    // i32
    name = symbolTableAddIdentifier(syms, "i32", 3);
    ty = dataTypeNew(name, 32, true);
    ids[TY_I32] = symbolTableAddType(syms, ty);
}

void astInitProgram(ASTProgram *prog) {
    prog->entry_point = NULL;
    prog->root_module = 0; // not an actual module id, just a known initial value.
    arrayInit(&prog->modules);
    symbolTableInit(&prog->symbols);
    init_primitives(prog->primitive_ids, &prog->symbols);
}

void astFreeProgram(ASTProgram *prog) {
    arrayMap(&prog->modules, free_module_callback, NULL);
    arrayFree(&prog->modules);
    prog->entry_point = NULL; // entry point should be in the root module, so it was already freed.
    prog->root_module = 0;
    symbolTableFree(&prog->symbols);
}

SymbolID astProgramGetPrimitiveType(ASTProgram *prog, PrimitiveType ty) {
    return prog->primitive_ids[(u32)ty];
}

ModuleID astProgramAddModule(ASTProgram *prog, ASTModule *module) {
    return (ModuleID)arrayPush(&prog->modules, (void *)module);
}

ASTModule *astProgramGetModule(ASTProgram *prog, ModuleID module_id) {
    // arrayGet() handles the id being out of bounds.
    return ARRAY_GET_AS(ASTModule *, &prog->modules, module_id);
}

void astPrintProgram(FILE *to, ASTProgram *prog) {
    fprintf(to, "ASTProgram{\x1b[1mprimitive_ids:\x1b[0m [");
    const u32 primitive_ids_length = sizeof(prog->primitive_ids)/sizeof(prog->primitive_ids[0]);
    for(u32 i = 0; i < primitive_ids_length; ++i) {
        // TODO: print the primitive before its id.
        symbolIDPrint(to, prog->primitive_ids[i]);
        if(i + 1 < primitive_ids_length) {
            fputs(", ", to);
        }
    }
    fprintf(to, "], \x1b[1mroot_module:\x1b[0m ModuleID{\x1b[34m%zu\x1b[0m}", prog->root_module);
    fprintf(to, ", \x1b[1mentry_point:\x1b[0m ");
    if(prog->entry_point) {
        astPrintObj(to, AS_OBJ(prog->entry_point));
    } else {
        fputs("(null)", to);
    }
    fprintf(to, ", \x1b[1mmodules:\x1b[0m [");
    for(usize i = 0; i < prog->modules.used; ++i) {
        astPrintModule(to, ARRAY_GET_AS(ASTModule *, &prog->modules, i));
        if(i + 1 < prog->modules.used) {
            fputs(", ", to);
        }
    }
    fprintf(to, "], \x1b[1msymbols:\x1b[0m ");
    symbolTablePrint(to, &prog->symbols);
    fputc('}', to);
}


ASTNode *astNewIdentifierNode(SymbolID data_type, ASTIdentifier *id) {
    ASTIdentifierNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = ND_IDENTIFIER,
        .location = id->location
    };
    n->data_type = data_type;
    n->id = id;
    return AS_NODE(n);
}

ASTNode *astNewNumberNode(Location loc, NumberConstant value) {
    ASTNumberNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = ND_NUMBER,
        .location = loc
    };
    n->value = value;
    return AS_NODE(n);
}

ASTNode *astNewUnaryNode(ASTNodeType type, Location loc, ASTNode *operand) {
    ASTUnaryNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->operand = operand;
    return AS_NODE(n);
}

ASTNode *astNewBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right) {
    ASTBinaryNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->left = left;
    n->right = right;
    return AS_NODE(n);
}

ASTNode *astNewListNode(ASTNodeType type, Location loc, Array body) {
    ASTListNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->body = body;
    return AS_NODE(n);
}

ASTNode *astNewConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTListNode *body, ASTNode *else_) {
    ASTConditionalNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->condition = condition;
    n->body = body;
    n->else_ = else_;
    return AS_NODE(n);
}

ASTNode *astNewLoopNode(ASTNodeType type, Location loc, ASTNode *initializer, ASTNode *condition, ASTNode *increment, ASTListNode *body) {
    ASTLoopNode *n;
    NEW0(n);
    n->header = (ASTNode){
        .type = type,
        .location = loc
    };
    n->initializer = initializer;
    n->condition = condition;
    n->increment = increment;
    n->body = body;
    return AS_NODE(n);
}


static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_ADD]        = "ND_ADD",
        [ND_SUB]        = "ND_SUB",
        [ND_MUL]        = "ND_MUL",
        [ND_DIV]        = "ND_DIV",
        [ND_NEG]        = "ND_NEG",
        [ND_EQ]         = "ND_EQ",
        [ND_NE]         = "ND_NE",
        [ND_CALL]       = "ND_CALL",
        [ND_ASSIGN]     = "ND_ASSIGN",
        [ND_NUMBER]     = "ND_NUMBER",
        [ND_IDENTIFIER] = "ND_IDENTIFIER",
        [ND_EXPR_STMT]  = "ND_EXPR_STMT",
        [ND_IF]         = "ND_IF",
        [ND_BLOCK]      = "ND_BLOCK",
        [ND_LOOP]       = "ND_LOOP",
        [ND_RETURN]     = "ND_RETURN"
    };
    return names[(i32)type];
}

static const char *node_name(ASTNodeType type) {
    switch(type) {
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_EQ:
        case ND_NE:
        case ND_ASSIGN:
            return "ASTBinaryNode";
        // unary node
        case ND_NEG:
        case ND_EXPR_STMT:
        case ND_RETURN:
        case ND_CALL:
            return "ASTUnaryNode";
        // conditional nodes
        case ND_IF:
            return "ASTConditionalNode";
        // list nodes
        case ND_BLOCK:
            return "ASTListNode";
        // loop nodes
        case ND_LOOP:
            return "ASTLoopNode";
        // other nodes
        case ND_NUMBER:
            return "ASTNumberNode";
        case ND_IDENTIFIER:
            return "ASTIdentifierNode";
        default:
            break;
    }
    UNREACHABLE();
}

void astPrint(FILE *to, ASTNode *node) {
    if(!node) {
        fprintf(to, "(null)");
        return;
    }
    fprintf(to, "%s{\x1b[1mtype:\x1b[0;33m %s\x1b[0m", node_name(node->type), node_type_name(node->type));
    switch(node->type) {
        // other nodes
        case ND_NUMBER:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            printNumberConstant(to, AS_NUMBER_NODE(node)->value);
            break;
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_EQ:
        case ND_NE:
        case ND_ASSIGN:
            fprintf(to, ", \x1b[1mleft:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->left);
            fprintf(to, ", \x1b[1mright:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
        case ND_EXPR_STMT:
        case ND_RETURN:
        case ND_CALL:
            fprintf(to, ", \x1b[1moperand:\x1b[0m ");
            astPrint(to, AS_UNARY_NODE(node)->operand);
            break;
        // conditional nodes
        case ND_IF:
            fprintf(to, ", \x1b[1mcondition:\x1b[0m ");
            astPrint(to, AS_CONDITIONAL_NODE(node)->condition);
            fprintf(to, ", \x1b[1mbody:\x1b[0m ");
            astPrint(to, AS_NODE(AS_CONDITIONAL_NODE(node)->body));
            fprintf(to, ", \x1b[1melse:\x1b[0m ");
            astPrint(to, AS_CONDITIONAL_NODE(node)->else_);
            break;
        // list nodes
        case ND_BLOCK:
            fprintf(to, ", \x1b[1mbody:\x1b[0m ");
            if(AS_LIST_NODE(node)->body.used == 0) {
                fputs("(empty)", to);
            }
            for(usize i = 0; i < AS_LIST_NODE(node)->body.used; ++i) {
                astPrint(to, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(node)->body, i));
		        if(i + 1 < AS_LIST_NODE(node)->body.used) {
			        fputs(", ", to);
		        }
            }
            break;
        // loop nodes
        case ND_LOOP:
            fprintf(to, ", \x1b[1minitializer:\x1b[0m ");
            astPrint(to, AS_LOOP_NODE(node)->initializer);
            fprintf(to, ", \x1b[1mcondition:\x1b[0m ");
            astPrint(to, AS_LOOP_NODE(node)->condition);
            fprintf(to, ", \x1b[1mincrement:\x1b[0m ");
            astPrint(to, AS_LOOP_NODE(node)->increment);
            fprintf(to, ", \x1b[1mbody:\x1b[0m ");
            astPrint(to, AS_NODE(AS_LOOP_NODE(node)->body));
            break;
        // identifier nodes
        case ND_IDENTIFIER:
            fprintf(to, ", \x1b[1mdata_type:\x1b[0m ");
            symbolIDPrint(to, AS_IDENTIFIER_NODE(node)->data_type);
            fprintf(to, ", \x1b[1mid:\x1b[0m ");
            astPrintIdentifier(to, AS_IDENTIFIER_NODE(node)->id);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

void astFree(ASTNode *node) {
    if(!node) {
        return;
    }
    switch(node->type) {
        // binary nodes
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_EQ:
        case ND_NE:
        case ND_ASSIGN:
            astFree(AS_BINARY_NODE(node)->left);
            astFree(AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
        case ND_EXPR_STMT:
        case ND_RETURN:
        case ND_CALL:
            astFree(AS_UNARY_NODE(node)->operand);
            break;
        // conditional nodes
        case ND_IF:
            astFree(AS_CONDITIONAL_NODE(node)->condition);
            astFree(AS_NODE(AS_CONDITIONAL_NODE(node)->body));
            astFree(AS_CONDITIONAL_NODE(node)->else_);
            break;
        // list nodes
        case ND_BLOCK:
            for(usize i = 0; i < AS_LIST_NODE(node)->body.used; ++i) {
                astFree(ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(node)->body, i));
            }
            arrayFree(&AS_LIST_NODE(node)->body);
            break;
        // loop nodes
        case ND_LOOP:
            astFree(AS_LOOP_NODE(node)->initializer);
            astFree(AS_LOOP_NODE(node)->condition);
            astFree(AS_LOOP_NODE(node)->increment);
            astFree(AS_NODE(AS_LOOP_NODE(node)->body));
            break;
        // identifier nodes
        case ND_IDENTIFIER:
            astFreeIdentifier(AS_IDENTIFIER_NODE(node)->id);
            break;
        default:
            break;
    }
    FREE(node);
}
