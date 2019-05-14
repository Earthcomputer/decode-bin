
#ifndef DECODE_BIN_UTIL_H
#define DECODE_BIN_UTIL_H

#include <set>
#include <string>
#include "tokenizer.h"

const std::set<std::string> OPERATORS = {
        "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>",
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "=",
        "~", "!", "?", ":", "::", "++", "--",
        "&&", "||", "==", "!=", "<", ">", "<=", ">=",
        "(", ")", "[", "]", "{", "}", ",", ";", "."
};

const std::set<std::string> KEYWORDS = {
        "if", "else", "do", "while", "switch", "case", "default", "break",
        "continue", "var", "struct", "enum", "flags", "union", "true", "false",
        "choose", "array_value", "hide"
};

// assumes that the input is a whole token
bool is_valid_identifier(std::string &name);


enum class Radix {
    BIN, OCT, DEC, HEX
};

bool is_whitespace(char ch);
bool is_digit(char ch);
bool is_digit(char ch, Radix radix);

std::string create_underline(Token &t);

#endif //DECODE_BIN_UTIL_H
