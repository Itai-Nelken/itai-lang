#ifndef TOKEN_H
#define TOKEN_H


typedef struct location {
    char *file;
    char *containing_line;
    int line_length;
    int line;
    int at;
} Location;

typedef enum token_type {
    // one character tokens
    TK_LPAREN, TK_RPAREN,
    TK_LBRACKET, TK_RBRACKET,
    TK_LBRACE, TK_RBRACE,
    TK_COMMA,
    TK_SEMICOLON, TK_COLON,
    TK_TILDE,

    // one or two character tokens
    TK_MINUS, TK_MINUS_EQUAL, TK_ARROW, // ->
    TK_PLUS, TK_PLUS_EQUAL,
    TK_SLASH, TK_SLASH_EQUAL,
    TK_STAR, TK_STAR_EQUAL,
    TK_BANG, TK_BANG_EQUAL,
    TK_EQUAL, TK_EQUAL_EQUAL,
    TK_PERCENT, TK_PERCENT_EQUAL,
    TK_XOR, TK_XOR_EQUAL,
    TK_PIPE, TK_PIPE_EQUAL,
    TK_AMPERSAND, TK_AMPERSAND_EQUAL,

    // one, two, or three character tokens
    TK_GREATER, TK_GREATER_EQUAL, TK_RSHIFT, TK_RSHIFT_EQUAL,
    TK_LESS, TK_LESS_EQUAL, TK_LSHIFT, TK_LSHIFT_EQUAL,
    TK_DOT, TK_ELIPSIS,

    // literals
    TK_STRLIT,
    TK_CHARLIT,
    TK_NUMLIT,
    TK_IDENTIFIER,

    // types
    TK_I8,
    TK_I16,
    TK_I32,
    TK_I64,
    TK_I128,
    TK_U8,
    TK_U16,
    TK_U32,
    TK_U64,
    TK_U128,
    TK_F32,
    TK_F64,
    TK_ISIZE,
    TK_USIZE,
    TK_CHAR,
    TK_STR,

    // keywords
    TK_VAR,
    TK_CONST,
    TK_STATIC,
    TK_FN,
    TK_RETURN,
    TK_ENUM,    
    TK_STRUCT,
    TK_IF,
    TK_ELSE,
    TK_SWITCH,
    TK_MODULE,
    TK_EXPORT,
    TK_IMPORT,
    TK_AS,
    TK_USING,
    TK_WHILE,
    TK_FOR,
    TK_TYPE,
    TK_NULL,
    TK_TYPEOF,

    // other
    TK_ERROR, TK_EOF,
    TK__COUNT
} TokenType;

typedef struct token {
    TokenType type;
    Location location;
    char *lexeme;
    int length;
} Token;

Token newToken(TokenType type, Location loc, char *lexeme, int length);
void printToken(Token t);
const char *tokenTypeToString(TokenType t);

#endif // TOKEN_H
