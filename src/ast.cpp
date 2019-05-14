
#include <limits>
#include <sstream>
#include "ast.h"

using namespace std;

void BlockStatement::execute(InterpreterContext &context) {
    context.push_scope();
    for (upStatement &statement : m_statements) {
        context.execute_statement(*statement);
        if (context.is_broken() || context.is_continued()) break;
    }
    context.pop_scope();
}

void IfStatement::execute(InterpreterContext &context) {
    if (context.evaluate_expression(*m_condition)->to_boolean()) {
        context.execute_statement(*m_if_true);
    } else if (m_if_false) {
        context.execute_statement(*m_if_false);
    }
}

void WhileStatement::execute(InterpreterContext &context) {
    while (context.evaluate_expression(*m_condition)->to_boolean()) {
        context.execute_statement(*m_body);
        if (context.is_broken()) {
            context.handle_break();
            break;
        }
        if (context.is_continued()) context.handle_continue();
    }
}

void DoWhileStatement::execute(InterpreterContext &context) {
    do {
        context.execute_statement(*m_body);
        if (context.is_broken()) {
            context.handle_break();
            break;
        }
        if (context.is_continued()) context.handle_continue();
    } while (context.evaluate_expression(*m_condition)->to_boolean());
}

void SwitchStatement::execute(InterpreterContext &context) {
    spRuntimeValue to_match = context.evaluate_expression(*m_value);
    int target = -1;

    for (pair<upExpression, int> &case_label : m_case_labels) {
        spRuntimeValue value = context.evaluate_expression(*case_label.first);
        if (to_match == value) {
            target = case_label.second;
            break;
        }
    }
    if (target == -1)
        target = m_default_label;

    context.push_scope();
    for (int i = target; i < m_statements.size(); i++) {
        context.execute_statement(*m_statements[i]);
        if (context.is_broken()) {
            context.handle_break();
            break;
        }
        if (context.is_continued()) {
            break;
        }
    }
    context.pop_scope();
}

void BreakStatement::execute(InterpreterContext &context) {
    context.do_break();
}

void ContinueStatement::execute(InterpreterContext &context) {
    context.do_continue();
}

void VarDeclStatement::execute(InterpreterContext &context) {
    for (pair<upVarDecl, upExpression> &decl : m_declarations) {
        spRuntimeValue &var_handle = context.declare_variable(decl.first->m_name);

        spRuntimeValue value;
        if (!decl.first->m_dimensions.empty())
            value = make_shared<ArrayRuntimeValue>(decl.first->m_dimensions.size());
        else if (decl.second == nullptr)
            value = nullptr;
        else
            value = context.evaluate_expression(*decl.second);

        var_handle = value;
    }
}

void AssignmentStatement::execute(InterpreterContext &context) {
    spRuntimeValue &var_handle = context.resolve_variable(m_name);

    if (var_handle == nullptr && !m_is_assign_only)
        throw "Reference to undefined variable";

    spRuntimeValue value = context.evaluate_expression(*m_value);

    if (m_is_assign_only)
        var_handle = value;
    else
        var_handle = m_operator(*var_handle, *value);
}

void BuiltinFunctionStatement::execute(InterpreterContext &context) {
    vector<RuntimeValue*> args(m_args.size());
    for (int i = 0; i < m_args.size(); i++)
        args[i] = &*context.evaluate_expression(*m_args[i]);

    context.execute_builtin_function(m_name, args);
}

spRuntimeValue define_struct_ref_array(
        Struct &type,
        string &name,
        map<StructRefModifierType, shared_ptr<void>> &modifiers,
        vector<int> &dimensions,
        int dim_index,
        vector<int> &indices,
        InterpreterContext &context) {

    if (dim_index == dimensions.size()) {
        stringstream element_name;
        element_name << name;
        for (int index : indices)
            element_name << "[" << index << "]";
        spStructRuntimeValue struct_ref = context.begin_struct_ref(element_name.str(), modifiers);
        context.execute_struct(type, struct_ref);
        context.end_struct_ref();
        return struct_ref;
    }

    auto array = make_shared<ArrayRuntimeValue>(dimensions[dim_index]);
    for (int index = 0; index < dimensions[dim_index]; index++) {
        indices[dim_index] = index;
        (*array->m_values)[index] = define_struct_ref_array(type, name, modifiers, dimensions, dim_index + 1, indices, context);
    }
    return array;
}

void StructRefStatement::execute(InterpreterContext &context) {
    Struct &type = m_type->resolve(context);

    for (upVarDecl &decl : m_values) {
        if (decl->m_dimensions.empty()) {
            spStructRuntimeValue struct_ref = context.begin_struct_ref(decl->m_name, m_modifiers);
            context.execute_struct(type, struct_ref);
            context.end_struct_ref();
            context.define_struct_ref(decl->m_name) = struct_ref;
        } else {

            vector<int> dimensions(decl->m_dimensions.size());
            for (int i = 0; i < dimensions.size(); i++) {
                spRuntimeValue dim = context.evaluate_expression(*decl->m_dimensions[i]);
                if (dim->m_type != RuntimeType::INT) throw "Array dimension must be an integer";
                int32_t dim_val = dynamic_cast<IntegerRuntimeValue&>(*dim).m_value;
                if (dim_val < 0) throw "Negative array size";
                if (dim_val >= numeric_limits<int>::max()) throw "Array size too large";
                dimensions[i] = dim_val;
            }

            vector<int> indices(dimensions.size());

            spRuntimeValue struct_ref = define_struct_ref_array(type, decl->m_name, m_modifiers, dimensions, 0, indices, context);
            context.define_struct_ref(decl->m_name) = struct_ref;
        }
    }
}


spRuntimeValue LiteralExpression::evaluate(InterpreterContext &context) {
    return m_value;
}

spRuntimeValue VarReferenceExpression::evaluate(InterpreterContext &context) {
    spRuntimeValue &var_handle = context.resolve_variable(m_name);
    if (var_handle == nullptr)
        throw "Reference to undefined variable";
    return var_handle;
}

spRuntimeValue BinaryOperatorExpression::evaluate(InterpreterContext &context) {
    spRuntimeValue lhs = context.evaluate_expression(*m_left);
    return m_operator(*lhs, *m_right, context);
}

spRuntimeValue UnaryOperatorExpression::evaluate(InterpreterContext &context) {
    spRuntimeValue value = context.evaluate_expression(*m_expr);
    return m_operator(*value);
}

spRuntimeValue FieldAccessExpression::evaluate(InterpreterContext &context) {
    spRuntimeValue owner = context.evaluate_expression(*m_struct);
    if (owner->m_type != RuntimeType::STRUCT)
        throw "Cannot get field from non-struct type";
    auto fields = dynamic_cast<StructRuntimeValue&>(*owner).m_values;
    auto itr = fields->find(m_field);
    if (itr == fields->end())
        throw "Cannot find field in struct";
    return itr->second;
}

spRuntimeValue PreIncrementExpression::evaluate(InterpreterContext &context) {
    spRuntimeValue &var_handle = context.resolve_variable(m_var);
    if (var_handle == nullptr)
        throw "Reference to undefined variable";
    IntegerRuntimeValue delta(m_delta);
    var_handle = *var_handle + delta;
    return var_handle;
}

spRuntimeValue PostIncrementExpression::evaluate(InterpreterContext &context) {
    spRuntimeValue &var_handle = context.resolve_variable(m_var);
    if (var_handle == nullptr)
        throw "Reference to undefined variable";
    IntegerRuntimeValue delta(m_delta);
    spRuntimeValue temp = *var_handle + delta;
    spRuntimeValue ret = var_handle->copy();
    var_handle = temp;
    return ret;
}

spRuntimeValue BuiltinFunctionExpression::evaluate(InterpreterContext &context) {
    vector<RuntimeValue*> args(m_args.size());
    for (int i = 0; i < m_args.size(); i++)
        args[i] = &*context.evaluate_expression(*m_args[i]);

    return context.evaluate_builtin_function(m_name, args);
}

Struct& DeclaringStructRef::resolve(InterpreterContext &context) {
    if (m_declaration->m_name) {
        context.declare_struct(*m_declaration->m_name) = &*m_declaration;
    }
    return *m_declaration;
}

Struct& ResolvingStructRef::resolve(InterpreterContext &context) {
    return context.resolve_struct(m_name);
}
