#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Array.h"
#include "Strings.h"
#include "Token.h"
#include "Error.h"
#include "Compiler.h"
#include "Scanner.h"
#include "Parser.h"

void parserInit(Parser *p, Compiler *c) {
    p->compiler = c;
    p->tokens = NULL;
    p->current_token = 0;
}

void parserFree(Parser *p) {
    p->compiler = NULL;
    p->tokens = NULL;
    p->current_token = 0;
}

ASTNode *newNumberNode(Location loc, NumberConstant value) {
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

ASTNode *newBinaryNode(ASTNodeType type, Location loc, ASTNode *left, ASTNode *right) {
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

static const char *node_type_name(ASTNodeType type) {
    static const char *names[] = {
        [ND_ADD]    = "ND_ADD",
        [ND_SUB]    = "ND_SUB",
        [ND_MUL]    = "ND_MUL",
        [ND_DIV]    = "ND_DIV",
        [ND_NEG]    = "ND_NEG",
        [ND_NUMBER] = "ND_NUMBER"
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
            return "ASTBinaryNode";
        // unary node
        case ND_NEG:
            return "ASTUnaryNode";
        // other nodes
        case ND_NUMBER:
            return "ASTNumberNode";
        default:
            break;
    }
    UNREACHABLE();
}

void astPrint(FILE *to, ASTNode *node) {
    fprintf(to, "%s{\x1b[1mtype:\x1b[0;33m %s\x1b[0m", node_name(node->type), node_type_name(node->type));
    switch(node->type) {
        // other nodes
        case ND_NUMBER:
            fprintf(to, ", \x1b[1mvalue:\x1b[0m ");
            printNumberConstant(to, AS_NUMBER_NODE(node)->value);
            break;
        // binary nodes
        case ND_ADD:
        case ND_MUL:
            fprintf(to, ", \x1b[1mleft:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->left);
            fprintf(to, ", \x1b[1mright:\x1b[0m ");
            astPrint(to, AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
            fprintf(to, ", \x1b[1moperand:\x1b[0m ");
            astPrint(to, AS_UNARY_NODE(node)->operand);
            break;
        default:
            UNREACHABLE();
    }
    fputc('}', to);
}

void astFree(ASTNode *node) {
    switch(node->type) {
        // binary nodes
        case ND_ADD:
        case ND_MUL:
            astFree(AS_BINARY_NODE(node)->left);
            astFree(AS_BINARY_NODE(node)->right);
            break;
        // unary nodes
        case ND_NEG:
            astFree(AS_UNARY_NODE(node)->operand);
            break;
        default:
            break;
    }
    FREE(node);
}

static inline bool is_eof(Parser *p) {
    return ARRAY_GET_AS(Token *, p->tokens, p->current_token)->type == TK_EOF;
}

static inline Token *advance(Parser *p) {
    return is_eof(p) ? NULL : ARRAY_GET_AS(Token *, p->tokens, p->current_token++);
}

static inline Token *next(Parser *p) {
    return p->current_token + 1 > p->tokens->used ? NULL : ARRAY_GET_AS(Token *, p->tokens, p->current_token);
}

static inline Token *previous(Parser *p) {
    return p->current_token == 0 ? NULL : ARRAY_GET_AS(Token *, p->tokens, p->current_token - 1);
}

// if a valid String is provided as message, it will be freed.
static void error(Parser *p, char *message) {
    Error *err;
    NEW0(err);
    errorInit(err, ERR_ERROR, previous(p)->location, message);
    compilerAddError(p->compiler, err);
    if(stringIsValid(message)) {
        stringFree(message);
    }
}

typedef struct binding_power {
    u8 left, right;
} BP;

static u8 prefix_binding_power(TokenType op) {
    u8 r_bp = 0;
    switch(op) {
        case TK_PLUS:
        case TK_MINUS:
            r_bp = 5;
            break;
        default:
            UNREACHABLE();
    }
    return r_bp;
}

static BP infix_binding_power(TokenType op) {
    BP bp = {0, 0};
    switch(op) {
        case TK_PLUS:
        case TK_MINUS:
            bp.left = 1;
            bp.right = 2;
            break;
        case TK_STAR:
        case TK_SLASH:
            bp.left = 3;
            bp.right = 4;
            break;
        default:
            UNREACHABLE();
    }
    return bp;
}

static ASTNodeType infix_token_to_node_type(TokenType type) {
    switch(type) {
        case TK_PLUS:
            return ND_ADD;
        case TK_MINUS:
            return ND_SUB;
        case TK_STAR:
            return ND_MUL;
        case TK_SLASH:
            return ND_DIV;
        case TK_NUMBER:
            return ND_NUMBER;
        case TK_LPAREN:
        case TK_RPAREN:
        case TK_EOF:
            break;
    }
    UNREACHABLE();
}

static ASTNodeType prefix_token_to_node_type(TokenType type) {
    switch(type) {
        // No TK_PLUS here because it doesn't actually do anything.
        // it is handled in primary() before calling this function.
        case TK_MINUS:
            return ND_NEG;
        default:
            break;
    }
    UNREACHABLE();
}

// pre-declarations
static ASTNode *expr_bp(Parser *p, u8 min_bp);

static ASTNode *primary(Parser *p) {
    bool was_eof = false;
    if(is_eof(p)) {
        was_eof = true;
        goto end;
    }
    switch(advance(p)->type) {
        case TK_NUMBER:
            return newNumberNode(previous(p)->location, AS_NUMBER_CONSTANT_TOKEN(previous(p))->value);
        case TK_PLUS:
        case TK_MINUS: {
            u8 r_bp = prefix_binding_power(previous(p)->type);
            Token *operator = previous(p);
            ASTNode *operand = expr_bp(p, r_bp);
            if(!operand) {
                return NULL;
            }
            if(operator->type == TK_PLUS) {
                return operand;
            }
            return astNewUnaryNode(prefix_token_to_node_type(operator->type), operator->location, operand);
            break;
        }
        default:
            break;
    }
end:
    error(p, stringFormat("Expected one of [<number>, '+', '-'] but got '%s'!", (was_eof ? "<eof>" : tokenTypeString(previous(p)->type))));
    return NULL;
}

static Token *operator(Parser *p) {
    switch(next(p)->type) {
        case TK_PLUS:
        case TK_STAR:
            return next(p);
        default:
            break;
    }
    
    error(p, "Expected ['+', '*']!");
    return NULL;
}

// expression parser based on the example
// here: https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
static ASTNode *expr_bp(Parser *p, u8 min_bp) {
    ASTNode *lhs = primary(p);
    if(!lhs) {
        return NULL;
    }

    while(!is_eof(p)) {
        // can't consume the operator here,
        // as if its left binding power is smaller
        // than min_bp, we won't do anything with it,
        // but it will be needed later so we can't consume it.
        Token *op = operator(p);
        if(!op) {
            return NULL;
        }
        
        BP bp = infix_binding_power(op->type);
        if(bp.left < min_bp) {
            break;
        }
        // consume the operator.
        advance(p);

        ASTNode *rhs = expr_bp(p, bp.right);
        if(!rhs) {
            return NULL;
        }

        lhs = newBinaryNode(infix_token_to_node_type(op->type), op->location, lhs, rhs);
    }
    return lhs;
}

static ASTNode *expression(Parser *p) {
    return expr_bp(p, 0);
}

ASTNode *parserParse(Parser *p, Array *tokens) {
    p->tokens = tokens;
    ASTNode *expr = expression(p);
    p->tokens = NULL;
    p->current_token = 0;
    return expr;
}

