
#include "interpreter.h"
#include "ast.h"

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

void InterpreterContext::execute_statement(Statement &statement) {
    statement.execute(*this);
}

spRuntimeValue InterpreterContext::evaluate_expression(Expression &expression) {
    return expression.evaluate(*this);
}

void InterpreterContext::execute_struct(Struct &type, spStructRuntimeValue &runtime_value) {
    push_scope();
    m_frames.back().current_struct = runtime_value;
    for (upStatement &statement : type.m_body) {
        execute_statement(*statement);
    }
    pop_scope();
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
        throw "Redeclaration of variable";
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
                throw "Redeclaration of struct reference";
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

void execute(vector<upStatement> &statements) {
    InterpreterContext context;
    context.push_scope();
    context.m_frames.back().current_struct = make_shared<StructRuntimeValue>();
    declare_builtin_variables(context);

    for (upStatement &statement : statements) {
        context.execute_statement(*statement);
        if (context.is_broken()) {
            throw "break statement not handled";
        }
        if (context.is_continued()) {
            throw "continue statement not handled";
        }
    }

    context.pop_scope();
}
