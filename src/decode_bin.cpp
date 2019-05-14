

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

    try {
        execute(statements);
    } catch (const char* error) {
        cerr << error << endl;
    } catch (string &error) {
        cerr << error << endl;
    }

}
