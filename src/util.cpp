
#include "util.h"
#include <cctype>

using namespace std;

bool is_valid_identifier(std::string &name) {
    if (name.empty())
        return false;
    if (OPERATORS.find(name) != OPERATORS.end())
        return false;
    if (KEYWORDS.find(name) != KEYWORDS.end())
        return false;
    if (is_digit(name[0]) || name[0] == '.')
        return false;
    return true;
}

bool is_whitespace(char ch) {
    return isspace(static_cast<unsigned char>(ch));
}
bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}
bool is_digit(char ch, Radix radix) {
    switch (radix) {
        case Radix::BIN:
            return ch == '0' || ch == '1';
        case Radix::OCT:
            return ch >= '0' && ch <= '7';
        case Radix::DEC:
            return is_digit(ch);
        case Radix::HEX:
            return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    }
}

string create_underline(Token &t) {
    string ret;
    for (int i = 0; i < t.col; i++)
        ret += " ";
    if (!t.value.empty())
        ret += "^";
    if (t.value.size() >= 2) {
        for (int i = 0; i < t.value.size() - 2; i++) {
            ret += "~";
        }
        ret += "^";
    }
    return ret;
}
