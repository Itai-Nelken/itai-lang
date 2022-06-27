#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"
#include "Array.h"
#include "Table.h"
#include "Scanner.h"

typedef struct test {
    const char *name;
    bool (*fn)();
} Test;

void print(Test *t, bool success) {
    const char *result = success ? "\x1b[1;32mPassed\x1b[0m" : "\x1b[1;31mFailed\x1b[0m";
    printf("%s: %s\n", t->name, result);
}

static void test_array_callback(void *item, void *cl) {
    static long i = 1;
    bool *ok = (bool *)cl;
    *ok = ((long)item) == i;
    ++i;
}
static bool test_array() {
    long expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    bool ok = true;
    Array a;
    initArray(&a);

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        arrayPush(&a, (void *)expected[i]);
    }

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        ok = ((long)arrayGet(&a, i)) == expected[i];
    }

    Array copy;
    initArray(&copy);
    arrayCopy(&copy, &a);
    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        ok = ((long)arrayPop(&copy)) == expected[sizeof(expected)/sizeof(expected[0]) - i];
    }

    arrayMap(&a, test_array_callback, (void *)&ok);

    freeArray(&copy);
    freeArray(&a);
    return ok;
}

static void test_table_callback(TableItem *item, void *cl) {
    static long i = 1;
    static char s = 'a';
    bool *ok = (bool *)cl;
    *ok = ((long)item->value) == i;
    *ok = ((char *)item->key)[0] == s;
    i++;
    s++;
}
static bool test_table() {
    struct {const char *s; long i;} expected[] = {
        {"a", 1},
        {"b", 2},
        {"c", 3},
        {"d", 4},
        {"e", 5},
        {"f", 6},
        {"g", 7},
        {"h", 8},
        {"i", 9},
        {"j", 10},
        {"k", 11},
        {"l", 12},
        {"m", 13},
        {"n", 14},
        {"o", 15},
        {"p", 16},
        {"q", 17}
    };
    bool ok = true;
    Table t;
    initTable(&t, NULL, NULL);

    for(int i = 0; i < (int)(sizeof(expected)/sizeof(expected[0])); ++i) {
        tableSet(&t, (void *)expected[i].s, (void *)expected[i].i);
    }

    tableMap(&t, test_table_callback, (void *)&ok);

    ok = ((long)tableGet(&t, (void *)"k")) == 11l;
    tableDelete(&t, (void *)"k");
    ok = tableGet(&t, (void *)"k") == NULL;

    freeTable(&t);
    return ok;
}

static bool test_scanner_1char() {
    TokenType expected[] = {
        TK_LPAREN, TK_RPAREN,
        TK_LBRACKET, TK_RBRACKET,
        TK_LBRACE, TK_RBRACE,
        TK_COMMA, TK_SEMICOLON, TK_COLON, TK_TILDE
    };
    Scanner s;
    initScanner(&s, "Test (1 char)", "()[]{},;:~");
    bool ok = true;

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        if(tk.type == TK_EOF || tk.type == TK_ERROR) {
            ok = false;
            break;
        }
        if(!(ok = tk.type == expected[idx])) {
            break;
        }
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return ok;
}

static bool test_scanner_1_2char() {
    TokenType expected[] = {
        TK_MINUS, TK_MINUS_EQUAL, TK_ARROW,
        TK_PLUS, TK_PLUS_EQUAL,
        TK_SLASH_EQUAL, TK_SLASH,
        TK_STAR, TK_STAR_EQUAL,
        TK_BANG, TK_BANG_EQUAL,
        TK_EQUAL_EQUAL, TK_BIG_ARROW, TK_EQUAL,
        TK_PERCENT, TK_PERCENT_EQUAL,
        TK_XOR, TK_XOR_EQUAL,
        TK_PIPE, TK_PIPE_EQUAL,
        TK_AMPERSAND, TK_AMPERSAND_EQUAL
    };
    Scanner s;
    // the order of the symbols matters.
    // for example: = ==
    // will be: TK_EQUAL_EQUAL, TK_EQUAL
    // instead of the other way round.
    initScanner(&s, "Test (1 or 2 char)", "- -= -> + += /= / * *= ! != == => = % %= ^ ^= | |= & &=");
    bool ok = true;

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        if(tk.type == TK_EOF || tk.type == TK_ERROR) {
            ok = false;
            break;
        }
        if(!(ok = tk.type == expected[idx])) {
            break;
        }
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return ok;
}

static bool test_scanner_1_2_3char() {
    TokenType expected[] = {
        TK_GREATER, TK_GREATER_EQUAL, TK_RSHIFT, TK_RSHIFT_EQUAL,
        TK_LESS, TK_LESS_EQUAL, TK_LSHIFT, TK_LSHIFT_EQUAL,
        TK_ELIPSIS, TK_DOT, // elipsis before dot becase '. ...' => TK_ELIPSIS, TK_DOT
    };
    Scanner s;
    initScanner(&s, "Test (1, 2 or 3 char)", "> >= >> >>= < <= << <<= ... .");
    bool ok = true;

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        if(tk.type == TK_EOF || tk.type == TK_ERROR) {
            ok = false;
            break;
        }
        if(!(ok = tk.type == expected[idx])) {
            break;
        }
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return ok;
}

static bool test_scanner_literals() {
    TokenType expected[] = {
        TK_STRLIT,
        TK_CHARLIT,
        TK_NUMLIT,
        TK_FLOATLIT,
        TK_IDENTIFIER
    };
    Scanner s;
    // TODO: hex, binary, and octal number literals
    initScanner(&s, "Test (literals)", "\"string\" 'c' 123 1.23 identifier");
    bool ok = true;

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        if(tk.type == TK_EOF || tk.type == TK_ERROR) {
            ok = false;
            break;
        }
        if(!(ok = tk.type == expected[idx])) {
            break;
        }
        switch(tk.type) {
            case TK_STRLIT:
                ok = strncmp(tk.lexeme, "\"string\"", tk.length) == 0;
                break;
            case TK_CHARLIT:
                // 3 == ' <char> '
                if(tk.length != 3) {
                    ok = false;
                    break;
                }
                ok = tk.lexeme[1] == 'c';
                break;
            case TK_NUMLIT:
                ok = strtol(tk.lexeme, NULL, 10) == 123l;
                break;
            case TK_FLOATLIT:
                ok = strtod(tk.lexeme, NULL) == 1.23;
                break;
            case TK_IDENTIFIER:
                ok = strncmp(tk.lexeme, "identifier", tk.length) == 0;
                break;
            default:
                UNREACHABLE();
        }
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return ok;
}

static bool test_scanner_keywords() {
    TokenType expected[] = {
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
        TK_BOOL,

        // keywords
        TK_PRINT,
        TK_VAR,
        TK_CONST,
        TK_PUBLIC,
        TK_FN,
        TK_RETURN,
        TK_ENUM,    
        TK_STRUCT,
        TK_IF,
        TK_ELSE,
        TK_SWITCH,
        TK_MODULE,
        TK_IMPORT,
        TK_AS,
        TK_USING,
        TK_WHILE,
        TK_FOR,
        TK_TYPE,
        TK_NULL,
        TK_TYPEOF
    };
    Scanner s;
    initScanner(&s, "Test (literals)", "i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 isize usize char str bool print var const public fn return enum struct if else switch module import as using while for type null typeof");
    bool ok = true;

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        if(tk.type == TK_EOF || tk.type == TK_ERROR) {
            ok = false;
            break;
        }
        if(!(ok = tk.type == expected[idx])) {
            break;
        }
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return ok;
}

// TODO: Strings, Symbols, Parser
Test tests[] = {
    {"Array", test_array},
    {"Table", test_table},
    {"Scanner (1 character)", test_scanner_1char},
    {"Scanner (1 or 2 character)", test_scanner_1_2char},
    {"Scanner (1, 2 or 3 character)", test_scanner_1_2_3char},
    {"Scanner (literals)", test_scanner_literals},
    {"Scanner (keywords)", test_scanner_keywords}
};

int main(void) {
    for(int i = 0; i < (int)(sizeof(tests)/sizeof(*tests)); ++i) {
        Test *t = &tests[i];
        print(t, t->fn());
    }
}
