
#ifndef DECODE_BIN_PARSER_H
#define DECODE_BIN_PARSER_H

#include <functional>
#include <string>
#include <vector>
#include "ast.h"
#include "tokenizer.h"

bool parse(std::vector<Token> &tokens, std::vector<upStatement> &statements, std::function<void(Token)> error_handler);

#endif //DECODE_BIN_PARSER_H
