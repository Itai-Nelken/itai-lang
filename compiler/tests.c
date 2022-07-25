#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include "common.h"
#include "Strings.h"
#include "Array.h"
#include "Table.h"

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

void _log(const char *format, ...) {
    va_list ap;

    printf("[test %d]: \x1b[1;33mLOG:\x1b[0m ", __data.current_test_number);
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    putchar('\n');
}

int _run_test_list(Test testlist[]) {
    _init();
    _count_tests(testlist);
    bool had_failure = false;

    for(; __data.current_test_number < __data.total_test_count; ++__data.current_test_number) {
        __data.current_test = &testlist[__data.current_test_number];
        __data.current_test->fn(__data.current_test->arg);
        if(__data.failure_count > 0) {
            had_failure = true;
        }
        _print_test_summary();
    }
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
 ***/
#define CHECK(expr) (_check((expr), #expr))
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

static void test_strings(void *a) {
    UNUSED(a);
    char *s1 = NULL, *s2 = NULL, *s3 = NULL;
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

    stringFree(s2);
    stringFree(s3);
    stringFree(s1);
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

Test tests[] = {
    {"Strings", test_strings, NULL},
    {"Array", test_array, NULL},
    {"Table", test_table, NULL},
    {NULL, NULL, NULL}
};

int main(void) {
    return RUN_TESTS(tests);
}
