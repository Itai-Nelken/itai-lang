#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"
#include "Scanner.h"

typedef struct test {
    const char *name;
    bool (*fn)();
} Test;

static const char *failed_expr = NULL;
#define CHECK(expr) do { if(!(expr)) { failed_expr = #expr; return false; } } while(0)

void print(Test *t, bool success) {
    printf("%s: ", t->name);
    if(!success) {
        printf("'%s': ", failed_expr);
        failed_expr = NULL;
    }
    printf("%s\x1b[0m\n", success ? "\x1b[1;32mPassed" : "\x1b[1;31mFailed");
}

static bool test_strings() {
    char *s1 = newString(5);
    stringAppend(s1, "Hello, %s!", "World");
    CHECK(stringIsValid(s1));
    CHECK(!strcmp(s1, "Hello, World!"));
    CHECK(stringEqual(s1, "Hello, World!"));

    // stringDUplicate() & stringCopy() use stringNCopy(), so it's also tested.
    char *s2 = stringDuplicate(s1);
    char *s3 = stringCopy("Hello, World!");
    CHECK(stringEqual(s2, s3));

    freeString(s2);
    freeString(s3);
    freeString(s1);
    return true;
}

static void test_array_callback(void *item, void *cl) {
    static long i = 1;
    bool *ok = (bool *)cl;
    if(!*ok) {
        return;
    }
    if(!(((long)item) == i)) {
        *ok = false;
        failed_expr = "(long)item == i";
    }
    ++i;
}
static bool test_array() {
    long expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const size_t expected_size = sizeof(expected) / sizeof(expected[0]);
    bool ok = true;
    Array a;
    initArray(&a);

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        arrayPush(&a, (void *)expected[i]);
    }

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        CHECK(((long)arrayGet(&a, i)) == expected[i]);
    }

    Array copy;
    initArray(&copy);
    arrayCopy(&copy, &a);
    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        CHECK(((long)arrayPop(&copy)) == expected[expected_size-1 - i]);
    }

    // can't use CHECK() here.
    // instead the callback won't run if ok == false.
    arrayMap(&a, test_array_callback, (void *)&ok);

    freeArray(&copy);
    freeArray(&a);
    return ok;
}

struct table_test_expected {
    const char *s;
    long i;
};
struct table_test {
    struct table_test_expected *expected;
    size_t expected_length;
    bool *ok;
};
static long find_value_for_key(struct table_test *t, char *key) {
    for(int i = 0; i < (int)t->expected_length; ++i) {
        if(t->expected[i].s[0] == key[0]) {
            return t->expected[i].i;
        }
    }
    return -1;
}
static void test_table_callback(TableItem *item, void *cl) {
    struct table_test *t = (struct table_test *)cl;
    if(!t->ok) {
        return;
    }
    if(!(((long)item->value) == find_value_for_key(t, (char *)item->key))) {
        t->ok = false;
        failed_expr = "(long)item->value == find_value_for_key(t, (char *)item->key)";
        return;
    }
}
static bool test_table() {
    struct table_test_expected expected[] = {
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


    CHECK(((long)(tableGet(&t, (void *)"k")->value)) == 11l);
    tableDelete(&t, (void *)"k");
    CHECK(tableGet(&t, (void *)"k") == NULL);

    struct table_test tt = {
        .expected = expected,
        .expected_length = sizeof(expected)/sizeof(expected[0]),
        .ok = &ok
    };
    // can't use CHECK() here.
    // instead the callback won't run if ok == false.
    tableMap(&t, test_table_callback, (void *)&tt);

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

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return true;
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

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return true;
}
static bool test_scanner_1_2_3char() {
    TokenType expected[] = {
        TK_GREATER, TK_GREATER_EQUAL, TK_RSHIFT, TK_RSHIFT_EQUAL,
        TK_LESS, TK_LESS_EQUAL, TK_LSHIFT, TK_LSHIFT_EQUAL,
        TK_ELIPSIS, TK_DOT, // elipsis before dot becase '. ...' => TK_ELIPSIS, TK_DOT
    };
    Scanner s;
    initScanner(&s, "Test (1, 2 or 3 char)", "> >= >> >>= < <= << <<= ... .");

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return true;
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

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        switch(tk.type) {
            case TK_STRLIT:
                CHECK(strncmp(tk.lexeme, "\"string\"", tk.length) == 0);
                break;
            case TK_CHARLIT:
                // 3 == ' <char> '
                CHECK(tk.length == 3);
                CHECK(tk.lexeme[1] == 'c');
                break;
            case TK_NUMLIT:
                CHECK(strtol(tk.lexeme, NULL, 10) == 123l);
                break;
            case TK_FLOATLIT:
                CHECK(strtod(tk.lexeme, NULL) == 1.23);
                break;
            case TK_IDENTIFIER:
                CHECK(strncmp(tk.lexeme, "identifier", tk.length) == 0);
                break;
            default:
                UNREACHABLE();
        }
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return true;
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

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
    return true;
}

// TODO: Symbols, Parser
Test tests[] = {
    {"Strings", test_strings},
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
