
#include "tokenizer.h"
#include <cctype>
#include "util.h"

using namespace std;

enum TokenType {
    WHITESPACE, WORD, NUMBER, OPERATOR, CHAR, STRING, COMMENT, MULTILINE_COMMENT
};


TokenType get_token_type(string &line, int index) {
    char ch = line[index];
    if (is_whitespace(ch)) {
        return WHITESPACE;
    } else if (ch == '.') {
        if (index + 1 == line.length()) {
            return OPERATOR;
        }
        else if (is_digit(line[index + 1])) {
            return NUMBER;
        }
        else {
            return OPERATOR;
        }
    } else if (ch == '/') {
        if (index + 1 == line.length()) {
            return OPERATOR;
        }
        else if (line[index + 1] == '/') {
            return COMMENT;
        }
        else if (line[index + 1] == '*') {
            return MULTILINE_COMMENT;
        }
        else {
            return OPERATOR;
        }
    } else if (is_digit(ch)) {
        return NUMBER;
    } else if (ch == '\'') {
        return CHAR;
    } else if (ch == '"') {
        return STRING;
    } else if (OPERATORS.find(string(1, ch)) != OPERATORS.end()) {
        return OPERATOR;
    } else {
        return WORD;
    }
}

inline void add_token(string &line, unsigned &token_start, unsigned lineno, unsigned index, vector<Token> &tokens) {
    tokens.push_back({line.substr(token_start, index - token_start), lineno, token_start});
    token_start = index;
}

bool tokenize(string &line, unsigned lineno, TokenType &token_type, vector<Token> &tokens, function<void(Token)> error_handler) {
    unsigned token_start = 0;

    for (unsigned index = 0; index < line.length(); index++) {
        switch (token_type) {
            case WHITESPACE: {
                token_type = get_token_type(line, index);
                token_start = index;
                break;
            }
            case WORD: {
                token_type = get_token_type(line, index);
                if (token_type != WORD) {
                    if (is_digit(line[index])) {
                        token_type = WORD;
                    } else {
                        add_token(line, token_start, lineno, index, tokens);
                    }
                }
                break;
            }
            case NUMBER: {
                Radix radix = Radix::DEC;
                char ch = line[index];
                // Determine the radix if it starts with 0. May incorrectly label as octal, that's okay
                if (line[index-1] == '0') {
                    if (ch == 'x' || ch == 'X')
                        radix = Radix::HEX;
                    else if (ch == 'b' || ch == 'B')
                        radix = Radix::BIN;
                    else
                        radix = Radix::OCT;
                    if (radix == Radix::HEX || radix == Radix::BIN) {
                        index++;
                        if (index == line.length() || !is_digit(line[index], radix)) {
                            error_handler({line.substr(token_start, index - token_start), lineno, token_start});
                            return false;
                        }
                    }
                    ch = line[index];
                }
                // Parse the main part of the number
                bool had_dot = line[index-1] == '.';
                while (is_digit(ch, radix) || is_digit(ch)) {
                    index++;
                    if (index == line.length())
                        goto end_line;
                    ch = line[index];
                }
                // Parse anything past the decimal point
                if (!had_dot && ch == '.' && radix != Radix::BIN) {
                    had_dot = true;
                    if (radix == Radix::OCT) radix = Radix::DEC;
                    if (index == line.length())
                        goto end_line;
                    ch = line[index];
                    while (is_digit(ch, radix)) {
                        index++;
                        if (index == line.length())
                            goto end_line;
                        ch = line[index];
                    }
                }
                // Parse the exponent to the number
                if (((radix == Radix::OCT || radix == Radix::DEC) && (ch == 'e' || ch == 'E')) || (radix == Radix::HEX && (ch == 'p' || ch == 'P'))) {
                    if (radix == Radix::OCT) radix = Radix::DEC;
                    index++;
                    if (index != line.length() && (line[index] == '+' || line[index] == '-')) {
                        index++;
                    }
                    if (index == line.length() || !is_digit(line[index])) {
                        error_handler({line.substr(token_start, index - token_start), lineno, token_start});
                        return false;
                    }
                    ch = line[index];
                    while (is_digit(ch)) {
                        index++;
                        if (index == line.length())
                            goto end_line;
                        ch = line[index];
                    }
                }
                // Parse datatype suffix
                if (ch == 'f' || ch == 'F' || ch == 'd' || ch == 'D' || ch == 'l' || ch == 'L') {
                    index++;
                    if (index == line.length())
                        goto end_line;
                    ch = line[index];
                }

                add_token(line, token_start, lineno, index, tokens);
                token_type = get_token_type(line, index);
                break;
            }
            case OPERATOR: {
                string op = line.substr(token_start, index + 1 - token_start);
                if (OPERATORS.find(op) == OPERATORS.end()) {
                    add_token(line, token_start, lineno, index, tokens);
                    token_type = get_token_type(line, index);
                }
                break;
            }
            case CHAR:
            case STRING: {
                if (line[index] == (token_type == CHAR ? '\'' : '"')) {
                    bool escaped = false;
                    for (unsigned i = index - 1; line[i] == '\\'; i--)
                        escaped = !escaped;
                    if (!escaped) {
                        index++;
                        if (index == line.length())
                            goto end_line;
                        add_token(line, token_start, lineno, index, tokens);
                        token_type = get_token_type(line, index);
                    }
                }
                break;
            }
            case COMMENT: {
                token_type = WHITESPACE;
                return true;
            }
            case MULTILINE_COMMENT: {
                if (index > 0 && line[index-1] == '*' && line[index] == '/') {
                    token_type = WHITESPACE;
                    index++;
                    if (index == line.length())
                        return true;
                    token_type = get_token_type(line, index);
                }
                break;
            }
        }
    }

    if (token_type == CHAR || token_type == STRING) {
        error_handler({line.substr(token_start, line.length() - token_start), lineno, token_start});
        return false;
    }

    end_line:
    if (token_type != WHITESPACE) {
        add_token(line, token_start, lineno, static_cast<unsigned>(line.length()), tokens);
        token_type = WHITESPACE;
    }
    return true;
}

bool tokenize(vector<string> &lines, vector<Token> &tokens, function<void(Token)> error_handler) {
    TokenType token_type = WHITESPACE;
    for (unsigned lineno = 1; lineno <= lines.size(); lineno++) {
        if (!tokenize(lines[lineno-1], lineno, token_type, tokens, error_handler))
            return false;
    }
    if (token_type == MULTILINE_COMMENT) {
        error_handler({"", static_cast<unsigned>(lines.size()), static_cast<unsigned>(lines.back().length())});
        return false;
    }
    return true;
}

