#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include "common.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"
#include "Token.h"
#include "Scanner.h"
#include "ast.h"
#include "Parser.h"

#define MAX_FAILURES_IN_TEST 20

typedef struct test {
    const char *name;
    void (*fn)(void *);
    void *arg;
} Test;

typedef struct test_suit {
    Test *current_test;
    int total_test_count, current_test_number;
    const char *failure_texts[MAX_FAILURES_IN_TEST];
    int failure_count;
    void (*current_end_fn)(void *);
    void *current_end_fn_arg;
} TestSuit;

static TestSuit __data;

static void _print_test_summary() {
    // current_test_number + 1 so the index starts at 1 (1/X) -> (X/X) instead of 0 (0/X) -> (X-1/X)
    printf("(%d/%d) %s: ", __data.current_test_number + 1, __data.total_test_count, __data.current_test->name);
    if(__data.failure_count > 0) {
        printf("\x1b[1;31mFailed\x1b[0m\n");
        printf("\t\x1b[1mwhat:\x1b[0m\n");
        for(int i = 0; i < __data.failure_count; ++i) {
            printf("\t  '%s'\n", __data.failure_texts[i]);
        }
        __data.failure_count = 0;
    } else {
        printf("\x1b[1;32mPassed\x1b[0m\n");
    }
}

static void _init() {
    __data.current_test = NULL;
    __data.total_test_count = __data.current_test_number = 0;
    __data.failure_count = 0;
    __data.current_end_fn = NULL;
    __data.current_end_fn_arg = NULL;
}

static void _count_tests(Test testlist[]) {
    for(; testlist[__data.total_test_count].name && testlist[__data.total_test_count].fn; ++__data.total_test_count);
}

void _check(bool result, const char *expr) {
    assert(__data.current_test);
    if(result) {
        return;
    }
    assert(__data.failure_count < MAX_FAILURES_IN_TEST);
    __data.failure_texts[__data.failure_count++] = expr;
}

void _set_end_fn(void (*fn)(void *), void *arg) {
    __data.current_end_fn = fn;
    __data.current_end_fn_arg = arg;
}

void _log(const char *format, ...) {
    va_list ap;

    printf("[test %d]: \x1b[1;33mLOG:\x1b[0m ", __data.current_test_number);
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    putchar('\n');
}

int _run_test_suit(Test testlist[]) {
    _init();
    _count_tests(testlist);
    bool had_failure = false;

    for(; __data.current_test_number < __data.total_test_count; ++__data.current_test_number) {
        __data.current_test = &testlist[__data.current_test_number];
        __data.current_test->fn(__data.current_test->arg);
        if(__data.failure_count > 0) {
            had_failure = true;
            if(__data.current_end_fn) {
                __data.current_end_fn(__data.current_end_fn_arg);
            }
        }
        __data.current_end_fn = NULL;
        __data.current_end_fn_arg = NULL;
        _print_test_summary();
    }
    return had_failure ? 1 : 0;
}

/***
 * Run all tests in 'testlist'.
 * 
 * @param testlist An array of Test structs.
 ***/
#define RUN_SUIT(testlist) (_run_test_suit((testlist)))
/***
 * Check an expression (evaluates to true or false).
 * 
 * @param expr The expression.
 ***/
#define CHECK(expr) (_check((expr), #expr))
/***
 * Set a callback that will be called if the current test fails.
 * 
 * @param fn A function pointer of type void (*)(void *).
 * @param arg An argument that will be passed to the callback. It will be casted to void *.
 ***/
#define SET_END_FN(fn, arg) (_set_end_fn((fn), (void *)(arg)))
/***
 * Print a message that will look nice with the rest of the output.
 * 
 * @param txt The message.
 ***/
#define LOG(txt) (_log((txt)))
/***
 * Print a message that will look nice with the rest of the output. printf-like formating version of LOG().
 * 
 * @param fmt The format string.
 * @param ... The format specifier arguments.
 ***/
#define LOG_F(fmt, ...) (_log((fmt), __VA_ARGS__))




struct string_test {
    char *s1, *s2, *s3;
};
static void test_strings_end(void *strings) {
    struct string_test *t = (struct string_test *)strings;
    freeString(t->s1);
    freeString(t->s2);
    freeString(t->s3);
}
static void test_strings(void *a) {
    UNUSED(a);
    struct string_test t = {NULL, NULL, NULL};
    SET_END_FN(test_strings_end, &t);
    t.s1 = newString(5);
    stringAppend(t.s1, "Hello, %s!", "World");
    CHECK(stringIsValid(t.s1));
    CHECK(!strcmp(t.s1, "Hello, World!"));
    CHECK(stringEqual(t.s1, "Hello, World!"));

    // stringDUplicate() & stringCopy() use stringNCopy(), so it's also tested.
    t.s2 = stringDuplicate(t.s1);
    t.s3 = stringCopy("Hello, World!");
    CHECK(stringEqual(t.s2, t.s3));

    freeString(t.s2);
    freeString(t.s3);
    freeString(t.s1);
}

struct array_test {
    Array a, copy;
    bool copy_used;
};
static void test_array_end(void *arrays) {
    struct array_test *t = (struct array_test *)arrays;
    freeArray(&t->a);
    if(t->copy_used) {
        freeArray(&t->copy);
    }
}
static void test_array_callback(void *item, void *cl) {
    UNUSED(cl);
    static long i = 1;
    CHECK(((long)item) == i);
    ++i;
}
static void test_array(void *a) {
    UNUSED(a);
    long expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const size_t expected_size = sizeof(expected) / sizeof(expected[0]);
    struct array_test t = {.copy_used = false};
    initArray(&t.a);
    SET_END_FN(test_array_end, &t);

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        arrayPush(&t.a, (void *)expected[i]);
    }

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        CHECK(((long)arrayGet(&t.a, i)) == expected[i]);
    }

    initArray(&t.copy);
    t.copy_used = true;
    arrayCopy(&t.copy, &t.a);
    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        CHECK(((long)arrayPop(&t.copy)) == expected[expected_size-1 - i]);
    }

    arrayMap(&t.a, test_array_callback, NULL);

    freeArray(&t.copy);
    freeArray(&t.a);
}

struct table_test_expected {
    const char *s;
    long i;
};
struct table_test {
    Table t;
    struct table_test_expected *expected;
    size_t expected_length;
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
    CHECK(((long)item->value) == find_value_for_key(t, (char *)item->key));
}
static void test_table(void *a) {
    UNUSED(a);
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
    struct table_test t = {
        .expected = expected,
        .expected_length = sizeof(expected)/sizeof(expected[0])
    };
    initTable(&t.t, NULL, NULL);

    for(int i = 0; i < (int)(sizeof(expected)/sizeof(expected[0])); ++i) {
        tableSet(&t.t, (void *)expected[i].s, (void *)expected[i].i);
    }


    CHECK(((long)(tableGet(&t.t, (void *)"k")->value)) == 11l);
    tableDelete(&t.t, (void *)"k");
    CHECK(tableGet(&t.t, (void *)"k") == NULL);

    tableMap(&t.t, test_table_callback, (void *)&t);

    freeTable(&t.t);
}

static void test_scanner_end(void *s) {
    freeScanner((Scanner *)s);
}
static void test_scanner_1char(void *a) {
    UNUSED(a);
    TokenType expected[] = {
        TK_LPAREN, TK_RPAREN,
        TK_LBRACKET, TK_RBRACKET,
        TK_LBRACE, TK_RBRACE,
        TK_COMMA, TK_SEMICOLON, TK_COLON, TK_TILDE
    };
    Scanner s;
    initScanner(&s, "Test (1 char)", "()[]{},;:~");
    SET_END_FN(test_scanner_end, &s);

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
}
static void test_scanner_1_2char(void *a) {
    UNUSED(a);
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
    SET_END_FN(test_scanner_end, &s);

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
}
static void test_scanner_1_2_3char(void *a) {
    UNUSED(a);
    TokenType expected[] = {
        TK_GREATER, TK_GREATER_EQUAL, TK_RSHIFT, TK_RSHIFT_EQUAL,
        TK_LESS, TK_LESS_EQUAL, TK_LSHIFT, TK_LSHIFT_EQUAL,
        TK_ELIPSIS, TK_DOT, // elipsis before dot becase '. ...' => TK_ELIPSIS, TK_DOT
    };
    Scanner s;
    initScanner(&s, "Test (1, 2 or 3 char)", "> >= >> >>= < <= << <<= ... .");
    SET_END_FN(test_scanner_end, &s);

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
}
static void test_scanner_literals(void *a) {
    UNUSED(a);
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
    SET_END_FN(test_scanner_end, &s);

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
}
static void test_scanner_keywords(void *a) {
    UNUSED(a);
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
    SET_END_FN(test_scanner_end, &s);

    Token tk = nextToken(&s);
    for(int idx = 0; idx < (int)(sizeof(expected)/sizeof(expected[0])); ++idx) {
        CHECK(tk.type != TK_EOF || tk.type != TK_ERROR);
        CHECK(tk.type == expected[idx]);
        tk = nextToken(&s);
    }

    freeScanner(&s);
}

struct test_parser {
    Scanner s;
    ASTProg prog;
    Parser p;
};
static void test_parser_end(void *data) {
    struct test_parser *t = (struct test_parser *)data;
    freeParser(&t->p);
    freeASTProg(&t->prog);
    freeScanner(&t->s);
}
static void test_parser_unary_expressions(void *a) {
    UNUSED(a);
    ASTNodeType expected[] = {
        ND_NUM, ND_NEG, ND_RETURN, ND_CALL
    };
    struct test_parser t;
    initScanner(&t.s, "Test (unary expressions)", "fn main() {42; -2; return 3; call();}");
    initASTProg(&t.prog);
    initParser(&t.p, &t.s, &t.prog);
    SET_END_FN(test_parser_end, &t);

    CHECK(parserParse(&t.p));
    ASTFunction *main = (ASTFunction *)t.prog.functions.data[0];
    CHECK(main->body->body.used == 4);
    for(size_t i = 0; i < main->body->body.used; ++i) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &main->body->body, i);
        switch(n->type) {
            case ND_EXPR_STMT:
                CHECK(AS_UNARY_NODE(n)->child->type == expected[i]);
                break;
            case ND_RETURN:
                CHECK(AS_UNARY_NODE(n)->child->type == ND_NUM);
                break;
            default:
                UNREACHABLE();
        }
    }

    freeParser(&t.p);
    freeASTProg(&t.prog);
    freeScanner(&t.s);
}
static void test_parser_binary_expressions(void *a) {
    UNUSED(a);
    ASTNodeType expected[] = {
        ND_ASSIGN,
        ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_REM,
        ND_EQ, ND_NE,
        ND_GT, ND_GE,
        ND_LT, ND_LE,
        ND_BIT_OR,
        ND_XOR,
        ND_BIT_AND,
        ND_BIT_RSHIFT, ND_BIT_LSHIFT,
    };
    struct test_parser t;
    initScanner(&t.s, "Test (binary expressions)", "fn main() {var a = 42; 2 + 2; 4 - 2; 2 * 2; 4 / 2; 4 % 2; a == 42; a != 42; a > 2; a >= 42; a < 50; a <= 42; a | 2; a ^ 2; a & 2; a >> 2; a << 2;}");
    initASTProg(&t.prog);
    initParser(&t.p, &t.s, &t.prog);
    SET_END_FN(test_parser_end, &t);

    CHECK(parserParse(&t.p));
    ASTFunction *main = (ASTFunction *)t.prog.functions.data[0];
    CHECK(main->body->body.used == 17);
    for(size_t i = 0; i < main->body->body.used; ++i) {
        ASTNode *n = ARRAY_GET_AS(ASTNode *, &main->body->body, i);
        switch(n->type) {
            case ND_EXPR_STMT:
                CHECK(AS_UNARY_NODE(n)->child->type == expected[i]);
                break;
            default:
                LOG_F("type: %d", n->type);
                UNREACHABLE();
        }
    }

    freeParser(&t.p);
    freeASTProg(&t.prog);
    freeScanner(&t.s);
}

// TODO: Symbols, Parser (finish)
Test tests[] = {
    {"Strings", test_strings, NULL},
    {"Array", test_array, NULL},
    {"Table", test_table, NULL},
    {"Scanner (1 character)", test_scanner_1char, NULL},
    {"Scanner (1 or 2 character)", test_scanner_1_2char, NULL},
    {"Scanner (1, 2 or 3 character)", test_scanner_1_2_3char, NULL},
    {"Scanner (literals)", test_scanner_literals, NULL},
    {"Scanner (keywords)", test_scanner_keywords, NULL},
    {"Parser (unary expressions)", test_parser_unary_expressions, NULL},
    {"Parser (binary expressions)", test_parser_binary_expressions, NULL},
    {NULL, NULL, NULL}
};

int main(void) {
    return RUN_SUIT(tests);
}
