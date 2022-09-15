#include <stdbool.h>
#include "common.h"
#include "ast.h"
#include "Codegenerator.h"
#include "codegenerators/c_codegen.h"

typedef struct c_codegen_data {

} CCodegenData;

#define GET_DATA(cgd) ((CCodegenData *)cgd->data)
#define print(cgd, format, ...) (cgd->print((cgd), format, ##__VA_ARGS__))
#define println(cgd, format, ...) print(cgd, format "\n", ##__VA_ARGS__)

static void gen_type(CodeGeneratorData *data, SymbolID type_id) {
    SymbolID typename = data->get_type(data, type_id)->name;
    char *name = data->get_identifier(data, typename);
    print(data, "%s", name);
}

static void gen_expression(CodeGeneratorData *data, ASTNode *expr) {
    // unary and literal nodes
    switch(expr->type) {
        // ND_OBJ doesn't do anything,
        // it's only used to store metadata for variables, functions etc.
        case ND_OBJ:
            return;
        case ND_NEG:
            print(data, "-");
            gen_expression(data, AS_UNARY_NODE(expr)->operand);
            return;
        case ND_CALL:
            print(data, "%s()", data->get_identifier(data, AS_IDENTIFIER_NODE(AS_UNARY_NODE(expr)->operand)->id->id));
            return;
        case ND_NUMBER:
            print(data, "%d", AS_NUMBER_NODE(expr)->value.as.int64);
            return;
        case ND_VAR:
            print(data, "%s", data->get_identifier(data, AS_IDENTIFIER_NODE(AS_BINARY_NODE(expr)->left)->id->id));
            return;
        case ND_IDENTIFIER:
            print(data, "%s", data->get_identifier(data, AS_IDENTIFIER_NODE(expr)->id->id));
            return;
        default:
            break;
    }

    // binary nodes
    gen_expression(data, AS_BINARY_NODE(expr)->left);
    switch(expr->type) {
        case ND_ADD:
            print(data, " + ");
            break;
        case ND_SUB:
            print(data, " - ");
            break;
        case ND_MUL:
            print(data, " * ");
            break;
        case ND_DIV:
            print(data, " / ");
            break;
        case ND_EQ:
            print(data, " == ");
            break;
        case ND_NE:
            print(data, " != ");
            break;
        case ND_ASSIGN:
            print(data, " = ");
            break;
        default:
            UNREACHABLE();
    }
    gen_expression(data, AS_BINARY_NODE(expr)->right);
}

static void gen_statement(CodeGeneratorData *data, ASTNode *stmt) {
    switch(stmt->type) {
        case ND_EXPR_STMT:
            gen_expression(data, AS_UNARY_NODE(stmt)->operand);
            println(data, ";");
            break;
        case ND_IF: {
            ASTConditionalNode *n = AS_CONDITIONAL_NODE(stmt);
            print(data, "if(");
            gen_expression(data, n->condition);
            print(data, ")");
            gen_statement(data, AS_NODE(n->body));
            if(n->else_) {
                print(data, " else ");
                gen_statement(data, n->else_);
            }
            print(data, "\n");
            break;
        }
        case ND_BLOCK:
            println(data, "{");
            for(usize i = 0; i < AS_LIST_NODE(stmt)->body.used; ++i) {
                gen_statement(data, ARRAY_GET_AS(ASTNode *, &AS_LIST_NODE(stmt)->body, i));
            }
            println(data, "}");
            break;
        case ND_LOOP: {
            ASTLoopNode *n = AS_LOOP_NODE(stmt);
            print(data, "for(");
            if(n->initializer) {
                gen_expression(data, n->initializer);
            }
            print(data, ";");
            gen_expression(data, n->condition);
            print(data, ";");
            if(n->increment) {
                gen_expression(data, n->increment);
            }
            print(data, ") ");
            gen_statement(data, AS_NODE(n->body));
            print(data, "\n");
            break;
        }
        case ND_RETURN:
            print(data, "return ");
            if(AS_UNARY_NODE(stmt)->operand) {
                gen_expression(data, AS_UNARY_NODE(stmt)->operand);
            }
            println(data, ";");
            break;
        default:
            UNREACHABLE();
    }
}

static void gen_var_decl(ASTVariableObj *var, CodeGeneratorData *data) {
    gen_type(data, var->header.data_type);
    print(data, " %s", data->get_identifier(data, var->header.name->id));
    if(var->initializer) {
        print(data, " = ");
        gen_expression(data, var->initializer);
    }
    println(data, ";");
}

static void gen_function(ASTFunctionObj *fn, CodeGeneratorData *data) {
    gen_type(data, fn->return_type);
    if(fn == data->_prog->entry_point) {
        print(data, " __ilc_main");
    } else {
        print(data, " %s", data->get_identifier(data, fn->header.name->id));
    }
    println(data, "() {");

    for(usize i = 0; i < fn->locals.used; ++i) {
        gen_var_decl(ARRAY_GET_AS(ASTVariableObj *, &fn->locals, i), data);
    }

    gen_statement(data, AS_NODE(fn->body));

    println(data, "}");
}

static void gen_function_predcl(ASTFunctionObj *fn, CodeGeneratorData *data) {
    gen_type(data, fn->return_type);
    if(fn == data->_prog->entry_point) {
        print(data, " __ilc_main");
    } else {
        print(data, " %s", data->get_identifier(data, fn->header.name->id));
    }
    println(data, "();");
}

#undef println
#undef print
#undef GET_DATA

bool c_codegen(ASTProgram *prog) {
    CCodegenData data = {};
    CodeGenerator cg;
    codeGeneratorInit(&cg);
    codeGeneratorSetFnCallback(&cg, gen_function);
    codeGeneratorSetPreFnCallback(&cg, gen_function_predcl);
    codeGeneratorSetGlobalCallback(&cg, gen_var_decl);
    codeGeneratorSetData(&cg, (void *)&data);

    // generate the header
    cg.cg_data.print(&cg.cg_data, "#include <stdbool.h>\n#include <stdint.h>\n");
    cg.cg_data.print(&cg.cg_data, "typedef int32_t i32;\n\n");

    bool result = codeGeneratorGenerate(&cg, prog);

    // generate the start code
    if(prog->entry_point->return_type != astProgramGetPrimitiveType(prog, TY_VOID)) {
        cg.cg_data.print(&cg.cg_data, "i32 main(void) {\n");
        cg.cg_data.print(&cg.cg_data, "return __ilc_main();\n");
    } else {
        cg.cg_data.print(&cg.cg_data, "i32 main(void) {\n");
        cg.cg_data.print(&cg.cg_data, "__ilc_main();\n");;
        cg.cg_data.print(&cg.cg_data, "return 0;\n");
    }
    cg.cg_data.print(&cg.cg_data, "}\n");

    codeGeneratorWriteOutput(&cg, stdout);
    codeGeneratorFree(&cg);
    return result;
}
