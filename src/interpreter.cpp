
#include "interpreter.h"
#include "ast.h"
#include <sstream>
#include <iostream>

using namespace std;

bool is_assignment_operator(string op) {
    return op == "+="
            || op == "-="
            || op == "*="
            || op == "/="
            || op == "%="
            || op == "&="
            || op == "|="
            || op == "^="
            || op == "<<="
            || op == ">>="
            || op == "=";
}

AssignmentOperator get_assignment_operator(string op) {
#define ASSIGN_OP(op_) if (op == #op_ "=") return [](RuntimeValue &left, RuntimeValue &right) { return left op_ right; };
    ASSIGN_OP(+)
    ASSIGN_OP(-)
    ASSIGN_OP(*)
    ASSIGN_OP(/)
    ASSIGN_OP(%)
    ASSIGN_OP(&)
    ASSIGN_OP(|)
    ASSIGN_OP(^)
    ASSIGN_OP(<<)
    ASSIGN_OP(>>)
#undef ASSIGN_OP
    if (op == "=") return [](RuntimeValue &left, RuntimeValue &right) { return right.copy(); };
    return nullptr;
}

string ArrayRuntimeValue::to_string() {
    stringstream ret;
    ret << "[";
    int i;
    for (i = 0; i < m_values->size() && i < 5; i++) {
        if (i != 0)
            ret << ", ";
        ret << (*m_values)[i]->to_string();
    }
    if (i < m_values->size()) {
        ret << ", ... (" << (m_values->size() - i) << " more)";
    }
    ret << "]";
    return ret.str();
}

string StructRuntimeValue::to_string() {
    stringstream ret;
    ret << "{";
    int count = 0;
    std::map<std::string, spRuntimeValue>::iterator it;
    for (it = m_values->begin(); it != m_values->end(); ++it) {
        if (count != 0)
            ret << ", ";
        ret << it->first << " = " << it->second->to_string();
        count++;
        if (count == 5)
            break;
    }
    if (it != m_values->end()) {
        ret << ", ... (" << (m_values->size() - count) << " more)";
    }
    ret << "}";
    return ret.str();
}

void InterpreterContext::execute_statement(Statement &statement) {
    executing_statements.push_back(&statement);
    statement.execute(*this);
    if (is_broken() || is_continued()) {
        executing_statements_to_remove++;
    } else {
        executing_statements.pop_back();
    }
}

spRuntimeValue InterpreterContext::evaluate_expression(Expression &expression) {
    evaluating_expressions.push_back(&expression);
    auto ret = expression.evaluate(*this);
    evaluating_expressions.pop_back();
    return ret;
}

void InterpreterContext::execute_struct(Struct &type, spStructRuntimeValue &runtime_value) {
    push_scope();
    m_frames.back().current_struct = runtime_value;
    for (upStatement &statement : type.m_body) {
        execute_statement(*statement);
    }
    pop_scope();
}

void InterpreterContext::handle_break() {
    broken = false;
    for (int i = 0; i < executing_statements_to_remove; i++)
        executing_statements.pop_back();
    executing_statements_to_remove = 0;
}

void InterpreterContext::handle_continue() {
    continued = false;
    for (int i = 0; i < executing_statements_to_remove; i++)
        executing_statements.pop_back();
    executing_statements_to_remove = 0;
}

Struct*& InterpreterContext::declare_struct(string name) {
    return m_struct_types[name];
}

Struct& InterpreterContext::resolve_struct(string name) {
    auto it = m_struct_types.find(name);
    if (it == m_struct_types.end())
        throw "Could not resolve struct " + name;
    return *it->second;
}

spRuntimeValue& InterpreterContext::declare_variable(string name) {
    StackFrame &frame = m_frames.back();
    if (frame.vars.find(name) != frame.vars.end())
        throw "Redeclaration of variable " + name;
    return frame.vars[name];
}

spRuntimeValue& InterpreterContext::resolve_variable(string name) {
    for (auto frame_itr = m_frames.rbegin(); frame_itr != m_frames.rend(); ++frame_itr) {
        StackFrame &frame = *frame_itr;
        if (frame.current_struct) {
            auto itr = frame.current_struct->m_values->find(name);
            if (itr != frame.current_struct->m_values->end())
                return itr->second;
        }
        auto itr = frame.vars.find(name);
        if (itr != frame.vars.end()) {
            return itr->second;
        }
    }
    throw "Could not resolve variable " + name;
}

spRuntimeValue& InterpreterContext::define_struct_ref(string name) {
    for (auto frame_itr = m_frames.rbegin(); frame_itr != m_frames.rend(); ++frame_itr) {
        if (frame_itr->current_struct) {
            if (frame_itr->current_struct->m_values->find(name) != frame_itr->current_struct->m_values->end())
                throw "Redeclaration of struct reference " + name;
            return (*frame_itr->current_struct->m_values)[name];
        }
    }
    throw "Assertion failed: we should always be inside a struct at some level";
}

spStructRuntimeValue InterpreterContext::begin_struct_ref(string name, map<StructRefModifierType, shared_ptr<void>> &modifiers) {
    // TODO: do stuff here
    return make_shared<StructRuntimeValue>();
}

void InterpreterContext::end_struct_ref() {
    // TODO: do stuff here
}

void InterpreterContext::execute_builtin_function(string name, vector<RuntimeValue *> &args) {
    // TODO: do stuff here
}

spRuntimeValue InterpreterContext::evaluate_builtin_function(string name, vector<RuntimeValue *> &args) {
    // TODO: do stuff here
    return make_shared<IntegerRuntimeValue>(0);
}

void InterpreterContext::push_scope() {
    m_frames.emplace_back(StackFrame());
}

void InterpreterContext::pop_scope() {
    m_frames.pop_back();
}

void declare_builtin_variables(InterpreterContext &context) {
    context.declare_variable("std::little_endian") = make_shared<IntegerRuntimeValue>(0);
    context.declare_variable("std::big_endian") = make_shared<IntegerRuntimeValue>(1);
}

void InterpreterContext::handle_error(string error, ErrorHandler error_handler) {
    error_handler(error, executing_statements, evaluating_expressions);
}

void execute(vector<upStatement> &statements, ErrorHandler error_handler) {
    InterpreterContext context;
    context.push_scope();
    context.m_frames.back().current_struct = make_shared<StructRuntimeValue>();
    declare_builtin_variables(context);

    try {
        for (upStatement &statement : statements) {
            context.execute_statement(*statement);
            if (context.is_broken()) {
                throw "break statement not handled";
            }
            if (context.is_continued()) {
                throw "continue statement not handled";
            }
        }
    } catch (const char *error) {
        context.handle_error(string(error), error_handler);
    } catch (string &error) {
        context.handle_error(error, error_handler);
    }

    context.pop_scope();
}
