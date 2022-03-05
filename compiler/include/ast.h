#ifndef AST_H
#define AST_H

#include "types.h"

typedef enum ast_type {
    ND_I8, ND_I16, ND_I32, ND_I64, ND_I128,
    ND_U8, ND_U16, ND_U32, ND_U64, ND_U128,
    ND_CHAR, ND_STR
} ASTNodeType;

typedef struct ast_node {
    ASTNodeType type;
    struct ast_node *left, *right;
    union {
        i8 int8;
        i16 int16;
        i32 int32;
        i64 int64;
        i128 int128;

        u8 uint8;
        u16 uint16;
        u32 uint32;
        u64 uint64;
        u128 uint128;

        f32 float32;
        f64 float64;

        char character;
        str *string;
    } literal;
} ASTNode;

ASTNode *newNode(ASTNodeType type, ASTNode *left, ASTNode *right);

void freeAST(ASTNode *root);

#endif // AST_H
