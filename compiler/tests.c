#include <stdio.h>
#include <stdlib.h> // abort
#include <string.h> // strsignal
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h> // kill

#define MAX_FAILURES_IN_TEST 20

typedef struct test {
    const char *name;
    void (*fn)(void *);
    void *arg;
} Test;

typedef struct test_list_data {
    Test *current_test;
    int total_test_count, current_test_number;
    const char *failure_texts[MAX_FAILURES_IN_TEST];
    int failure_count;

    jmp_buf panic_jump;
    bool panic_jump_set;
} TestListData;

static TestListData __data;

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
    __data.panic_jump_set = false;
}

static void _count_tests(Test testlist[]) {
    for(; testlist[__data.total_test_count].name && testlist[__data.total_test_count].fn; ++__data.total_test_count);
}

bool _check(bool result, const char *expr) {
    assert(__data.current_test);
    if(result) {
        return true;
    }
    assert(__data.failure_count < MAX_FAILURES_IN_TEST);
    __data.failure_texts[__data.failure_count++] = expr;
    return false;
}

void _assert(bool result, const char *expr) {
    assert(__data.current_test);
    if(result) {
        return;
    }
    assert(__data.failure_count < MAX_FAILURES_IN_TEST);
    __data.failure_texts[__data.failure_count++] = expr;
    kill(getpid(), SIGUSR1); // will be catched by the signal handler
}

void _log(const char *format, ...) {
    va_list ap;

    printf("[test %d]: \x1b[1;33mLOG:\x1b[0m ", __data.current_test_number);
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    putchar('\n');
}

static void signal_handler(int signal) {
    fprintf(stderr, "[test %d]: recieved signal %d (%s)!", __data.current_test_number + 1, signal, strsignal(signal));
    if(__data.panic_jump_set) {
        fputc('\n', stderr);
        fflush(stderr);
        longjmp(__data.panic_jump, 1);
    } else {
        fputs("no jump, exiting.\n", stderr);
        fflush(stderr);
        exit(1);
    }
}

static void assert_handler(int signal) {
    (void)signal; // unused
    fprintf(stderr, "[test %d]: assertion failed!", __data.current_test_number + 1);
    if(__data.panic_jump_set) {
        fputc('\n', stderr);
        fflush(stderr);
        longjmp(__data.panic_jump, 1);
    } else {
        fputs("no jump, exiting.\n", stderr);
        fflush(stderr);
        exit(1);
    }
}

int _run_test_list(Test testlist[]) {
    _init();
    _count_tests(testlist);
    bool had_failure = false;

    struct sigaction new_act;
    new_act.sa_handler = signal_handler;
    sigemptyset(&new_act.sa_mask);
    sigaction(SIGSEGV, &new_act, NULL);
    sigaction(SIGABRT, &new_act, NULL);
    new_act.sa_handler = assert_handler;
    sigaction(SIGUSR1, &new_act, NULL);

    for(; __data.current_test_number < __data.total_test_count; ++__data.current_test_number) {
        __data.current_test = &testlist[__data.current_test_number];
        __data.panic_jump_set = true;
        if(setjmp(__data.panic_jump) != 0) {
            __data.panic_jump_set = false;
            assert(__data.failure_count < MAX_FAILURES_IN_TEST);
            __data.failure_texts[__data.failure_count++] = "panic!";
            fflush(stdout);
        } else {
            __data.current_test->fn(__data.current_test->arg);
        }
        if(__data.failure_count > 0) {
            had_failure = true;
        }
        _print_test_summary();
    }

    new_act.sa_handler = SIG_DFL;
    sigemptyset(&new_act.sa_mask);
    sigaction(SIGABRT, &new_act, NULL);
    sigaction(SIGSEGV, &new_act, NULL);
    sigaction(SIGUSR1, &new_act, NULL);
    return had_failure ? 1 : 0;
}

/***
 * Run all tests in 'testlist'.
 * 
 * @param testlist An array of Test structs.
 ***/
#define RUN_TESTS(testlist) (_run_test_list((testlist)))
/***
 * Check an expression (evaluates to true or false).
 * 
 * @param expr The expression.
 * @return true if the expression evaluates to true, false if not.
 ***/
#define CHECK(expr) (_check((expr), #expr))
/***
 * Check an expression and abort the test if false..
 *
 * @param expr The expression.
 ***/
#define ASSERT(expr) (_assert((expr), #expr))
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


// includes for the tests
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "common.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"
#include "Compiler.h"
#include "Token.h"
#include "Scanner.h"

static void test_strings(void *a) {
    UNUSED(a);
    char *s1 = NULL, *s2 = NULL, *s3 = NULL, *s4 = NULL;
    s1 = stringNew(5);
    stringAppend(&s1, "Hello,");
    CHECK(stringIsValid(s1));
    stringAppend(&s1, " %s!", "World");
    CHECK(stringIsValid(s1));
    CHECK(stringLength(s1) == 13);
    CHECK(!strcmp(s1, "Hello, World!"));
    CHECK(stringEqual(s1, "Hello, World!"));

    // stringDuplicate() & stringCopy() use stringNCopy(), so it's also tested.
    s2 = stringDuplicate(s1);
    s3 = stringCopy("Hello, World!");
    CHECK(stringEqual(s2, s3));

    s4 = stringFormat("%s", s3);
    CHECK(stringEqual(s2, s4));

    stringFree(s1);
    stringFree(s2);
    stringFree(s3);
    stringFree(s4);
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
    Array array, copy;
    arrayInit(&array);

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        arrayPush(&array, (void *)expected[i]);
    }

    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        CHECK(((long)arrayGet(&array, i)) == expected[i]);
    }

    arrayInit(&copy);
    arrayCopy(&copy, &array);
    for(int i = 0; i < ARRAY_INITIAL_CAPACITY + 2; ++i) {
        CHECK(((long)arrayPop(&copy)) == expected[expected_size-1 - i]);
    }

    arrayMap(&array, test_array_callback, NULL);

    arrayFree(&copy);
    arrayFree(&array);
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
static void test_table_callback(TableItem *item, bool is_last, void *cl) {
    UNUSED(is_last);
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
    tableInit(&t.t, NULL, NULL);

    for(int i = 0; i < (int)(sizeof(expected)/sizeof(expected[0])); ++i) {
        tableSet(&t.t, (void *)expected[i].s, (void *)expected[i].i);
    }


    CHECK(((long)(tableGet(&t.t, (void *)"k")->value)) == 11l);
    tableDelete(&t.t, (void *)"k");
    CHECK(tableGet(&t.t, (void *)"k") == NULL);

    tableMap(&t.t, test_table_callback, (void *)&t);

    tableFree(&t.t);
}

struct scanner_test_token_type {
    TokenType type;
    union {
        i64 value;
        const char *identifier;
    } as;
};
static void test_scanner(void *a) {
    UNUSED(a);
    const char *input = "fn -> i32 return (1 + 2 - 3 * 4 / 5) == 2; 2 != 2 if !2 {} else {} hello = 1; while 1 {} @";
    struct scanner_test_token_type expected[] = {
        {TK_FN,          {0}},
        {TK_ARROW,       {0}},
        {TK_I32,         {0}},
        {TK_RETURN,      {0}},
        {TK_LPAREN,      {0}},
        {TK_NUMBER,      {1}},
        {TK_PLUS,        {0}},
        {TK_NUMBER,      {2}},
        {TK_MINUS,       {0}},
        {TK_NUMBER,      {3}},
        {TK_STAR,        {0}},
        {TK_NUMBER,      {4}},
        {TK_SLASH,       {0}},
        {TK_NUMBER,      {5}},
        {TK_RPAREN,      {0}},
        {TK_EQUAL_EQUAL, {0}},
        {TK_NUMBER,      {2}},
        {TK_SEMICOLON,   {0}},
        {TK_NUMBER,      {2}},
        {TK_BANG_EQUAL,  {0}},
        {TK_NUMBER,      {2}},
        {TK_IF,          {0}},
        {TK_BANG,        {0}},
        {TK_NUMBER,      {2}},
        {TK_LBRACE,      {0}},
        {TK_RBRACE,      {0}},
        {TK_ELSE,        {0}},
        {TK_LBRACE,      {0}},
        {TK_RBRACE,      {0}},
        {TK_IDENTIFIER,  {.identifier = "hello"}},
        {TK_EQUAL,       {0}},
        {TK_NUMBER      ,{1}},
        {TK_SEMICOLON   ,{0}},
        {TK_WHILE,       {0}},
        {TK_NUMBER,      {1}},
        {TK_LBRACE,      {0}},
        {TK_RBRACE,      {0}},
        {TK_GARBAGE,     {0}},
        {TK_EOF,         {0}}
    };
    char tmp_file_name[] = "ilc_scanner_test_XXXXXX";
    int fd = mkstemp(tmp_file_name);
    CHECK(fd != -1);
    if(fd == -1) {
        LOG_F("Failed to create a temporary file: %s", strerror(errno));
        return;
    }
    usize input_length = strlen(input);
    isize written = write(fd, (void *)input, input_length);
    close(fd);
    CHECK(written == (isize)input_length);
    if(written != (isize)input_length) {
        LOG("Failed to write to temporary file!");
        return;
    }

    Compiler c;
    Scanner s;
    compilerInit(&c);
    scannerInit(&s, &c);
    compilerAddFile(&c, tmp_file_name);

    for(u32 i = 0; i < sizeof(expected)/sizeof(expected[0]); ++i) {
        Token tk = scannerNextToken(&s);
        CHECK(tk.type == expected[i].type);
        if(tk.type == TK_NUMBER) {
            CHECK(tk.as.number_constant.as.int64 == expected[i].as.value);
        } else if(tk.type == TK_IDENTIFIER && CHECK(expected[i].as.identifier != 0)) {
            CHECK(strncmp(tk.as.identifier.text, expected[i].as.identifier, tk.as.identifier.length) == 0);
        }
    }

    scannerFree(&s);
    compilerFree(&c);
    remove(tmp_file_name);
}

Test tests[] = {
    {"Strings", test_strings, NULL},
    {"Array", test_array, NULL},
    {"Table", test_table, NULL},
    {"Scanner", test_scanner, NULL},
    {NULL, NULL, NULL}
};

int main(void) {
    return RUN_TESTS(tests);
}
