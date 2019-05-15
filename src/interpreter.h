
#ifndef DECODE_BIN_INTERPRETER_H
#define DECODE_BIN_INTERPRETER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>


class Expression;
class Statement;
class Struct;
enum class StructRefModifierType;

enum class RuntimeType {
    INT, LONG, FLOAT, DOUBLE, BOOLEAN, ARRAY, STRUCT
};
struct RuntimeValue {
public:
    RuntimeType m_type;
    explicit RuntimeValue(RuntimeType type) : m_type(type) {}
    virtual std::shared_ptr<RuntimeValue> copy() = 0;

#define DEFAULT_OPERATOR(op_) virtual std::shared_ptr<RuntimeValue> operator op_(RuntimeValue &other) { throw "Undefined operator " #op_ " for operands (" + to_string() + ", " + other.to_string() + ")"; }
    DEFAULT_OPERATOR(+)
    DEFAULT_OPERATOR(-)
    DEFAULT_OPERATOR(*)
    DEFAULT_OPERATOR(/)
    DEFAULT_OPERATOR(%)
    DEFAULT_OPERATOR(&)
    DEFAULT_OPERATOR(|)
    DEFAULT_OPERATOR(^)
    DEFAULT_OPERATOR(<<)
    DEFAULT_OPERATOR(>>)
    DEFAULT_OPERATOR(&&)
    DEFAULT_OPERATOR(||)
    DEFAULT_OPERATOR(==)
    DEFAULT_OPERATOR(!=)
    DEFAULT_OPERATOR(<)
    DEFAULT_OPERATOR(>)
    DEFAULT_OPERATOR(<=)
    DEFAULT_OPERATOR(>=)
    DEFAULT_OPERATOR([])
#undef DEFAULT_OPERATOR
#define DEFAULT_OPERATOR(op_) virtual std::shared_ptr<RuntimeValue> operator op_() { throw "Undefined operator " #op_ " for operand " + to_string(); }
    DEFAULT_OPERATOR(~)
    DEFAULT_OPERATOR(!)
    DEFAULT_OPERATOR(+)
    DEFAULT_OPERATOR(-)
#undef DEFAULT_OPERATOR

    virtual bool to_boolean() { throw "Cannot interpret " + to_string() + " as a boolean"; }
    virtual std::string to_string() = 0;
};
typedef std::shared_ptr<RuntimeValue> spRuntimeValue;

template<typename T, RuntimeType TYPE>
struct BasicRuntimeValue;

template<typename T>
struct BasicRuntimeType {};
#define BASIC_TYPE(type_, runtime_type_, name) typedef BasicRuntimeValue<type_, runtime_type_> name##RuntimeValue;\
template<>\
struct BasicRuntimeType<type_> {\
    typedef name##RuntimeValue type;\
    static constexpr RuntimeType runtime_type = runtime_type_;\
    typedef type_ cpp_type;\
};
BASIC_TYPE(int32_t, RuntimeType::INT, Integer)
BASIC_TYPE(int64_t, RuntimeType::LONG, Long)
BASIC_TYPE(float, RuntimeType::FLOAT, Float)
BASIC_TYPE(double, RuntimeType::DOUBLE, Double)
BASIC_TYPE(bool, RuntimeType::BOOLEAN, Boolean)
#undef BASIC_TYPE

template<typename T, RuntimeType TYPE>
struct BasicRuntimeValue : public RuntimeValue {
public:
    T m_value;
    explicit BasicRuntimeValue(T value) : RuntimeValue(TYPE), m_value(value) {}
    spRuntimeValue copy() override { return std::make_shared<BasicRuntimeValue<T, TYPE>>(m_value); }

    spRuntimeValue operator+(RuntimeValue &right) override;
    spRuntimeValue operator-(RuntimeValue &right) override;
    spRuntimeValue operator*(RuntimeValue &right) override;
    spRuntimeValue operator/(RuntimeValue &right) override;
    spRuntimeValue operator&&(RuntimeValue &right) override;
    spRuntimeValue operator||(RuntimeValue &right) override;
    spRuntimeValue operator==(RuntimeValue &right) override;
    spRuntimeValue operator!=(RuntimeValue &right) override;
    spRuntimeValue operator<(RuntimeValue &right) override;
    spRuntimeValue operator>(RuntimeValue &right) override;
    spRuntimeValue operator<=(RuntimeValue &right) override;
    spRuntimeValue operator>=(RuntimeValue &right) override;
    spRuntimeValue operator%(RuntimeValue &right) override;
    spRuntimeValue operator&(RuntimeValue &right) override;
    spRuntimeValue operator|(RuntimeValue &right) override;
    spRuntimeValue operator^(RuntimeValue &right) override;
    spRuntimeValue operator<<(RuntimeValue &right) override;
    spRuntimeValue operator>>(RuntimeValue &right) override;
    spRuntimeValue operator!() override;
    spRuntimeValue operator+() override;
    spRuntimeValue operator-() override;
    spRuntimeValue operator~() override;

    bool to_boolean() override { return m_value; }
    std::string to_string() override;
};

class ArrayRuntimeValue : public RuntimeValue {
    typedef std::shared_ptr<std::vector<spRuntimeValue>> values_type;
public:
    values_type m_values;

    explicit ArrayRuntimeValue(values_type values) : RuntimeValue(RuntimeType::ARRAY), m_values(std::move(values)) {}

    explicit ArrayRuntimeValue(int length) : RuntimeValue(RuntimeType::ARRAY) {
        m_values = std::make_shared<std::vector<spRuntimeValue>>(length);
    }

    spRuntimeValue operator[](RuntimeValue &other) override {
        if (other.m_type != RuntimeType::INT)
            throw "Can only index arrays with integers, " + other.to_string() + " used";
        int32_t val = dynamic_cast<IntegerRuntimeValue&>(other).m_value;
        if (val < 0 || val >= m_values->size())
            throw "Array index " + other.to_string() + " is out of bounds";
        if ((*m_values)[val])
            return (*m_values)[val];
        else
            throw "Reference to uninitialized array value";
    }

    spRuntimeValue copy() override { return std::make_shared<ArrayRuntimeValue>(m_values); }

    std::string to_string() override;
};

class StructRuntimeValue : public RuntimeValue {
    typedef std::shared_ptr<std::map<std::string, spRuntimeValue>> values_type;
public:
    values_type m_values;

    StructRuntimeValue() : RuntimeValue(RuntimeType::STRUCT) {
        m_values = std::make_shared<std::map<std::string, spRuntimeValue>>();
    }

    explicit StructRuntimeValue(values_type values) : RuntimeValue(RuntimeType::STRUCT), m_values(std::move(values)) {}

    spRuntimeValue copy() override { return std::make_shared<StructRuntimeValue>(m_values); }

    std::string to_string() override;
};
typedef std::shared_ptr<StructRuntimeValue> spStructRuntimeValue;

typedef std::function<void(std::string&, std::vector<Statement*>&, std::vector<Expression*>&)> ErrorHandler;

class InterpreterContext {
    struct StackFrame {
        std::map<std::string, spRuntimeValue> vars;
        spStructRuntimeValue current_struct;
    };
    std::map<std::string, Struct*> m_struct_types;

    bool broken = false, continued = false;

    std::vector<Statement*> executing_statements;
    std::vector<Expression*> evaluating_expressions;
    int executing_statements_to_remove = 0;
public:
    std::vector<StackFrame> m_frames;

    void execute_statement(Statement &statement);
    spRuntimeValue evaluate_expression(Expression &expression);
    void execute_struct(Struct &type, spStructRuntimeValue &runtime_value);

    void do_break() { broken = true; }
    void do_continue() { continued = true; }
    bool is_broken() { return broken; }
    bool is_continued() { return continued; }
    void handle_break();
    void handle_continue();

    // tried to make this Struct&& but the compiler didn't like it
    Struct*& declare_struct(std::string name);
    Struct& resolve_struct(std::string name);

    spRuntimeValue& declare_variable(std::string name);
    spRuntimeValue& resolve_variable(std::string name);

    spRuntimeValue& define_struct_ref(std::string name);
    spStructRuntimeValue begin_struct_ref(std::string name, std::map<StructRefModifierType, std::shared_ptr<void>> &modifiers);
    void end_struct_ref();

    void execute_builtin_function(std::string name, std::vector<RuntimeValue*> &args);
    spRuntimeValue evaluate_builtin_function(std::string name, std::vector<RuntimeValue*> &args);

    void push_scope();
    void pop_scope();

    void handle_error(std::string error, ErrorHandler error_handler);
};

void execute(std::vector<std::unique_ptr<Statement>> &statements, ErrorHandler error_handler);

typedef std::function<spRuntimeValue(RuntimeValue&)> UnaryOperator;
typedef std::function<spRuntimeValue(RuntimeValue&, Expression&, InterpreterContext&)> BinaryOperator;
typedef std::function<spRuntimeValue(RuntimeValue&, RuntimeValue&)> AssignmentOperator;

bool is_assignment_operator(std::string token);
AssignmentOperator get_assignment_operator(std::string token);

#include "interpreter.tpp"

#endif //DECODE_BIN_INTERPRETER_H
