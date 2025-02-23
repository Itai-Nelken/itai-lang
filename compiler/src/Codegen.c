#include <stdio.h> // FILE
#include <stdarg.h>
#include "common.h"
#include "Ast/Ast.h"
#include "Codegen.h"

/**
 * Function types in C are declared with the variable in the middle,
 * so to be able to use them as normal types, we need to predeclare them
 * like this for example:
 * | typedef void (*fn0)(int, int);
 * would declare 'fn0' as the type for 'fn(i32, i32) -> void'
 *
 * Methods are also a bit tricky. Since C doesn't support methods,
 * they are generated as normal functions with an internal name
 * which is stored in the 'methodCNames' table.
 **/

typedef struct codegen {
    FILE *output;
    ASTProgram *program;
    Table fnTypes; // Table<ASTString, ASTString> (typename, C typename)
    unsigned fnTypenameCounter;
    bool isInCall; // For proper method call generation.
    ASTObj *currentFn;
    Array defersInCurrentFn; // Array<ASTStmtNode *>
    Table methodCNames; // Table<ASTString, ASTString> (name, C name)
} Codegen;

static void print(Codegen *cg, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(cg->output, format, ap);
    va_end(ap);
}

// Note: [prefix] & [postfix] can be NULL if not needed.
static void genInternalID(Codegen *cg, const char *name, const char *prefix, const char *postfix) {
    print(cg, "%s___ilc_internal__%s%s", prefix ? prefix : "", name, postfix ? postfix : "");
}

static void genType(Codegen *cg, Type *ty) {
    switch(ty->type) {
        case TY_VOID:
        case TY_I32:
        case TY_U32:
        case TY_STR:
        case TY_BOOL:
        case TY_STRUCT: // yes, its here. This is intentional.
            print(cg, "%s", ty->name);
            break;
        case TY_POINTER:
            genType(cg, ty->as.ptr.innerType);
            print(cg, "*");
            break;
        case TY_FUNCTION: {
            TableItem *item = tableGet(&cg->fnTypes, (void *)ty->name);
            VERIFY(item);
            print(cg, "%s", (ASTString)item->value);
            break;
        }
        case TY_IDENTIFIER:
        default:
            UNREACHABLE();
    }
}

static void genExpr(Codegen *cg, ASTExprNode *expr);
static void genPropertyAccessExpr(Codegen *cg, ASTExprNode *expr) {
    Array stack;
    arrayInit(&stack);
    while(NODE_IS(NODE_AS(ASTBinaryExpr, expr)->lhs, EXPR_PROPERTY_ACCESS)) {
        arrayPush(&stack, (void *)NODE_AS(ASTBinaryExpr, expr)->rhs);
        expr = NODE_AS(ASTBinaryExpr, expr)->lhs;
    }
    arrayPush(&stack, (void *)NODE_AS(ASTBinaryExpr, expr)->rhs);
    arrayPush(&stack, (void *)NODE_AS(ASTBinaryExpr, expr)->lhs);
    while(arrayLength(&stack) > 0) {
        ASTExprNode *n = ARRAY_POP_AS(ASTExprNode *, &stack);
        genExpr(cg, n);
        if(arrayLength(&stack) > 0) {
            print(cg, ".");
        }
    }
    arrayFree(&stack);
}

static const char *binaryOperatorToString(ASTExprType node_type) {
    switch(node_type) {
        case EXPR_ASSIGN: return "=";
        case EXPR_ADD: return "+";
        case EXPR_SUBTRACT: return "-";
        case EXPR_MULTIPLY: return "*";
        case EXPR_DIVIDE: return "/";
        case EXPR_EQ: return "==";
        case EXPR_NE: return "!=";
        case EXPR_LT: return "<";
        case EXPR_LE: return "<=";
        case EXPR_GT: return ">";
        case EXPR_GE: return ">=";
        default:
            UNREACHABLE();
    }
}

static void genExpr(Codegen *cg, ASTExprNode *expr) {
    switch(expr->type) {
        // Constant value nodes.
        case EXPR_NUMBER_CONSTANT:
            print(cg, "%lu", NODE_AS(ASTConstantValueExpr, expr)->as.number);
            break;
        case EXPR_STRING_CONSTANT:
            print(cg, "%s", NODE_AS(ASTConstantValueExpr, expr)->as.string);
            break;
        case EXPR_BOOLEAN_CONSTANT:
            print(cg, "%s", NODE_AS(ASTConstantValueExpr, expr)->as.boolean ? "true" : "false");
            break;
        // Obj nodes
        case EXPR_VARIABLE:
        case EXPR_FUNCTION:
            print(cg, "%s", NODE_AS(ASTObjExpr, expr)->obj->name);
            break;
        // Binary nodes
        case EXPR_PROPERTY_ACCESS:
            if(cg->isInCall) {
                // Generate C method name.
                ASTExprNode *methodNode = NODE_AS(ASTBinaryExpr, expr)->rhs;
                VERIFY(NODE_IS(methodNode, EXPR_VARIABLE) && methodNode->dataType->type == TY_FUNCTION);
                ASTString methodName = NODE_AS(ASTObjExpr, methodNode)->obj->name;
                TableItem *item = tableGet(&cg->methodCNames, (void *)methodName);
                VERIFY(item);
                ASTString CName = (ASTString)item->value;
                print(cg, "%s", CName);
            } else {
                genPropertyAccessExpr(cg, expr);
            }
            break;
        case EXPR_ASSIGN:
        case EXPR_ADD:
        case EXPR_SUBTRACT:
        case EXPR_MULTIPLY:
        case EXPR_DIVIDE:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
            genExpr(cg, NODE_AS(ASTBinaryExpr, expr)->lhs);
            print(cg, " %s ", binaryOperatorToString(expr->type));
            genExpr(cg, NODE_AS(ASTBinaryExpr, expr)->rhs);
            break;
        // Unary nodes
        case EXPR_NEGATE:
            print(cg, "-(");
            genExpr(cg, NODE_AS(ASTUnaryExpr, expr)->operand);
            print(cg, ")");
            break;
        case EXPR_NOT:
            print(cg, "!(");
            genExpr(cg, NODE_AS(ASTUnaryExpr, expr)->operand);
            print(cg, ")");
            break;
        case EXPR_ADDROF:
            print(cg, "&(");
            genExpr(cg, NODE_AS(ASTUnaryExpr, expr)->operand);
            print(cg, ")");
            break;
        case EXPR_DEREF:
            print(cg, "(*");
            genExpr(cg, NODE_AS(ASTUnaryExpr, expr)->operand);
            print(cg, ")");
            break;
        // Call nodes
        case EXPR_CALL:
            cg->isInCall = true;
            genExpr(cg, NODE_AS(ASTCallExpr, expr)->callee);
            print(cg, "(");
            for(usize i = 0; i < arrayLength(&NODE_AS(ASTCallExpr, expr)->arguments); ++i) {
                ASTExprNode *arg = ARRAY_GET_AS(ASTExprNode *, &NODE_AS(ASTCallExpr, expr)->arguments, i);
                genExpr(cg, arg);
                if(i + 1 < arrayLength(&NODE_AS(ASTCallExpr, expr)->arguments)) {
                    print(cg, ", ");
                }
            }
            print(cg, ")");
            cg->isInCall = false;
            break;
        // Other nodes
        case EXPR_IDENTIFIER:
        default:
            UNREACHABLE();
    }
}

static void genVarDecl(Codegen *cg, ASTVarDeclStmt *vdecl) {
    genType(cg, vdecl->variable->dataType);
    print(cg, " %s", vdecl->variable->name);
    if(vdecl->initializer) {
        print(cg, " = ");
        genExpr(cg, vdecl->initializer);
    }
    print(cg, ";\n");
}

static void genStmt(Codegen *cg, ASTStmtNode *stmt) {
    switch(stmt->type) {
        // VarDecl nodes
        case STMT_VAR_DECL:
            genVarDecl(cg, NODE_AS(ASTVarDeclStmt, stmt));
            break;
        // Block nodes
        case STMT_BLOCK:{
            print(cg, "{\n");
            Scope *scope = NODE_AS(ASTBlockStmt, stmt)->scope;
            // SCOPE_DEPTH_BLOCK == root of function scope tree.
            bool isFunctionScope = scope->depth == SCOPE_DEPTH_BLOCK;
            if(isFunctionScope && cg->currentFn->as.fn.returnType->type != TY_VOID) {
                genType(cg, cg->currentFn->as.fn.returnType);
                genInternalID(cg, "return_value", " ", ";\n\n");
            }
            print(cg, "// start block\n");
            for(usize i = 0; i < arrayLength(&NODE_AS(ASTBlockStmt, stmt)->nodes); ++i) {
                ASTStmtNode *node = ARRAY_GET_AS(ASTStmtNode *, &NODE_AS(ASTBlockStmt, stmt)->nodes, i);
                genStmt(cg, node);
            }
            print(cg, "// end block\n\n");


            if(isFunctionScope) {
                print(cg, "_fn_%s_end:\n// start defers\n", cg->currentFn->name, cg->currentFn->name);
                for(usize i = 0; i < arrayLength(&cg->defersInCurrentFn); ++i) {
                    ASTStmtNode *defer = ARRAY_GET_AS(ASTStmtNode *, &cg->defersInCurrentFn, i);
                    genStmt(cg, NODE_AS(ASTDeferStmt, defer)->body);
                }
                print(cg, "// end defers\n");
                print(cg, "return");
                if(cg->currentFn->as.fn.returnType->type != TY_VOID) {
                    genInternalID(cg, "return_value", " ", NULL);
                }
                print(cg, ";\n");
            }
            print(cg, "}\n");
            break;
        }
        // Conditional nodes
        case STMT_EXPECT:
            if(NODE_AS(ASTConditionalStmt, stmt)->then == NULL) {
                print(cg, "if(");
                genExpr(cg, NODE_AS(ASTConditionalStmt, stmt)->condition);
                print(cg, ") {\n");
                // TODO: provide location info.
                print(cg, "fprintf(stderr, \"Failed expect!\\n\");\n");
                print(cg, "exit(1);\n");
                print(cg, "}\n");
                break;
            }
            // fallthrough
        case STMT_IF:
            print(cg, "if(");
            genExpr(cg, NODE_AS(ASTConditionalStmt, stmt)->condition);
            print(cg, ") ");
            genStmt(cg, NODE_AS(ASTConditionalStmt, stmt)->then);
            if(NODE_AS(ASTConditionalStmt, stmt)->else_) {
                print(cg, "else ");
                genStmt(cg, NODE_AS(ASTConditionalStmt, stmt)->else_);
            }
            break;
        // Loop nodes
        case STMT_LOOP:
            print(cg, "for(");
            if(NODE_AS(ASTLoopStmt, stmt)->initializer) {
                genStmt(cg, NODE_AS(ASTLoopStmt, stmt)->initializer);
            } else {
                print(cg, ";");
            }
            genExpr(cg, NODE_AS(ASTLoopStmt, stmt)->condition);
            print(cg, ";");
            if(NODE_AS(ASTLoopStmt, stmt)->increment) {
                genExpr(cg, NODE_AS(ASTLoopStmt, stmt)->increment);
            }
            print(cg, ") ");
            genStmt(cg, NODE_AS(ASTStmtNode, NODE_AS(ASTLoopStmt, stmt)->body));
            break;
        // Expr nodes
        case STMT_RETURN:
            if(NODE_AS(ASTExprStmt, stmt)->expression) {
                genInternalID(cg, "return_value", NULL, " = ");
                genExpr(cg, NODE_AS(ASTExprStmt, stmt)->expression);
                print(cg, ";\n");
            }
            print(cg, "goto _fn_%s_end;\n", cg->currentFn->name);
            break;
        case STMT_EXPR:
            genExpr(cg, NODE_AS(ASTExprStmt, stmt)->expression);
            print(cg, ";\n");
            break;
        // Defer nodes
        case STMT_DEFER:
            // Defers are generated at the end of the function body.
            arrayPush(&cg->defersInCurrentFn, (void *)stmt);
            break;
        default:
            UNREACHABLE();
    }
}

static void genScope(Codegen *cg, Scope *sc, Table *moduleTypeTable, ASTObj *structure);
static void genStruct(Codegen *cg, ASTObj *st) {
    print(cg, "struct %s {\n", st->name); // typedef is done in predecl.
    Array objects; // Array<ASTObj *>
    arrayInitSized(&objects, scopeGetNumObjects(st->as.structure.scope));
    scopeGetAllObjects(st->as.structure.scope, &objects);
    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        if(obj->type == OBJ_VAR) {
            genType(cg, obj->dataType);
            print(cg, " %s;\n", obj->name);
        }
    }
    arrayFree(&objects);
    print(cg, "};\n");
    genScope(cg, st->as.structure.scope, NULL, st);
}

static void collect_type_callback(TableItem *item, bool is_last, void *type_array) {
    UNUSED(is_last);
    Type *ty = (Type *)item->value;
    if(ty->type == TY_STRUCT) {
        arrayPush((Array *)type_array, (void *)ty);
    }
}

// typeTable: Table<ASTString, Type *>
// output: Array<Type *>
static void topologicallySortTypes(Table *typeTable, Array *output) {
    Array types;
    arrayInit(&types);
    tableMap(typeTable, collect_type_callback, (void *)&types);

    Table dependencies; // Table<ASTString, i64>
    tableInit(&dependencies, NULL, NULL);

    ARRAY_FOR(i, types) {
        Type *ty = ARRAY_GET_AS(Type *, &types, i);
        VERIFY(ty->type == TY_STRUCT);
        ARRAY_FOR(j, ty->as.structure.fieldTypes) {
            Type *fieldTy = ARRAY_GET_AS(Type *, &ty->as.structure.fieldTypes, j);
            if(fieldTy->type == TY_STRUCT) {
                TableItem *item = tableGet(&dependencies, (void *)fieldTy->name);
                i64 existing = item == NULL ? 0 : (i64)item->value;
                tableSet(&dependencies, (void *)fieldTy->name, (void *)(existing + 1));
            }
        }
        if(tableGet(&dependencies, (void *)ty->name) == NULL) {
            tableSet(&dependencies, (void *)ty->name, (void *)0l);
        }
    }

    Array stack;
    arrayInit(&stack);
    ARRAY_FOR(i, types) {
        Type *ty = ARRAY_GET_AS(Type *, &types, i);
        TableItem *item = tableGet(&dependencies, (void *)ty->name);
        if((i64)item->value == 0) {
            arrayPush(&stack, (void *)ty);
        }
    }

    arrayClear(output);
    while(arrayLength(&stack) > 0) {
        Type *ty = ARRAY_POP_AS(Type *, &stack);
        VERIFY(ty->type == TY_STRUCT);
        arrayPush(output, (void *)ty);
        ARRAY_FOR(i, ty->as.structure.fieldTypes) {
            Type *fieldTy = ARRAY_GET_AS(Type *, &ty->as.structure.fieldTypes, i);
            if(fieldTy->type == TY_STRUCT) {
                TableItem *item = tableGet(&dependencies, (void *)fieldTy->name);
                i64 existing = (i64)item->value;
                tableSet(&dependencies, (void *)fieldTy->name, (void *)(existing - 1));
                if(existing == 1) {
                    arrayPush(&stack, fieldTy);
                }
            }
        }
    }
    arrayFree(&stack);

    if(arrayLength(output) != arrayLength(&types)) {
        LOG_ERR("internal error: Undetected cyclic structs!");
        UNREACHABLE();
    }
    // Reverse output array so structure with the least dependencies
    // will be first, then the next one etc.
    // The last structure has the most dependencies.
    // Note: dependency means that another struct depends on it.
    arrayReverse(output);

    tableFree(&dependencies);
    arrayFree(&types);
}

// moduleTypeTable: for module scope ONLY. NULL otherwise
// structure: Enclosing struct. for struct scope ONLY, NULL otherwise.
static void genScope(Codegen *cg, Scope *sc, Table *moduleTypeTable, ASTObj *structure) {
    VERIFY(sc->depth == SCOPE_DEPTH_MODULE_NAMESPACE || sc->depth == SCOPE_DEPTH_STRUCT);
    print(cg, "/* scope, depth: %d */\n", sc->depth);
    if(moduleTypeTable) {
        print(cg, "// structs:\n");
        Array sortedStructTypes; // Array<Type *> (TY_STRUCT)
        arrayInit(&sortedStructTypes);
        topologicallySortTypes(moduleTypeTable, &sortedStructTypes);
        ARRAY_FOR(i, sortedStructTypes) {
            Type *ty = ARRAY_GET_AS(Type *, &sortedStructTypes, i);
            ASTObj *st = scopeGetObject(sc, OBJ_STRUCT, ty->name);
            VERIFY(st);
            genStruct(cg, st);
            print(cg, "\n");
        }
        arrayFree(&sortedStructTypes);
    }
    // Note: The newline before is printed by the loop/print() before the loop if no structs exist.
    print(cg, "// predeclarations:\n");
    Array objects; // Array<ASTObj *>
    arrayInit(&objects);
    scopeGetAllObjects(sc, &objects);
    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        if(obj->type == OBJ_FN) {
            // function predecl
            ASTString CName = obj->name;
            if(sc->depth == SCOPE_DEPTH_STRUCT) {
                CName = stringTableFormat(cg->program->strings, "___ilc_internal__struct%s_method_%s", structure->name, obj->name);
                tableSet(&cg->methodCNames, (void *)obj->name, (void *)CName);
            }
            genType(cg, obj->as.fn.returnType);
            print(cg, " %s(", CName);
            ARRAY_FOR(i, obj->as.fn.parameters) {
                ASTObj *param = ARRAY_GET_AS(ASTObj *, &obj->as.fn.parameters, i);
                genType(cg, param->dataType);
                if(i + 1 < arrayLength(&obj->as.fn.parameters)) {
                    print(cg, ", ");
                }
            }
            print(cg, ");\n");
        }
    }
    // Actually generate the rest of the objects (actually only functions).
    print(cg, "// declarations: \n");
    ARRAY_FOR(i, objects) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &objects, i);
        if(obj->type == OBJ_FN) {
            cg->currentFn = obj;
            ASTString CName = obj->name;
            if(sc->depth == SCOPE_DEPTH_STRUCT) {
                TableItem *item = tableGet(&cg->methodCNames, (void *)obj->name);
                VERIFY(item);
                CName = (ASTString)item->value;
            }
            arrayClear(&cg->defersInCurrentFn);
            genType(cg, obj->as.fn.returnType);
            print(cg, " %s(", CName);
            ARRAY_FOR(i, obj->as.fn.parameters) {
                ASTObj *param = ARRAY_GET_AS(ASTObj *, &obj->as.fn.parameters, i);
                genType(cg, param->dataType);
                print(cg, " %s", param->name);
                if(i + 1 < arrayLength(&obj->as.fn.parameters)) {
                    print(cg, ", ");
                }
            }
            print(cg, ") ");
            genStmt(cg, NODE_AS(ASTStmtNode, obj->as.fn.body));
            cg->currentFn = NULL;
        }
    }
    print(cg, "/* end scope */\n");
    arrayFree(&objects);
}

static void predecl_struct_types_cb(TableItem *item, bool isLast, void *cl) {
    UNUSED(isLast);
    Codegen *cg = (Codegen *)cl;
    Type *ty = (Type *)item->value;
    if(ty->type == TY_STRUCT) {
        print(cg, "typedef struct %s %s;\n", ty->name, ty->name);
    }
}
static void predecl_fn_types_cb(TableItem *item, bool isLast, void *cl) {
    UNUSED(isLast);
    Codegen *cg = (Codegen *)cl;
    Type *ty = (Type *)item->value;
    if(ty->type == TY_FUNCTION) {
        ASTString fnCTypename = stringTableFormat(cg->program->strings, "fn%u", cg->fnTypenameCounter++);
        tableSet(&cg->fnTypes, (void *)ty->name, (void *)fnCTypename);
        print(cg, "typedef ");
        genType(cg, ty->as.fn.returnType);
        print(cg, " (*%s)(", fnCTypename);
        ARRAY_FOR(i, ty->as.fn.parameterTypes) {
            Type *paramTy = ARRAY_GET_AS(Type *, &ty->as.fn.parameterTypes, i);
            genType(cg, paramTy);
            if(i + 1 < arrayLength(&ty->as.fn.parameterTypes)) {
                print(cg, ", ");
            }
        }
        print(cg, ");\n");
    }
}

static void genModule(Codegen *cg, ASTModule *m) {
    print(cg, "/* Module '%s' */\n", m->name);
    print(cg, "// Module variables:\n");
    ARRAY_FOR(i, m->variableDecls) {
        ASTVarDeclStmt *vdecl = ARRAY_GET_AS(ASTVarDeclStmt *, &m->variableDecls, i);
        genVarDecl(cg, vdecl);
    }
    // Declare function and struct types. See block comment at top of this file.
    print(cg, "// Struct & Function predeclarations:\n");
    tableMap(&m->types, predecl_struct_types_cb, (void *)cg);
    tableMap(&m->types, predecl_fn_types_cb, (void *)cg);
    print(cg, "// Module scope:\n");
    genScope(cg, m->moduleScope, &m->types, NULL);
}

static void genHeader(Codegen *cg) {
    print(cg, "// File generated by ilc\n\n");
    print(cg, "#include<stdio.h>\n#include<stdlib.h>\n#include <stdint.h>\n#include <stdbool.h>\n\n"); // bool included here.
    print(cg, "// primitive types:\n");
    // TODO: do this for the types stored in each module.
    //       - This would mean making different files for each module.
    print(cg, "typedef int32_t i32;\ntypedef uint32_t u32;\ntypedef const char *str;\n\n");
}

void codegenGenerate(FILE *output, ASTProgram *prog) {
    VERIFY(output);
    VERIFY(prog);
    Codegen cg = {
        .output = output,
        .program = prog,
        .fnTypenameCounter = 0,
        .currentFn = NULL,
        .isInCall = false
    };
    tableInit(&cg.fnTypes, NULL, NULL);
    arrayInit(&cg.defersInCurrentFn);
    tableInit(&cg.methodCNames, NULL, NULL);
    genHeader(&cg);
    ARRAY_FOR(i, prog->modules) {
        ASTModule *m = ARRAY_GET_AS(ASTModule *, &prog->modules, i);
        genModule(&cg, m);
    }
    tableFree(&cg.methodCNames);
    arrayFree(&cg.defersInCurrentFn);
    tableFree(&cg.fnTypes);
}
