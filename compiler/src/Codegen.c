#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "common.h"
#include "Table.h"
#include "Ast.h"
#include "Types.h"
#include "Codegen.h"

typedef struct codegen {
    String output_buffer;
    ASTProgram *program;
    ASTObj *current_function;
    Table fn_type_names; // Table<ASTString, ASTString> (ilc typename, C typename)
    u32 fn_typename_counter;
} Codegen;

static void codegen_init(Codegen *cg, ASTProgram *prog) {
    cg->output_buffer = stringNew(1024); // start with a 1kb buffer.
    cg->program = prog;
    tableInit(&cg->fn_type_names, NULL, NULL);
    cg->fn_typename_counter = 0;
}

static void codegen_free(Codegen *cg) {
    stringFree(cg->output_buffer);
    cg->program = NULL;
    tableFree(&cg->fn_type_names);
    cg->fn_typename_counter = 0;
}

static void print(Codegen *cg, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    String buffer = stringVFormat(format, ap);
    va_end(ap);
    stringAppend(&cg->output_buffer, "%s", buffer);
    stringFree(buffer);
}

//static bool obj_is_local_in_current_function(Codegen *cg, ASTObj *var) {
//    for(usize i = 0; i < cg->current_function->as.fn.locals.used; ++i) {
//        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &cg->current_function->as.fn.locals, i);
//        if(obj->type == OBJ_VAR && obj == var) {
//            return true;
//        }
//    }
//    return false;
//}

static void gen_type(Codegen *cg, Type *ty) {
    //if(!ty) {
    //    print(cg, "void");
    //    return;
    //}
    VERIFY(ty);
    if(ty->type == TY_FN) {
        TableItem *item;
        VERIFY((item = tableGet(&cg->fn_type_names, (void *)ty->name)) != NULL);
        print(cg, "%s", (ASTString)item->value);
    } else if(ty->type == TY_PTR) {
        print(cg, "%s*", ty->as.ptr.inner_type->name);
    } else {
        VERIFY(ty->type != TY_ID);
        print(cg, "%s", ty->name);
    }
}

static const char *binary_op_str(ASTNodeType node_type) {
    switch(node_type) {
        case ND_ADD: return "+";
        case ND_SUBTRACT: return "-";
        case ND_MULTIPLY: return "*";
        case ND_DIVIDE: return "/";
        case ND_EQ: return "==";
        case ND_NE: return "!=";
        case ND_LT: return "<";
        case ND_LE: return "<=";
        case ND_GT: return ">";
        case ND_GE: return ">=";
        default:
            UNREACHABLE();
    }
}

static void gen_variable(ASTNode *variable, Codegen *cg);
static void gen_expr(Codegen *cg, ASTNode *expr) {
    print(cg, "(");
    switch(expr->node_type) {
        // Literal nodes
        case ND_NUMBER_LITERAL:
            print(cg, "%lu", AS_LITERAL_NODE(expr)->value.as.number);
            break;
        case ND_STRING_LITERAL:
            print(cg, "\"%s\"", AS_LITERAL_NODE(expr)->value.as.str);
            break;
        // Binary nodes
        case ND_ASSIGN:
            gen_variable(AS_BINARY_NODE(expr)->lhs, cg);
            print(cg, " = ");
            gen_expr(cg, AS_BINARY_NODE(expr)->rhs);
            break;
        case ND_CALL:
            print(cg, "%s(", AS_OBJ_NODE(AS_BINARY_NODE(expr)->lhs)->obj->name);
            for(usize i = 0; i < AS_LIST_NODE(AS_BINARY_NODE(expr)->rhs)->nodes.used; ++i) {
                ASTNode *arg = ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(AS_BINARY_NODE(expr)->rhs)->nodes, i);
                gen_expr(cg, arg);
                if(i + 1 < AS_LIST_NODE(AS_BINARY_NODE(expr)->rhs)->nodes.used) {
                    print(cg, ", ");
                }
            }
            print(cg, ")");
            break;
        case ND_PROPERTY_ACCESS:
            gen_variable(expr, cg);
            break;
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
            gen_expr(cg, AS_BINARY_NODE(expr)->lhs);
            print(cg, "%s", binary_op_str(expr->node_type));
            gen_expr(cg, AS_BINARY_NODE(expr)->rhs);
            break;
        // Unary nodes
        case ND_NEGATE:
            print(cg, "-");
            gen_expr(cg, AS_UNARY_NODE(expr)->operand);
            break;
        case ND_ADDROF:
            print(cg, "&");
            gen_expr(cg, AS_UNARY_NODE(expr)->operand);
            break;
        case ND_DEREF: // FIXME: unduplicate with gen_variable():ND_DEREF
            print(cg, "*");
            gen_expr(cg, AS_UNARY_NODE(expr)->operand);
            break;
        // Object nodes
        case ND_VARIABLE:
        case ND_FUNCTION:
            print(cg, "%s", AS_OBJ_NODE(expr)->obj->name);
            break;
        default:
            UNREACHABLE();
    }
    print(cg, ")");
}

static void gen_variable(ASTNode *variable, Codegen *cg);

static void gen_stmt(Codegen *cg, ASTNode *n) {
    VERIFY(cg->current_function);
    switch(n->node_type) {
        case ND_RETURN:
            if(AS_UNARY_NODE(n)->operand) {
                print(cg, "__ILC_INTERNAL_ID(return_value) = ");
                gen_expr(cg, AS_UNARY_NODE(n)->operand);
                print(cg, ";\n");
            }
            print(cg, "goto _fn_%s_end;\n", cg->current_function->name);
            break;
        case ND_BLOCK: {
            print(cg, "{\n");
            bool is_function_scope = AS_LIST_NODE(n)->scope.depth == FUNCTION_SCOPE_DEPTH;
            if(is_function_scope && cg->current_function->as.fn.return_type->type != TY_VOID) { // is function scope
                gen_type(cg, cg->current_function->as.fn.return_type);
                print(cg, " __ILC_INTERNAL_ID(return_value);\n\n");
            }
            print(cg, "// start block\n");
            for(usize i = 0; i < AS_LIST_NODE(n)->nodes.used; ++i) {
                ASTNode *node = ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(n)->nodes, i);
                gen_stmt(cg, node);
            }
            print(cg, "// end block\n\n");


            if(is_function_scope) {
                print(cg, "_fn_%s_end:\n// start defers\n", cg->current_function->name, cg->current_function->name);
                for(usize i = 0; i < arrayLength(&cg->current_function->as.fn.defers); ++i) {
                    ASTNode *n = ARRAY_GET_AS(ASTNode *, &cg->current_function->as.fn.defers, i);
                    gen_stmt(cg, n);
                }
                print(cg, "// end defers\n");
                print(cg, "return");
                if(cg->current_function->as.fn.return_type->type != TY_VOID) {
                    print(cg, " __ILC_INTERNAL_ID(return_value)");
                }
                print(cg, ";\n");
            }
            print(cg, "}\n");
            break;
        }
        case ND_IF:
            print(cg, "if(");
            gen_expr(cg, AS_CONDITIONAL_NODE(n)->condition);
            print(cg, ") ");
            gen_stmt(cg, AS_CONDITIONAL_NODE(n)->body); // The newline is added by gen_stmt:ND_BLOCK.
            if(AS_CONDITIONAL_NODE(n)->else_) {
                print(cg, "else ");
                gen_stmt(cg, AS_CONDITIONAL_NODE(n)->else_); // The newline is added by gen_stmt:ND_BLOCK.
            }
            break;
        case ND_WHILE_LOOP:
            print(cg, "while(");
            gen_expr(cg, AS_LOOP_NODE(n)->condition);
            print(cg, ") ");
            gen_stmt(cg, AS_LOOP_NODE(n)->body); // The newline is added by gen_stmt:ND_BLOCK.
            break;
        case ND_ASSIGN:
            gen_variable(AS_BINARY_NODE(n)->lhs, cg);
            print(cg, " = ");
            gen_expr(cg, AS_BINARY_NODE(n)->rhs);
            print(cg, ";\n");
            break;
        case ND_VAR_DECL:
            gen_variable(n, cg);
            print(cg, ";\n");
            break;
        default:
            gen_expr(cg, n);
            print(cg, ";\n");
            break;
    }
}

static void gen_variable(ASTNode *variable, Codegen *cg) {
    if(NODE_IS(variable, ND_VARIABLE)) {
        ASTObj *var = AS_OBJ_NODE(variable)->obj;
        print(cg, "%s", var->name);
    } else if(NODE_IS(variable, ND_VAR_DECL)) {
        ASTObj *var = AS_OBJ_NODE(variable)->obj;
        gen_type(cg, var->data_type);
        print(cg, " %s", var->name);
    } else if(NODE_IS(variable, ND_PROPERTY_ACCESS)) {
        Array stack;
        arrayInit(&stack);
        while(NODE_IS(AS_BINARY_NODE(variable)->lhs, ND_PROPERTY_ACCESS)) {
            arrayPush(&stack, (void *)AS_BINARY_NODE(variable)->rhs);
            variable = AS_BINARY_NODE(variable)->lhs;
        }
        arrayPush(&stack, (void *)AS_BINARY_NODE(variable)->rhs);
        arrayPush(&stack, (void *)AS_BINARY_NODE(variable)->lhs);
        while(arrayLength(&stack) > 0) {
            ASTNode *n = ARRAY_POP_AS(ASTNode *, &stack);
            if(NODE_IS(n, ND_DEREF)) {
                print(cg, "(*%s)", AS_OBJ_NODE(AS_UNARY_NODE(n)->operand)->obj->name);
            } else {
                VERIFY(NODE_IS(n, ND_VARIABLE));
                print(cg, "%s", AS_OBJ_NODE(n)->obj->name);
            }
            if(arrayLength(&stack) > 0) {
                print(cg, ".");
            }
        }
        arrayFree(&stack);
    } else if(NODE_IS(variable, ND_DEREF)) { // FIXME: unduplicate with gen_expr:ND_DEREF.
        print(cg, "(*");
        gen_variable(AS_UNARY_NODE(variable)->operand, cg);
        print(cg, ")");
    } else {
        UNREACHABLE();
    }
}

static void gen_function_def(Codegen *cg, ASTObj *fn) {
    gen_type(cg, fn->as.fn.return_type);
    print(cg, " %s(", fn->name);
    for(usize i = 0; i < fn->as.fn.parameters.used; ++i) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &fn->as.fn.parameters, i);
        gen_type(cg, param->data_type);
        print(cg, " %s", param->name);
        if(i + 1 < fn->as.fn.parameters.used) {
            print(cg, ", ");
        }
    }
    print(cg, ") ");
    gen_stmt(cg, AS_NODE(fn->as.fn.body));
    // defers are generated by gen_stmt:ND_BLOCK
}

static void gen_struct(Codegen *cg, ASTObj *s) {
    print(cg, "typedef struct %s {\n", s->name);
    for(usize i = 0; i < s->as.structure.fields.used; ++i) {
        ASTObj *member = ARRAY_GET_AS(ASTObj *, &s->as.structure.fields, i);
        print(cg, "    "); // 4 spaces
        gen_type(cg, member->data_type);
        print(cg, " %s;\n", member->name);
    }
    print(cg, "} %s;\n", s->name);
}

static void object_callback(void *object, void *codegen) {
    ASTObj *obj = (ASTObj *)object;
    Codegen *cg = (Codegen *)codegen;

    switch(obj->type) {
        case OBJ_VAR:
        case OBJ_EXTERN_FN:
            // nothing
            break;
        case OBJ_FN:
            cg->current_function = obj;
            gen_function_def(cg, obj);
            cg->current_function = NULL;
            print(cg, "\n");
            break;
        case OBJ_STRUCT:
            gen_struct(cg, obj);
            print(cg, "\n");
            break;
        default:
            UNREACHABLE();
    }
}

static void gen_function_predecl(Codegen *cg, ASTString name, Array *parameters, Type *return_type) {
    gen_type(cg, return_type);
    print(cg, " %s(", name);
    for(usize i = 0; i < parameters->used; ++i) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, parameters, i);
        gen_type(cg, param->data_type);
        if(i + 1 < parameters->used) {
            print(cg, ", ");
        }
    }
    print(cg, ");\n");
}

static void object_predecl_callback(void *object, void *codegen) {
    ASTObj *obj = (ASTObj *)object;
    Codegen *cg = (Codegen *)codegen;

    switch(obj->type) {
        case OBJ_VAR:
            // nothing
            break;
        case OBJ_FN:
            gen_function_predecl(cg, obj->name, &obj->as.fn.parameters, obj->as.fn.return_type);
            break;
        case OBJ_STRUCT:
            print(cg, "typedef struct %s %s;\n", obj->name, obj->name);
            break;
        case OBJ_EXTERN_FN:
            print(cg, "// source: '%s'\n", obj->as.extern_fn.source_attr->as.source);
            print(cg, "extern ");
            gen_function_predecl(cg, obj->name, &obj->as.extern_fn.parameters, obj->as.extern_fn.return_type);
            break;
        default:
            UNREACHABLE();
    }
}

static void gen_fn_types(TableItem *item, bool is_last, void *codegen) {
    UNUSED(is_last);
    Codegen *cg = (Codegen *)codegen;
    Type *ty = (Type *)item->key;
    if(ty->type != TY_FN) {
        return;
    }

    tableSet(&cg->fn_type_names, (void *)ty->name, (void *)astProgramAddString(cg->program, stringFormat("fn%d", cg->fn_typename_counter)));
    print(cg, "typedef ");
    gen_type(cg, ty->as.fn.return_type);
    print(cg, " (*fn%d)(", cg->fn_typename_counter++);
    for(usize i = 0; i < ty->as.fn.parameter_types.used; ++i) {
        Type *param_ty = ARRAY_GET_AS(Type *, &ty->as.fn.parameter_types, i);
        gen_type(cg, param_ty);
        if(i + 1 < ty->as.fn.parameter_types.used) {
            print(cg, ", ");
        }
    }
    print(cg, ");\n");
}

static void global_variable_callback(void *variable, void *codegen) {
    ASTNode *g = AS_NODE(variable);
    Codegen *cg = (Codegen *)codegen;
    if(NODE_IS(g, ND_ASSIGN)) {
        gen_stmt(cg, g);
    } else {
        gen_variable(g, cg);
        print(cg, ";\n");
    }
}

static void module_callback(void *module, void *codegen) {
    ASTModule *m = (ASTModule *)module;
    Codegen *cg = (Codegen *)codegen;

    print(cg, "/* module %s */\n", m->name);
    print(cg, "\n// pre-declarations:\n");
    arrayMap(&m->scope->objects, object_predecl_callback, codegen);
    print(cg, "// function types:\n");
    tableMap(&m->scope->types, gen_fn_types, codegen);
    print(cg, "\n// global variables:\n");
    arrayMap(&m->globals, global_variable_callback, codegen);
    print(cg, "\n// objects:\n");
    arrayMap(&m->scope->objects, object_callback, codegen);
    print(cg, "/* end module '%s' */\n\n", m->name);
}

static void gen_header(Codegen *cg) {
    print(cg, "// File generated by ilc\n\n");
    print(cg, "#include <stdint.h>\n#include <stdbool.h>\n\n");
    print(cg, "// primitive types:\n");
    print(cg, "typedef int32_t i32;\ntypedef uint32_t u32;\ntypedef const char *str;\n\n");
    print(cg, "// macros\n#define __ILC_INTERNAL_ID(id) ____ilc_internal_##id\n\n");
}

bool codegenGenerate(FILE *output, ASTProgram *prog) {
    Codegen cg;
    codegen_init(&cg, prog);
    gen_header(&cg);
    arrayMap(&prog->modules, module_callback, (void *)&cg);
    fprintf(output, "%s", cg.output_buffer);
    codegen_free(&cg);
    return true;
}
