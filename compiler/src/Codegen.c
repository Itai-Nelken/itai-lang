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
    Table locals_already_declared; // Table<ASTString, void>
    Table fn_type_names; // Table<ASTString, ASTString> (ilc typename, C typename)
    u32 fn_typename_counter;
} Codegen;

static void codegen_init(Codegen *cg, ASTProgram *prog) {
    cg->output_buffer = stringNew(1024); // start with a 1kb buffer.
    cg->program = prog;
    tableInit(&cg->locals_already_declared, NULL, NULL);
    tableInit(&cg->fn_type_names, NULL, NULL);
    cg->fn_typename_counter = 0;
}

static void codegen_free(Codegen *cg) {
    stringFree(cg->output_buffer);
    cg->program = NULL;
    tableFree(&cg->locals_already_declared);
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

static bool obj_is_local_in_current_function(Codegen *cg, ASTObj *var) {
    for(usize i = 0; i < cg->current_function->as.fn.locals.used; ++i) {
        ASTObj *obj = ARRAY_GET_AS(ASTObj *, &cg->current_function->as.fn.locals, i);
        if(obj->type == OBJ_VAR && obj == var) {
            return true;
        }
    }
    return false;
}

static void gen_type(Codegen *cg, Type *ty) {
    if(!ty) {
        print(cg, "void");
        return;
    }
    if(ty->type == TY_FN) {
        TableItem *item;
        VERIFY((item = tableGet(&cg->fn_type_names, (void *)ty->name)) != NULL);
        print(cg, "%s", (ASTString)item->value);
    } else {
        print(cg, "%s", ty->name);
    }
}

static const char *binary_op_str(ASTNodeType node_type) {
    switch(node_type) {
        case ND_ADD: return "+";
        case ND_SUBTRACT: return "-";
        case ND_MULTIPLY: return "*";
        case ND_DIVIDE: return "/";
        default:
            UNREACHABLE();
    }
}

static void gen_expr(Codegen *cg, ASTNode *expr) {
    print(cg, "(");
    switch(expr->node_type) {
        case ND_NUMBER_LITERAL:
            print(cg, "%lu", AS_LITERAL_NODE(expr)->value.as.number);
            break;
        // Binary nodes
        case ND_ASSIGN:
            print(cg, "%s = ", AS_OBJ_NODE(AS_BINARY_NODE(expr)->lhs)->obj->name);
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
        case ND_ADD:
        case ND_SUBTRACT:
        case ND_MULTIPLY:
        case ND_DIVIDE:
            gen_expr(cg, AS_BINARY_NODE(expr)->lhs);
            print(cg, "%s", binary_op_str(expr->node_type));
            gen_expr(cg, AS_BINARY_NODE(expr)->rhs);
            break;
        // Unary nodes
        case ND_NEGATE:
            print(cg, "-");
            gen_expr(cg, AS_UNARY_NODE(expr)->operand);
            break;
        case ND_VARIABLE:
        case ND_FUNCTION:
            print(cg, "%s", AS_OBJ_NODE(expr)->obj->name);
            break;
        default:
            UNREACHABLE();
    }
    print(cg, ")");
}

static void variable_callback(void *variable, void *codegen);
static void gen_stmt(Codegen *cg, ASTNode *n) {
    switch(n->node_type) {
        case ND_RETURN:
            print(cg, "return");
            if(AS_UNARY_NODE(n)->operand) {
                print(cg, " ");
                gen_expr(cg, AS_UNARY_NODE(n)->operand);
            }
            print(cg, ";\n");
            break;
        case ND_BLOCK:
            print(cg, "{\n");
            for(usize i = 0; i < AS_LIST_NODE(n)->nodes.used; ++i) {
                ASTNode *node = ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(n)->nodes, i);
                gen_stmt(cg, node);
            }
            print(cg, "}\n");
            break;
        case ND_IF:
            print(cg, "if(");
            gen_expr(cg, AS_CONDITIONAL_NODE(n)->condition);
            print(cg, ") ");
            gen_stmt(cg, AS_CONDITIONAL_NODE(n)->body);
            if(AS_CONDITIONAL_NODE(n)->else_) {
                print(cg, "else ");
                gen_stmt(cg, AS_CONDITIONAL_NODE(n)->else_);
            }
            break;
        case ND_ASSIGN:
        case ND_VARIABLE: {
            ASTObj *obj = NODE_IS(n, ND_ASSIGN) ? AS_OBJ_NODE(AS_BINARY_NODE(n)->lhs)->obj : AS_OBJ_NODE(n)->obj;
            if(obj_is_local_in_current_function(cg, obj) && tableGet(&cg->locals_already_declared, obj->name) == NULL) {
                tableSet(&cg->locals_already_declared, obj->name, NULL);
                variable_callback((void *)n, (void *)cg);
                break;
            }
        }
        // fallthrough
        default:
            gen_expr(cg, n);
            print(cg, ";\n");
            break;
    }
}

static void variable_callback(void *variable, void *codegen) {
    ASTNode *v = AS_NODE(variable);
    Codegen *cg = (Codegen *)codegen;

    if(NODE_IS(v, ND_ASSIGN)) {
        ASTObj *var = AS_OBJ_NODE(AS_BINARY_NODE(v)->lhs)->obj;
        gen_type(cg, var->data_type);
        print(cg, " %s = ", var->name);
        gen_expr(cg, AS_BINARY_NODE(v)->rhs);
        print(cg, ";\n");
    } else if(NODE_IS(v, ND_VARIABLE)) {
        ASTObj *var = AS_OBJ_NODE(v)->obj;
        gen_type(cg, var->data_type);
        print(cg, " %s", var->name);
        if(cg->current_function && IS_NUMERIC(var->data_type)) {
            print(cg, " = 0");
        }
        print(cg, ";\n");
    } else {
        UNREACHABLE();
    }
}

static void gen_function_decl(Codegen *cg, ASTObj *fn) {
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
    tableClear(&cg->locals_already_declared, NULL, NULL);
}

static void object_callback(void *object, void *codegen) {
    ASTObj *obj = (ASTObj *)object;
    Codegen *cg = (Codegen *)codegen;

    if(obj->type == OBJ_FN) {
        cg->current_function = obj;
        gen_function_decl(cg, obj);
        cg->current_function = NULL;
    }
}

static void fn_predecl_callback(void *object, void *codegen) {
    ASTObj *obj = (ASTObj *)object;
    Codegen *cg = (Codegen *)codegen;

    if(obj->type != OBJ_FN) {
        return;
    }
    gen_type(cg, obj->as.fn.return_type);
    print(cg, " %s(", obj->name);
    for(usize i = 0; i < obj->as.fn.parameters.used; ++i) {
        ASTObj *param = ARRAY_GET_AS(ASTObj *, &obj->as.fn.parameters, i);
        gen_type(cg, param->data_type);
        if(i + 1 < obj->as.fn.parameters.used) {
            print(cg, ", ");
        }
    }
    print(cg, ");\n");
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

static void module_callback(void *module, void *codegen) {
    ASTModule *m = (ASTModule *)module;
    Codegen *cg = (Codegen *)codegen;

    print(cg, "/* module %s */\n", m->name);
    print(cg, "// function types:\n");
    tableMap(&m->types, gen_fn_types, codegen);
    print(cg, "\n// function pre-declarations:\n");
    arrayMap(&m->objects, fn_predecl_callback, codegen);
    print(cg, "\n// global variables:\n");
    arrayMap(&m->globals, variable_callback, codegen);
    print(cg, "\n// objects:\n");
    arrayMap(&m->objects, object_callback, codegen);
    print(cg, "/* end module '%s' */\n\n", m->name);
}

static void gen_header(Codegen *cg) {
    print(cg, "// File generated by ilc\n\n");
    print(cg, "#include <stdint.h>\n#include <stdbool.h>\n\n");
    print(cg, "// primitive types:\n");
    print(cg, "typedef int32_t i32;\ntypedef uint32_t u32;\n\n");
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
