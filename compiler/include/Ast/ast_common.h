#ifndef AST_COMMON_H
#define AST_COMMON_H

#include <stdio.h>
#include <stdbool.h>
#include "Token.h" // Location
#include "Table.h"


/* Utility macros */

#define PRINT_ARRAY(type, print_fn, stream, array) ARRAY_FOR(i, (array)) { \
    print_fn((stream), ARRAY_GET_AS(type, &(array), i)); \
    if(i + 1 < arrayLength(&(array))) { \
        fputs(", ", (stream)); \
        } \
    }


/* ModuleID */

typedef u64 ModuleID;

#define EMPTY_MODULE_ID ((ModuleID)-1)


/* ASTString */

// An ASTInternedString holds a unique string.
// It is guaranteed that to equal ASTInternedStrings will point
// to the same string in memory.
typedef char *ASTInternedString;

// An ASTString is an interned string with a location.
// It is used to represent strings in the source code (such as identifiers & string literals).
// [data] should NOT be changed.
// [data] can be used as a 'String' (with stringDuplicate() for example) as long as it isn't changed.
typedef struct ast_string {
    Location location;
    ASTInternedString data;
} ASTString;


/***
 * Create a new ASTString.
 *
 * @param location_ The location of the string.
 * @param string The string data.
 ***/
#define AST_STRING(string, location_) ((ASTString){.location = (location_), .data = (string)})

/***
 * Print an ASTString.
 *
 * @param to The stream to print to.
 * @param str The ASTString to print.
 ***/
void astStringPrint(FILE *to, ASTString *str);


/* ASTStringTable */

typedef struct ast_string_table {
    // TODO/Idea: Store strings in arena owned by this struct?
    Table strings; // Table<ASTInternedString, String> - NOTE: the same string is stored both as the key & the value for no reason at all.
} ASTStringTable;

/***
 * Initialize an ASTStringTable.
 *
 * @param st The ASTStringTable to initialize.
 ****/
void astStringTableInit(ASTStringTable *st);

/***
 * Free an ASTStringTable & all strings in it.
 *
 * @param st The ASTStringTable to free.
 ***/
void astStringTableFree(ASTStringTable *st);

/***
 * Intern a string.
 * NOTE: Ownership of [str] is NOT taken.
 *
 * @param st The ASTStringTable to save the string in.
 * @param str The string to intern.
 ***/
ASTInternedString astStringTableAddString(ASTStringTable *st, char *str);

/***
 * Print an ASTStringTable.
 *
 * @param to The stream to print to.
 * @param st Rhe ASTStringTable to print.
 ***/
void astStringTablePrint(FILE *to, ASTStringTable *st);


/* Value */

typedef enum value_type {
    VAL_NUMBER,
    VAL_STRING
} ValueType;

typedef struct value {
    ValueType type;
    union {
        u64 number;
        ASTString string;
    } as;
} Value;

/***
 * Create a new Value.
 *
 * @param literal_type (type: ValueType) The type of the value.
 * @param value_name (type: C identifier) The name of the value's field in the 'as' union.
 * @param value (type: any) The value itself (must be a constexpr).
 ***/
#define MAKE_VALUE(value_type, value_name, value) ((Value){.type = (value_type), .as.value_name = (value)})

/***
 * Print a Value.
 *
 * @param to The stream to print to.
 * @param value The value to print.
 ***/
void valuePrint(FILE *to, Value value);


/* ScopeID */

typedef struct scope_id {
    ModuleID module;
    usize index; // Index into the ASTModule::scopes array.
} ScopeID;

// FIXME: find a better way to represent an empty ScopeID.
#define EMPTY_SCOPE_ID ((ScopeID){.module = EMPTY_MODULE_ID, .index = (usize)-1})

void scopeIDPrint(FILE *to, ScopeID scope_id, bool compact);

static inline bool scopeIDCompare(ScopeID a, ScopeID b) {
    return (a.module == b.module) && (a.index == b.index);
}

/* ControlFlow */

typedef enum control_flow {
    CF_NONE,
    CF_NEVER_RETURNS,
    CF_MAY_RETURN,
    CF_ALWAYS_RETURNS,
    __CF_STATE_COUNT
} ControlFlow;

/***
 * Update a ControlFlow with a new one.
 *
 * @param old The old ControlFlow.
 * @param new The new ControlFlow.
 * @return The updated ControlFlow.
 ***/
ControlFlow controlFlowUpdate(ControlFlow old, ControlFlow new);

/***
 * Pretty-print A ControlFlow state to stream [to].
 *
 * @param to The stream to print to.
 * @param cf The ControlFlow to print.
 ***/
void controlFlowPrint(FILE *to, ControlFlow cf);


/* Attribute */

// Note: update tables in attributeTypeString() and attribute_type_name() when adding new types.
typedef enum attribute_type {
    ATTR_SOURCE,
    //ATTR_DESTRUCTOR
} AttributeType;

typedef struct attribute {
    AttributeType type;
    Location location;
    union {
        ASTString source;
        //ASTObj *destructor;
    } as;
} Attribute;

/***
 * Create a new Attributute.
 *
 * @param type The AttributeType of the new attribute.
 * @param loc The Location where the attribute was defined.
 * @return The new attribute (heap allocated).
 ***/
Attribute *attributeNew(AttributeType type, Location loc);

/***
 * Free an Attribute.
 *
 * @param a The Attribute to free.
 ***/
void attributeFree(Attribute *a);

/***
 * Print an Attribute.
 *
 * @param to The stream to print to.
 * @param a The Attribute to print.
 ***/
void attributePrint(FILE *to, Attribute *a);

/***
 * Return a user friendly string version of an AttributeType.
 *
 * @param type An AttributeType.
 * @return The type as a user friendly string.
 ***/
const char *attributeTypeString(AttributeType type);


/* AST nodes - common */

#define NODE_IS(node, type_) ((node)->type == (type_))
#define NODE_AS(node_typename, node) ((node_typename *)(node))


/* ASTObj - common */

typedef enum ast_obj_type {
    OBJ_VAR, OBJ_FN,
    OBJ_STRUCT,
    OBJ_EXTERN_FN,
    OBJ_TYPE_COUNT
} ASTObjType;

const char *__ast_obj_type_name(ASTObjType type);

#endif // AST_COMMON_H
