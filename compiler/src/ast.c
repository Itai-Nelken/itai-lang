#include <stdlib.h>
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Array.h"
#include "Symbols.h"
#include "Token.h"
#include "ast.h"

/*** callbacks ***/
static void free_ast_callback(void *node, void *cl) {
    UNUSED(cl);
    freeAST((ASTNode *)node);
}

static void free_function_callback(void *fn, void *cl) {
    UNUSED(cl);
    freeFunction((ASTFunction *)fn);
}

static void free_identifier_callback(void *identifier, void *cl) {
    UNUSED(cl);
    freeIdentifier((ASTIdentifier *)identifier);
}

ASTIdentifier *newIdentifier(char *str, int length) {
    ASTIdentifier *identifier;
    NEW0(identifier);
    identifier->text = stringNCopy(str, length);
    identifier->length = length;
    return identifier;
}

void freeIdentifier(ASTIdentifier *identifier) {
    freeString(identifier->text);
    identifier->text = NULL;
    identifier->length = 0;
    FREE(identifier);
}

ASTFunction *newFunction(ASTIdentifierNode *name, Type return_type) {
    assert(name);
    ASTFunction *fn;
    NEW0(fn);
    fn->name = name;
    fn->return_type = return_type;
    initSymTable(&fn->identifiers, SYM_LOCAL, free_identifier_callback, NULL);
    initArray(&fn->locals);
    fn->body = NULL;
    return fn;
}

void freeFunction(ASTFunction *fn) {
    freeAST(AS_NODE(fn->name));
    freeSymTable(&fn->identifiers);
    arrayMap(&fn->locals, free_ast_callback, NULL);
    freeArray(&fn->locals);
    if(fn->body) {
        freeAST(AS_NODE(fn->body));
    }
    FREE(fn);
}

void initASTProg(ASTProg *astp) {
    initSymTable(&astp->identifiers, SYM_GLOBAL, free_identifier_callback, NULL);
    initArray(&astp->types);
    initArray(&astp->globals);
    initArray(&astp->functions);
}

void freeASTProg(ASTProg *astp) {
    freeSymTable(&astp->identifiers);
    freeArray(&astp->types);
    arrayMap(&astp->globals, free_ast_callback, NULL);
    freeArray(&astp->globals);
    arrayMap(&astp->functions, free_function_callback, NULL);
    freeArray(&astp->functions);
}

ASTNode newNode(ASTNodeType type, Location loc) {
    ASTNode n = {type, loc};
    return n;
} 

// TODO: single newNode(type, loc, ...) function for
//        new nodes that decides the node's type according
//        to the node type.
ASTNode *newNumberNode(i32 value, Location loc) {
    ASTLiteralNode *n;
    NEW0(n);
    n->header = newNode(ND_NUM, loc);
    n->as.int32 = value;
    return AS_NODE(n);
}

ASTNode *newUnaryNode(ASTNodeType type, Location loc, ASTNode *child) {
    ASTUnaryNode *n;
    NEW0(n);
    n->header = newNode(type, loc);
    n->child = child;
    return AS_NODE(n);
}

ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right) {
    ASTBinaryNode *n;
    NEW0(n);
    n->header = newNode(type, loc);
    n->left = left;
    n->right = right;
    return AS_NODE(n);
}

ASTNode *newIdentifierNode(Location loc, int id, Type ty) {
    ASTIdentifierNode *n;
    NEW0(n);
    n->header = newNode(ND_IDENTIFIER, loc);
    n->id = id;
    n->type = ty;
    return AS_NODE(n);
}

ASTNode *newBlockNode(Location loc) {
    ASTBlockNode *n;
    NEW0(n);
    n->header = newNode(ND_BLOCK, loc);
    initArray(&n->body);
    return AS_NODE(n);
}

ASTNode *newConditionalNode(ASTNodeType type, Location loc, ASTNode *condition, ASTNode *body, ASTNode *else_) {
    ASTConditionalNode *n;
    NEW0(n);
    n->header = newNode(type, loc);
    n->condition = condition;
    n->body = body;
    n->else_ = else_;
    return AS_NODE(n);
}

ASTNode *newLoopNode(Location loc, ASTNode *init, ASTNode *condition, ASTNode *increment, ASTNode *body) {
    ASTLoopNode *n;
    NEW0(n);
    n->header = newNode(ND_LOOP, loc);
    n->init = init;
    n->condition = condition;
    n->increment = increment;
    n->body = body;
    return AS_NODE(n);
}

void freeAST(ASTNode *root) {
    if(root == NULL) {
        return;
    }
    switch(root->type) {
        // unary nodes
        case ND_EXPR_STMT:
        case ND_NEG:
        case ND_RETURN:
        case ND_CALL:
            freeAST(AS_UNARY_NODE(root)->child);
            break;
        // binary nodes
        case ND_ASSIGN:
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_REM:
        case ND_EQ:
        case ND_NE:
        case ND_GT:
        case ND_GE:
        case ND_LT:
        case ND_LE:
        case ND_BIT_OR:
        case ND_XOR:
        case ND_BIT_AND:
        case ND_BIT_RSHIFT:
        case ND_BIT_LSHIFT:
            freeAST(AS_BINARY_NODE(root)->left);
            freeAST(AS_BINARY_NODE(root)->right);
            break;
        // everything else
        case ND_BLOCK:
            arrayMap(&AS_BLOCK_NODE(root)->body, free_ast_callback, NULL);
            freeArray(&AS_BLOCK_NODE(root)->body);
            break;
        case ND_IF:
            freeAST(AS_CONDITIONAL_NODE(root)->condition);
            freeAST(AS_CONDITIONAL_NODE(root)->body);
            freeAST(AS_CONDITIONAL_NODE(root)->else_);
            break;
        case ND_LOOP:
            freeAST(AS_LOOP_NODE(root)->init);
            freeAST(AS_LOOP_NODE(root)->condition);
            freeAST(AS_LOOP_NODE(root)->increment);
            freeAST(AS_LOOP_NODE(root)->body);
            break;
        case ND_IDENTIFIER:
        case ND_NUM:
            // nothing
            break;
        default:
            break;
    }
    FREE(root);
}

static const char *ast_node_type_str[] = {
    [ND_LOOP] = "ND_LOOP",
    [ND_IF] = "ND_IF",
    [ND_CALL] = "ND_CALL",
    [ND_RETURN] = "ND_RETURN",
    [ND_BLOCK] = "ND_BLOCK",
    [ND_IDENTIFIER] = "ND_IDENTIFIER",
    [ND_ASSIGN] = "ND_ASSIGN",
    [ND_EXPR_STMT] = "ND_EXPR_STMT",
    [ND_ADD] = "ND_ADD",
    [ND_SUB] = "ND_SUB",
    [ND_MUL] = "ND_MUL",
    [ND_DIV] = "ND_DIV",
    [ND_REM] = "ND_REM",
    [ND_EQ] = "ND_EQ",
    [ND_NE] = "ND_NE",
    [ND_GT] = "ND_GT",
    [ND_GE] = "ND_GE",
    [ND_LT] = "ND_LT",
    [ND_LE] = "ND_LE",
    [ND_BIT_OR] = "ND_BIT_OR",
    [ND_XOR] = "ND_XOR",
    [ND_BIT_AND] = "ND_BIT_AND",
    [ND_BIT_RSHIFT] = "ND_RSHIFT",
    [ND_BIT_LSHIFT] = "ND_LSHIFT",
    [ND_NEG] = "ND_NEG",
    [ND_NUM] = "ND_NUM"
};

static const char *node_name(ASTNodeType type) {
    switch(type) {
        // unary nodes
        case ND_EXPR_STMT:
        case ND_NEG:
        case ND_RETURN:
        case ND_CALL:
            return "ASTUnaryNode";
        // binary nodes
        case ND_ASSIGN:
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_REM:
        case ND_EQ:
        case ND_NE:
        case ND_GT:
        case ND_GE:
        case ND_LT:
        case ND_LE:
        case ND_BIT_OR:
        case ND_XOR:
        case ND_BIT_AND:
        case ND_BIT_RSHIFT:
        case ND_BIT_LSHIFT:
            return "ASTBinaryNode";
        // everything else
        case ND_BLOCK:
            return "ASTBlockNode";
        case ND_IDENTIFIER:
            return "ASTIdentifierNode";
        case ND_NUM:
            return "ASTLiteralNode";
        case ND_IF:
            return "ASTConditionalNode";
        case ND_LOOP:
            return "ASTLoopNode";
        default:
            break;
    }
    UNREACHABLE();
}

void printAST(ASTNode *root) {
#define TYPE(type) (ast_node_type_str[type])
    if(root == NULL) {
        return;
    }

    printf("%s {", node_name(root->type));
    printf("\x1b[1mtype:\x1b[0;33m %s\x1b[0m, ", TYPE(root->type));
    switch(root->type) {
        // unary nodes
        case ND_EXPR_STMT:
        case ND_NEG:
        case ND_RETURN:
        case ND_CALL:
            printf("\x1b[1mchild:\x1b[0m ");
            printAST(AS_UNARY_NODE(root)->child);
            putchar(' ');
            break;
        // binary nodes
        case ND_ASSIGN:
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_REM:
        case ND_EQ:
        case ND_NE:
        case ND_GT:
        case ND_GE:
        case ND_LT:
        case ND_LE:
        case ND_BIT_OR:
        case ND_XOR:
        case ND_BIT_AND:
        case ND_BIT_RSHIFT:
        case ND_BIT_LSHIFT:
            printf("\x1b[1mleft:\x1b[0m ");
            printAST(AS_BINARY_NODE(root)->left);
            printf(", \x1b[1mright:\x1b[0m ");
            printAST(AS_BINARY_NODE(root)->right);
            putchar(' ');
            break;
        // everything else
        case ND_BLOCK:
            printf("\x1b[1mbody:\x1b[0m [");
            for(size_t i = 0; i < AS_BLOCK_NODE(root)->body.used; ++i) {
                printAST(ARRAY_GET_AS(ASTNode *, &AS_BLOCK_NODE(root)->body, i));
		// if not last element, print a comma and a space.
		if(i + 1 < AS_BLOCK_NODE(root)->body.used) {
			printf(", ");
		}
            }
            printf(" ] ");
            break;
        case ND_IDENTIFIER:
            printf("\x1b[1mid:\x1b[0;36m %d\x1b[0m, ", AS_IDENTIFIER_NODE(root)->id);
            printf("\x1b[1mtype:\x1b[0m {");
            printType(AS_IDENTIFIER_NODE(root)->type);
            putchar('}');
            break;
        case ND_NUM:
            // FIXME: handle all literals
            printf("\x1b[1mas.int32:\x1b[0;36m %d\x1b[0m", AS_LITERAL_NODE(root)->as.int32);
            break;
        case ND_IF:
            printf("\x1b[1mcondition:\x1b[0m ");
            printAST(AS_CONDITIONAL_NODE(root)->condition);
            printf(", \x1b[1mbody:\x1b[0m ");
            printAST(AS_CONDITIONAL_NODE(root)->body);
            putchar(' ');
            printf("\x1b[1melse_:\x1b[0m ");
            printAST(AS_CONDITIONAL_NODE(root)->else_);
            putchar(' ');
            break;
        case ND_LOOP:
            printf("\x1b[1minit:\x1b[0m ");
            if(!AS_LOOP_NODE(root)->init) {
                printf("(null)");
            } else {
                printAST(AS_LOOP_NODE(root)->init);
            }
            printf(", \x1b[1mcondition:\x1b[0m ");
            if(!AS_LOOP_NODE(root)->condition) {
                printf("(null)");
            } else {
                printAST(AS_LOOP_NODE(root)->condition);
            }
            printf(", \x1b[1mincrement:\x1b[0m ");
            if(!AS_LOOP_NODE(root)->increment) {
                printf("(null)");
            } else {
                printAST(AS_LOOP_NODE(root)->increment);
            }
            printf(", \x1b[1mbody:\x1b[0m ");
            if(!AS_LOOP_NODE(root)->body) {
                printf("(null)");
            } else {
                printAST(AS_LOOP_NODE(root)->body);
            }
            putchar(' ');
            break;
        default:
            UNREACHABLE();
            break;
    }

    putchar('}');

#undef TYPE
}
