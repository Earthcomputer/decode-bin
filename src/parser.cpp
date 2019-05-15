
#include "parser.h"
#include "util.h"
#include "interpreter.h"
#include <limits>

using namespace std;

typedef function<void(Token)> ParserErrorHandler;

class Parser {
    vector<Token> &m_tokens;
    int m_index;
    ParserErrorHandler m_error_handler;

public:
    Parser(vector<Token> &tokens, ParserErrorHandler &error_handler) : m_tokens(tokens), m_index(0), m_error_handler(error_handler) {
    }

    bool eof() {
        return m_index >= m_tokens.size();
    }

    Token peek() {
        if (eof())
            return {"", m_tokens.back().line, m_tokens.back().col + static_cast<unsigned>(m_tokens.back().value.length())};
        else
            return m_tokens[m_index];
    }

    void advance() {
        m_index++;
    }
    void backtrack() {
        m_index--;
    }

    upStatement statement() {
        Token first_token = peek();

        if (first_token.value == "{") {
            return block_statement();
        } else if (first_token.value == "if") {
            return if_statement();
        } else if (first_token.value == "while") {
            return while_statement();
        } else if (first_token.value == "do") {
            return do_while_statement();
        } else if (first_token.value == "switch") {
            return switch_statement();
        } else if (first_token.value == "break") {
            return break_statement();
        } else if (first_token.value == "continue") {
            return continue_statement();
        } else if (first_token.value == ";") {
            return empty_statement();
        } else if (first_token.value == "var") {
            return var_decl_statement();
        }

        advance();
        Token second_token = peek();
        backtrack();

        if (is_assignment_operator(second_token.value)) {
            return assignment_statement();
        } else if (first_token.value == "++" || first_token.value == "--" || second_token.value == "++" || second_token.value == "--") {
            return var_incr_statement();
        } else if (second_token.value == "(") {
            return builtin_function_statement();
        }

        return struct_ref_statement();
    }

    upStatement block_statement() {
        auto ret = make_unique<BlockStatement>(peek());
        advance(); // {
        while (peek().value != "}") {
            ret->m_statements.push_back(statement());
        }
        ret->m_end_token = peek();
        advance(); // }
        return ret;
    }

    upStatement if_statement() {
        auto ret = make_unique<IfStatement>(peek());
        advance(); // if
        if (peek().value != "(") throw peek();
        advance(); // (
        ret->m_condition = expression();
        if (peek().value != ")") throw peek();
        advance(); // )
        ret->m_if_true = statement();
        ret->m_end_token = ret->m_if_true->m_end_token;
        ret->m_if_false = nullptr;
        if (peek().value == "else") {
            advance(); // else
            ret->m_if_false = statement();
            ret->m_end_token = ret->m_if_false->m_end_token;
        }
        return ret;
    }

    upStatement while_statement() {
        auto ret = make_unique<WhileStatement>(peek());
        advance(); // while
        if (peek().value != "(") throw peek();
        advance(); // (
        ret->m_condition = expression();
        if (peek().value != ")") throw peek();
        advance(); // )
        ret->m_body = statement();
        ret->m_end_token = ret->m_body->m_end_token;
        return ret;
    }

    upStatement do_while_statement() {
        auto ret = make_unique<DoWhileStatement>(peek());
        advance(); // do
        ret->m_body = statement();
        if (peek().value != "while") throw peek();
        advance(); // while
        if (peek().value != "(") throw peek();
        advance(); // (
        ret->m_condition = expression();
        if (peek().value != ")") throw peek();
        advance(); // )
        if (peek().value != ";") throw peek();
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upStatement switch_statement() {
        auto ret = make_unique<SwitchStatement>(peek());
        advance(); // switch
        if (peek().value != "(") throw peek();
        advance(); // (
        ret->m_value = expression();
        if (peek().value != ")") throw peek();
        advance(); // )
        if (peek().value != "{") throw peek();
        advance(); // {

        bool had_default_case = false;
        for (int index = 0; peek().value != "}"; index++) {
            Token token = peek();
            if (token.value == "case") {
                advance(); // case
                ret->m_case_labels.emplace_back(make_pair(expression(), index));
                if (peek().value != ":") throw peek();
                advance(); // :
                index--;
            } else if (token.value == "default") {
                if (had_default_case) throw peek();
                advance(); // default
                if (peek().value != ":") throw peek();
                advance(); // :
                ret->m_default_label = index;
                had_default_case = true;
                index--;
            } else {
                ret->m_statements.push_back(statement());
            }
        }
        ret->m_end_token = peek();
        advance(); // }

        if (!had_default_case)
            ret->m_default_label = static_cast<int>(ret->m_statements.size());

        return ret;
    }

    upStatement break_statement() {
        auto ret = make_unique<BreakStatement>(peek());
        advance(); // break
        if (peek().value != ";") throw peek();
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upStatement continue_statement() {
        auto ret = make_unique<ContinueStatement>(peek());
        advance(); // continue
        if (peek().value != ";") throw peek();
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upStatement empty_statement() {
        auto ret = make_unique<EmptyStatement>(peek());
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upStatement var_decl_statement() {
        auto ret = make_unique<VarDeclStatement>(peek());
        advance(); // var
        while (true) {
            upVarDecl decl = var_decl();
            upExpression value = nullptr;
            if (decl->m_dimensions.empty() && peek().value == "=") {
                advance(); // =
                value = expression();
            }
            ret->m_declarations.emplace_back(make_pair(move(decl), move(value)));
            if (peek().value == ";")
                break;
            if (peek().value != ",") throw peek();
            advance(); // ,
        }
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upStatement assignment_statement() {
        auto ret = make_unique<AssignmentStatement>(peek());
        ret->m_name = peek().value;
        advance(); // name
        ret->m_operator = get_assignment_operator(peek().value);
        ret->m_is_assign_only = peek().value == "=";
        advance(); // operator
        ret->m_value = expression();
        if (peek().value != ";") throw peek();
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upStatement var_incr_statement() {
        Token begin_token = peek();
        string op, var;
        if (peek().value == "++" || peek().value == "--") {
            op = peek().value;
            advance();
            var = peek().value;
            if (!is_valid_identifier(var)) throw peek();
            advance();
        } else {
            var = peek().value;
            if (!is_valid_identifier(var)) throw peek();
            advance();
            op = peek().value;
            advance();
        }
        if (peek().value != ";") throw peek();
        Token end_token = peek();
        advance(); // ;
        // turn "i++" into "i = i + 1"
        auto ret = make_unique<AssignmentStatement>(begin_token);
        ret->m_end_token = end_token;
        ret->m_name = var;
        ret->m_operator = get_assignment_operator("=");
        ret->m_is_assign_only = true;
        auto val = make_unique<BinaryOperatorExpression>(begin_token);
        val->m_end_token = end_token;
        auto left = make_unique<VarReferenceExpression>(begin_token);
        left->m_end_token = end_token;
        left->m_name = var;
        val->m_left = move(left);
        auto right = make_unique<LiteralExpression>(begin_token);
        right->m_end_token = end_token;
        right->m_value = make_shared<IntegerRuntimeValue>(1);
        val->m_right = move(right);
        if (op == "++") val->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context) { return left + *context.evaluate_expression(right); };
        else val->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context) { return left - *context.evaluate_expression(right); };
        ret->m_value = move(val);
        return ret;
    }

    upStatement builtin_function_statement() {
        auto ret = make_unique<BuiltinFunctionStatement>(peek());
        ret->m_name = peek().value;
        if (!is_valid_identifier(ret->m_name)) throw peek();
        advance(); // name
        advance(); // (
        if (peek().value != ")") {
            while (true) {
                ret->m_args.push_back(expression());
                if (peek().value == ")")
                    break;
                if (peek().value != ",") throw peek();
                advance(); // ,
            }
        }
        advance(); // )
        if (peek().value != ";") throw peek();
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upStatement struct_ref_statement() {
        auto ret = make_unique<StructRefStatement>(peek());
        ret->m_type = struct_ref();
        while (parse_struct_ref_modifier(ret->m_modifiers))
            ;
        while (peek().value != ";") {
            ret->m_values.push_back(var_decl());
            if (peek().value == ";")
                break;
            if (peek().value != ",") throw peek();
            advance(); // ,
        }
        ret->m_end_token = peek();
        advance(); // ;
        return ret;
    }

    upVarDecl var_decl() {
        string name = peek().value;
        if (!is_valid_identifier(name)) throw peek();
        advance(); // name
        upVarDecl ret = make_unique<VarDecl>();
        ret->m_name = name;
        while (peek().value == "[") {
            advance(); // [
            ret->m_dimensions.push_back(expression());
            if (peek().value != "]") throw peek();
            advance(); // ]
        }
        return ret;
    }

    upExpression expression() {
        return expression1();
    }

#define BINARY_OPERATOR_EXPRESSION(operator_, level_, next_level_) upExpression expression##level_() { \
    upExpression expr = expression##next_level_(); \
    if (peek().value == #operator_) { \
        advance(); \
        auto ret = make_unique<BinaryOperatorExpression>(expr->m_begin_token); \
        ret->m_left = move(expr); \
        ret->m_right = expression##level_(); \
        ret->m_end_token = ret->m_right->m_end_token; \
        ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator_ *context.evaluate_expression(right);}; \
        return ret; \
    } \
    return expr; \
}
#define BINARY_OPERATOR_EXPRESSION2(operator1_, operator2_, level_, next_level_) upExpression expression##level_() { \
    upExpression expr = expression##next_level_(); \
    if (peek().value == #operator1_ || peek().value == #operator2_) { \
        string op = peek().value; \
        advance(); \
        auto ret = make_unique<BinaryOperatorExpression>(expr->m_begin_token); \
        ret->m_left = move(expr); \
        ret->m_right = expression##level_(); \
        ret->m_end_token = ret->m_right->m_end_token; \
        if (op == #operator1_) \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator1_ *context.evaluate_expression(right);}; \
        else \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator2_ *context.evaluate_expression(right);}; \
        return ret; \
    } \
    return expr; \
}
#define BINARY_OPERATOR_EXPRESSION3(operator1_, operator2_, operator3_, level_, next_level_) upExpression expression##level_() { \
    upExpression expr = expression##next_level_(); \
    if (peek().value == #operator1_ || peek().value == #operator2_ || peek().value == #operator3_) { \
        string op = peek().value; \
        advance(); \
        auto ret = make_unique<BinaryOperatorExpression>(expr->m_begin_token); \
        ret->m_left = move(expr); \
        ret->m_right = expression##level_(); \
        ret->m_end_token = ret->m_right->m_end_token; \
        if (op == #operator1_) \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator1_ *context.evaluate_expression(right);}; \
        else if (op == #operator2_) \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator2_ *context.evaluate_expression(right);}; \
        else \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator3_ *context.evaluate_expression(right);}; \
        return ret; \
    } \
    return expr; \
}
#define BINARY_OPERATOR_EXPRESSION4(operator1_, operator2_, operator3_, operator4_, level_, next_level_) upExpression expression##level_() { \
    upExpression expr = expression##next_level_(); \
    if (peek().value == #operator1_ || peek().value == #operator2_ || peek().value == #operator3_ || peek().value == #operator4_) { \
        string op = peek().value; \
        advance(); \
        auto ret = make_unique<BinaryOperatorExpression>(expr->m_begin_token); \
        ret->m_left = move(expr); \
        ret->m_right = expression##level_(); \
        ret->m_end_token = ret->m_right->m_end_token; \
        if (op == #operator1_) \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator1_ *context.evaluate_expression(right);}; \
        else if (op == #operator2_) \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator2_ *context.evaluate_expression(right);}; \
        else if (op == #operator3_) \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator3_ *context.evaluate_expression(right);}; \
        else \
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){return left operator4_ *context.evaluate_expression(right);}; \
        return ret; \
    } \
    return expr; \
}
    BINARY_OPERATOR_EXPRESSION(||, 1, 2)
    BINARY_OPERATOR_EXPRESSION(&&, 2, 3)
    BINARY_OPERATOR_EXPRESSION(|, 3, 4)
    BINARY_OPERATOR_EXPRESSION(^, 4, 5)
    BINARY_OPERATOR_EXPRESSION(&, 5, 6)
    BINARY_OPERATOR_EXPRESSION2(==, !=, 6, 7)
    BINARY_OPERATOR_EXPRESSION4(<, <=, >, >=, 7, 8)
    BINARY_OPERATOR_EXPRESSION2(<<, >>, 8, 9)
    BINARY_OPERATOR_EXPRESSION2(+, -, 9, 10)
    BINARY_OPERATOR_EXPRESSION3(*, /, %, 10, 11)

    upExpression expression11() {
        if (peek().value == "+" || peek().value == "-" || peek().value == "!" || peek().value == "~") {
            string op = peek().value;
            auto ret = make_unique<UnaryOperatorExpression>(peek());
            advance(); // op
            if (op == "+") ret->m_operator = [](RuntimeValue &val){return +val;};
            else if (op == "-") ret->m_operator = [](RuntimeValue &val){return -val;};
            else if (op == "!") ret->m_operator = [](RuntimeValue &val){return !val;};
            else ret->m_operator = [](RuntimeValue &val){return ~val;};
            ret->m_expr = expression11();
            ret->m_end_token = ret->m_expr->m_end_token;
            return ret;
        }
        if (peek().value == "++" || peek().value == "--") {
            string op = peek().value;
            auto ret = make_unique<PreIncrementExpression>(peek());
            advance(); // op
            ret->m_var = peek().value;
            if (!is_valid_identifier(ret->m_var)) throw peek();
            ret->m_end_token = peek();
            advance(); // var
            ret->m_delta = op == "++" ? 1 : -1;
            return ret;
        }
        return expression12();
    }

    upExpression expression12() {
        advance();
        Token next_token = peek();
        backtrack();
        if (next_token.value == "++" || next_token.value == "--") {
            auto ret = make_unique<PostIncrementExpression>(peek());
            ret->m_var = peek().value;
            if (!is_valid_identifier(ret->m_var)) throw peek();
            advance(); // var
            ret->m_end_token = peek();
            advance(); // op
            ret->m_delta = next_token.value == "++" ? 1 : -1;
            return ret;
        }

        string name = peek().value;
        if (is_valid_identifier(name) && next_token.value == "(") {
            auto ret = make_unique<BuiltinFunctionExpression>(peek());
            ret->m_name = name;
            advance(); // name
            advance(); // (
            if (peek().value != ")") {
                while (true) {
                    ret->m_args.push_back(expression());
                    if (peek().value == ")")
                        break;
                    if (peek().value != ",") throw peek();
                    advance(); // ,
                }
            }
            ret->m_end_token = peek();
            advance(); // )
            return ret;
        }

        upExpression expr = expression13();
        if (peek().value == ".") {
            advance(); // .
            string field = peek().value;
            if (!is_valid_identifier(field)) throw peek();
            auto ret = make_unique<FieldAccessExpression>(expr->m_begin_token);
            ret->m_end_token = peek();
            advance(); // field
            ret->m_struct = move(expr);
            ret->m_field = field;
            return ret;
        }

        if (peek().value == "[") {
            advance(); // [
            auto ret = make_unique<BinaryOperatorExpression>(expr->m_begin_token);
            ret->m_left = move(expr);
            ret->m_right = expression();
            ret->m_operator = [](RuntimeValue &left, Expression &right, InterpreterContext &context){ return left[*context.evaluate_expression(right)]; };
            if (peek().value != "]") throw peek();
            ret->m_end_token = peek();
            advance(); // ]
            return ret;
        }

        return expr;
    }

    upExpression expression13() {
        if (peek().value == "(") {
            advance(); // (
            upExpression ret = expression1();
            if (peek().value != ")") throw peek();
            advance(); // )
            return ret;
        }
        return expression14();
    }

#undef BINARY_OPERATOR_EXPRESSION
#undef BINARY_OPERATOR_EXPRESSION2
#undef BINARY_OPERATOR_EXPRESSION3
#undef BINARY_OPERATOR_EXPRESSION4

    upExpression expression14() {
        Token first_token = peek();

        if (first_token.value == "true") {
            auto ret = make_unique<LiteralExpression>(peek());
            ret->m_end_token = peek();
            advance(); // true
            ret->m_value = make_shared<BooleanRuntimeValue>(true);
            return ret;
        } else if (first_token.value == "false") {
            auto ret = make_unique<LiteralExpression>(peek());
            ret->m_end_token = peek();
            advance(); // false
            ret->m_value = make_shared<BooleanRuntimeValue>(false);
            return ret;
        } else if (!first_token.value.empty() && (is_digit(first_token.value[0]) || (first_token.value.length() > 1 && first_token.value[0] == '.' && is_digit(first_token.value[1])))) {
            return literal_expression();
        } else if (is_valid_identifier(first_token.value)) {
            return var_reference_expression();
        } else {
            throw peek();
        }
    }

    upExpression literal_expression() {
        Token token = peek();
        string str = token.value;

        int index = 0;
        Radix radix = Radix::DEC;
        if (str[0] == '0' && str.length() > 1) {
            if (str[1] == 'x' || str[1] == 'X') {
                radix = Radix::HEX;
                index = 2;
            } else if (str[1] == 'b' || str[1] == 'B') {
                radix = Radix::BIN;
                index = 2;
            } else {
                radix = Radix::OCT;
            }
        }

        bool is_floating_point = false;
        is_floating_point |= str.find('.') != string::npos;
        if (is_floating_point && radix == Radix::OCT) radix = Radix::DEC;
        is_floating_point |= radix == Radix::DEC && (str.find('e') != string::npos || str.find('E') != string::npos);
        is_floating_point |= radix == Radix::HEX && (str.find('p') != string::npos || str.find('P') != string::npos);
        is_floating_point |= radix != Radix::HEX && (str.back() == 'f' || str.back() == 'F' || str.back() == 'd' || str.back() == 'D');

        int base = 10;
        switch (radix) {
            case Radix::BIN: base = 2; break;
            case Radix::OCT: base = 8; break;
            case Radix::DEC: base = 10; break;
            case Radix::HEX: base = 16; break;
        }

        uint64_t mantissa = 0;
        while (index < str.length() && is_digit(str[index], radix) && mantissa <= numeric_limits<uint64_t>::max() / base) {
            mantissa = base * mantissa;
            int to_add = is_digit(str[index]) ? str[index] - '0' : str[index] >= 'a' ? str[index] - 'a' + 10 : str[index] - 'A' + 10;
            if (to_add >= base)
                throw peek();
            if (numeric_limits<uint64_t>::max() - mantissa >= to_add)
                mantissa += to_add;
            index++;
        }

        spRuntimeValue val;

        if (!is_floating_point) {
            if (str.back() == 'l' || str.back() == 'L') {
                if (index != str.length() - 1)
                    throw peek();
                if (radix == Radix::DEC && mantissa > numeric_limits<int64_t>::max())
                    throw peek();
                val = make_shared<LongRuntimeValue>(static_cast<int64_t>(mantissa));
            } else {
                if (index != str.length())
                    throw peek();
                if (radix == Radix::DEC && mantissa > numeric_limits<int32_t>::max())
                    throw peek();
                if (mantissa > numeric_limits<uint32_t>::max()) // for radix != DEC
                    throw peek();
                val = make_shared<IntegerRuntimeValue>(static_cast<int32_t>(mantissa));
            }
        } else {
            int exponent1 = 0;
            if (index < str.length() && str[index] == '.') {
                if (radix != Radix::DEC && radix != Radix::HEX)
                    throw peek();
                index++;
                while (index < str.length() && is_digit(str[index], radix) && mantissa <= numeric_limits<uint64_t>::max() / base) {
                    mantissa = base * mantissa;
                    int to_add = is_digit(str[index]) ? str[index] - '0' : str[index] >= 'a' ? str[index] - 'a' + 10 : str[index] - 'A' + 10;
                    if (numeric_limits<uint64_t>::max() - mantissa >= to_add)
                        mantissa += to_add;
                    exponent1 -= radix == Radix::HEX ? 4 : 1;
                    index++;
                }
            }
            if (str[index] == 'e' || str[index] == 'E' || str[index] == 'p' || str[index] == 'P') {
                index++;
                if (index == str.length()) throw peek();
                if (str[index] == '+')
                    index++;
                bool neg_exponent = false;
                if (str[index] == '-') {
                    neg_exponent = true;
                    index++;
                }
                if (index == str.length()) throw peek();

                int exponent2 = 0;
                while (index < str.length() && is_digit(str[index]) && exponent2 < 1100) {
                    exponent2 = 10 * exponent2 + (str[index] - '0');
                    index++;
                }
                if (neg_exponent) exponent2 = -exponent2;
                exponent1 += exponent2;
            }
            bool is_double = true;
            if (index < str.length()) {
                if (str[index] == 'd' || str[index] == 'D')
                    index++;
                if (str[index] == 'f' || str[index] == 'F') {
                    is_double = false;
                    index++;
                }
            }
            if (index < str.length())
                throw peek();

            auto dval = double(mantissa);
            if (exponent1 < 0) {
                for (int i = 0; i > exponent1; i--) {
                    dval /= radix == Radix::HEX ? 2 : 10;
                }
            } else if (exponent1 > 1) {
                for (int i = 0; i < exponent1; i++) {
                    dval *= radix == Radix::HEX ? 2 : 10;
                }
            }

            if (is_double) {
                val = make_shared<DoubleRuntimeValue>(dval);
            } else {
                val = make_shared<FloatRuntimeValue>(float(dval));
            }
        }

        auto ret = make_unique<LiteralExpression>(peek());
        ret->m_end_token = peek();
        advance(); // literal
        ret->m_value = val;
        return ret;
    }

    upExpression var_reference_expression() {
        auto ret = make_unique<VarReferenceExpression>(peek());
        ret->m_end_token = peek();
        ret->m_name = peek().value;
        advance(); // name
        if (peek().value == "::") {
            advance(); // ::
            string val = peek().value;
            if (!is_valid_identifier(val)) throw peek();
            ret->m_end_token = peek();
            advance(); // val
            ret->m_name += "::" + val;
        }
        return ret;
    }

    upStructRef struct_ref() {
        bool is_decl = false;
        map<StructModifierType, shared_ptr<void>> modifiers;

        if (parse_struct_modifier(modifiers)) {
            is_decl = true;
            while (parse_struct_modifier(modifiers))
                ;
        }

        StructType type;
        if (peek().value == "struct") {
            advance(); // struct
            is_decl = true;
            type = StructType::STRUCT;
        } else if (peek().value == "enum") {
            advance(); // enum
            is_decl = true;
            type = StructType::ENUM;
            modifiers[StructModifierType::ELEMENT_TYPE] = struct_ref();
        } else if (peek().value == "flags") {
            advance(); // flags
            is_decl = true;
            type = StructType::FLAGS;
            modifiers[StructModifierType::ELEMENT_TYPE] = struct_ref();
        } else if (peek().value == "union") {
            advance(); // union
            is_decl = true;
            type = StructType::UNION;
        } else if (peek().value == "choose") {
            advance(); // choose
            is_decl = true;
            type = StructType::CHOOSE;
        } else if (is_decl) {
            throw peek();
        }

        if (is_decl) {
            auto struct_def = make_unique<Struct>();
            struct_def->m_type = type;
            struct_def->m_modifiers = modifiers;
            struct_def->m_name = nullptr;
            if (peek().value != "{") {
                string name = peek().value;
                if (!is_valid_identifier(name)) throw peek();
                advance(); // name
                struct_def->m_name = make_unique<string>(name);
            }
            if (peek().value != "{") throw peek();
            advance(); // {
            while (peek().value != "}") {
                struct_def->m_body.push_back(statement());
            }
            advance(); // }
            auto ret = make_unique<DeclaringStructRef>();
            ret->m_declaration = move(struct_def);
            return ret;
        } else {
            auto ret = make_unique<ResolvingStructRef>();
            ret->m_name = peek().value;
            if (!is_valid_identifier(ret->m_name)) throw peek();
            advance(); // name
            return ret;
        }
    }

    bool parse_struct_modifier(map<StructModifierType, shared_ptr<void>> &modifiers) {
        if (peek().value == "array_value") {
            if (modifiers.find(StructModifierType::ARRAY_VALUE) != modifiers.end())
                throw peek();
            advance(); // array_value
            string array_value = peek().value;
            if (!is_valid_identifier(array_value)) throw peek();
            advance(); // id
            modifiers[StructModifierType::ARRAY_VALUE] = make_shared<string>(array_value);
            return true;
        }
        return false;
    }

    bool parse_struct_ref_modifier(map<StructRefModifierType, shared_ptr<void>> &modifiers) {
        if (peek().value == "hide") {
            if (modifiers.find(StructRefModifierType::HIDE) != modifiers.end())
                throw peek();
            advance(); // hide
            modifiers[StructRefModifierType::HIDE] = nullptr;
            return true;
        }
        return false;
    }
};

bool parse(vector<Token> &tokens, vector<upStatement> &statements, ParserErrorHandler error_handler) {
    Parser parser(tokens, error_handler);
    while (!parser.eof()) {
        try {
            statements.push_back(parser.statement());
        } catch (Token &error) {
            error_handler(error);
            return false;
        }
    }
    return true;
}
