
#ifndef DECODE_BIN_TOKENIZER_H
#define DECODE_BIN_TOKENIZER_H

#include <functional>
#include <string>
#include <vector>

struct Token {
    std::string value;
    unsigned line;
    unsigned col;
};

bool tokenize(std::vector<std::string> &lines, std::vector<Token> &tokens, std::function<void(Token)> error_handler);

#endif //DECODE_BIN_TOKENIZER_H
