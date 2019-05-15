

#include <iostream>
#include <fstream>
#include <vector>
#include "util.h"
#include "tokenizer.h"
#include "parser.h"

using namespace std;

void print_usage(char *program_name) {
    cout << program_name << " <binformat_file>" << endl;
}

void read_lines(ifstream &file, vector<string> &lines) {
    string line;
    while (getline(file, line)) {
        lines.push_back(line);
    }
}

void print_token_range(const vector<string> &lines, Token begin_token, Token end_token, string padding) {
    if (begin_token.line == end_token.line) {
        cerr << padding << lines[begin_token.line - 1] << endl;
        Token compound_token = {
                lines[begin_token.line - 1].substr(begin_token.col, end_token.col + end_token.value.length() - begin_token.col),
                begin_token.line, begin_token.col
        };
        cerr << padding << create_underline(compound_token) << endl;
    } else {
        cerr << padding << lines[begin_token.line - 1] << endl;
        Token compound_token = {lines[begin_token.line - 1].substr(begin_token.col), begin_token.line, begin_token.col};
        cerr << padding << create_underline(compound_token) << endl;
        if (end_token.line != begin_token.line + 1) {
            cerr << padding << "... " << (end_token.line - begin_token.line - 1) << " " << (end_token.line == begin_token.line + 2 ? "line" : "lines") << " omitted" << endl;
        }
        cerr << padding << lines[end_token.line - 1] << endl;
        compound_token = {lines[end_token.line - 1].substr(0, end_token.col + end_token.value.length()), end_token.line, 0};
        cerr << padding << create_underline(compound_token) << endl;
    }
}

int main(int argc, char **argv) {

    if (argc != 2) {
        print_usage(argv[0]);
        return 0;
    }

    ifstream infile(argv[1]);
    if (infile.fail()) {
        cerr << "Failed to open input file" << endl;
        return 1;
    }

    vector<string> lines;
    read_lines(infile, lines);

    vector<Token> tokens;
    if (!tokenize(lines, tokens, [lines](Token t) {
        cerr << lines[t.line - 1] << endl;
        cerr << create_underline(t) << endl;
        cerr << "Syntax error " << t.line << ":" << t.col << endl;
    })) {
        return 1;
    }

    vector<upStatement> statements;

    parse(tokens, statements, [lines](Token t) {
        cerr << lines[t.line - 1] << endl;
        cerr << create_underline(t) << endl;
        cerr << "Parsing error " << t.line << ":" << t.col << endl;
    });

    execute(statements, [lines](string &error, vector<Statement*> &executing_statements, vector<Expression*> evaluating_expressions) {
        Token begin_token, end_token;
        if (evaluating_expressions.empty()) {
            begin_token = executing_statements.back()->m_begin_token;
            end_token = executing_statements.back()->m_end_token;
        } else {
            begin_token = evaluating_expressions.back()->m_begin_token;
            end_token = evaluating_expressions.back()->m_end_token;
        }
        cerr << ":" << begin_token.line << ":" << begin_token.col << ": " << error << endl;
        print_token_range(lines, begin_token, end_token, "");

        auto it = executing_statements.rbegin();
        if (evaluating_expressions.empty()) ++it;
        for (; it != executing_statements.rend(); ++it) {
            cerr << "  at :" << (*it)->m_begin_token.line << ":" << (*it)->m_begin_token.col << endl;
            print_token_range(lines, (*it)->m_begin_token, (*it)->m_end_token, "    ");
        }
    });

}
