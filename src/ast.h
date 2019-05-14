
#ifndef DECODE_BIN_AST_H
#define DECODE_BIN_AST_H

#include <map>
#include <memory>
#include <utility>
#include <vector>
#include "interpreter.h"

enum class StructType {
    STRUCT, ENUM, FLAGS, UNION, CHOOSE
};
enum class StructModifierType {
    ARRAY_VALUE, ELEMENT_TYPE
};
enum class StructRefModifierType {
    HIDE
};

class Statement {
public:
    virtual void execute(InterpreterContext &context) = 0;
};
typedef std::unique_ptr<Statement> upStatement;

class Expression {
public:
    virtual spRuntimeValue evaluate(InterpreterContext &context) = 0;
};
typedef std::unique_ptr<Expression> upExpression;

class VarDecl {
public:
    std::string m_name;
    std::vector<upExpression> m_dimensions;
};
typedef std::unique_ptr<VarDecl> upVarDecl;

class BlockStatement : public Statement {
public:
    std::vector<upStatement> m_statements;
    void execute(InterpreterContext &context) override;
};

class IfStatement : public Statement {
public:
    upExpression m_condition;
    upStatement m_if_true;
    upStatement m_if_false;
    void execute(InterpreterContext &context) override;
};

class WhileStatement : public Statement {
public:
    upExpression m_condition;
    upStatement m_body;
    void execute(InterpreterContext &context) override;
};

class DoWhileStatement : public Statement {
public:
    upStatement m_body;
    upExpression m_condition;
    void execute(InterpreterContext &context) override;
};

class SwitchStatement : public Statement {
public:
    upExpression m_value;
    std::vector<upStatement> m_statements;
    std::vector<std::pair<upExpression, int>> m_case_labels;
    int m_default_label;
    void execute(InterpreterContext &context) override;
};

class BreakStatement : public Statement {
    void execute(InterpreterContext &context) override;
};

class ContinueStatement : public Statement {
    void execute(InterpreterContext &context) override;
};

class EmptyStatement : public Statement {
    void execute(InterpreterContext &context) override {}
};

class VarDeclStatement : public Statement {
public:
    std::vector<std::pair<upVarDecl, upExpression>> m_declarations;
    void execute(InterpreterContext &context) override;
};

class AssignmentStatement : public Statement {
public:
    std::string m_name;
    AssignmentOperator m_operator;
    upExpression m_value;
    bool m_is_assign_only;
    void execute(InterpreterContext &context) override;
};

class BuiltinFunctionStatement : public Statement {
public:
    std::string m_name;
    std::vector<upExpression> m_args;
    void execute(InterpreterContext &context) override;
};

class Struct;
typedef std::unique_ptr<Struct> upStruct;

class StructRef {
public:
    virtual Struct& resolve(InterpreterContext &context) = 0;
};
typedef std::unique_ptr<StructRef> upStructRef;

class DeclaringStructRef : public StructRef {
public:
    upStruct m_declaration;
    Struct& resolve(InterpreterContext &context) override;
};
class ResolvingStructRef : public StructRef {
public:
    std::string m_name;
    Struct& resolve(InterpreterContext &context) override;
};

class Struct {
public:
    StructType m_type;
    std::map<StructModifierType, std::shared_ptr<void>> m_modifiers;
    std::unique_ptr<std::string> m_name;
    std::vector<upStatement> m_body;
};

class StructRefStatement : public Statement {
public:
    upStructRef m_type;
    std::map<StructRefModifierType, std::shared_ptr<void>> m_modifiers;
    std::vector<upVarDecl> m_values;
    void execute(InterpreterContext &context) override;
};


class LiteralExpression : public Expression {
public:
    spRuntimeValue m_value;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

class VarReferenceExpression : public Expression {
public:
    std::string m_name;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

class BinaryOperatorExpression : public Expression {
public:
    upExpression m_left;
    upExpression m_right;
    BinaryOperator m_operator;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

class UnaryOperatorExpression : public Expression {
public:
    upExpression m_expr;
    UnaryOperator m_operator;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

class FieldAccessExpression : public Expression {
public:
    upExpression m_struct;
    std::string m_field;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

class PreIncrementExpression : public Expression {
public:
    std::string m_var;
    int m_delta;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

class PostIncrementExpression : public Expression {
public:
    std::string m_var;
    int m_delta;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

class BuiltinFunctionExpression : public Expression {
public:
    std::string m_name;
    std::vector<upExpression> m_args;
    spRuntimeValue evaluate(InterpreterContext &context) override;
};

#endif //DECODE_BIN_AST_H
