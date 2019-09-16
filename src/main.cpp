#include <fstream>
#include <sstream>
#include <iostream>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <list>
#include <functional>
#include <initializer_list>
#include <unordered_map>

#include "ast.hpp"
#include "asm_gen.hpp"
#include "lex.hpp"
#include "misc.hpp"
#include "type.hpp"

using namespace std;

ofstream log;

void print(const t_ast& t, unsigned level = 0) {
    auto print_spaces = [&](unsigned n) {
        for (auto i = 0u; i < n; i++) {
            log << " ";
        }
    };
    print_spaces(1 * level);
    log << "(" << t.uu << ")";
    if (t.vv.size() > 0) {
        log << " : " << print_bytes(t.vv);
    }
    log << "\n";
    for (auto& c : t.children) {
        print(c, level + 1);
    }
    log.flush();
}

auto delete_comments(const string& str) {
    string res;
    auto i = size_t(0);
    auto in_comment = false;
    while (i != str.length()) {
        if (not in_comment) {
            if (str.compare(i, 2, "/*") == 0) {
                in_comment = true;
                res += "  ";
                i += 2;
            } else {
                res += str[i];
                i++;
            }
        } else {
            if (str.compare(i, 2, "*/") == 0) {
                in_comment = false;
                res += "  ";
                i += 2;
            } else {
                if (str[i] == '\n') {
                    res += '\n';
                } else {
                    res += ' ';
                }
                i++;
            }
        }
    }
    return res;
}

auto get_line_pos(const string& src, int line) {
    auto i = 0;
    auto line_count = 0;
    if (line_count != line) {
        while (i < src.size()) {
            if (i != 0 and src[i-1] == '\n') {
                line_count++;
                if (line_count == line) {
                    break;
                }
            }
            i++;
        }
    }
    return i;
}

auto preprocess(const string& str) {
    string res;
    auto ignore = false;
    auto line_beginning = true;
    for (auto ch : str) {
        if (ignore) {
            if (ch == '\n') {
                res += ch;
                ignore = false;
            }
        } else {
            if (line_beginning and ch == '#') {
                ignore = true;
            } else {
                res += ch;
            }
        }
        line_beginning = (ch == '\n');
    }
    return res;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "error : bad argument list\n";
        return 1;
    }
    ifstream is(argv[1]);
    if (!is.good()) {
        cerr << "error : could not open input file\n";
        return 1;
    }

    log.open("log.txt");
    auto sep = [&]() {
        log << "\n";
        log << "-------------\n";
        log << "\n";
    };

    auto src = read_file_into_string(is);
    auto lexemes = lex(preprocess(delete_comments(src)));

    for (auto& l : lexemes) {
        log << l.uu;
        if (not l.vv.empty()) {
            log << " -- " << print_bytes(l.vv);
        }
        log << "\n";
    }
    sep();
    log.flush();

    try {
        auto ast = parse_program(lexemes);
        print(ast);
        sep();
        auto res = gen_asm(ast);
        log << res;
        ofstream os(argv[2]);
        if (!os.good()) {
            cerr << "error : could not open output file\n";
            return 1;
        }
        os << res;
    } catch (const t_compile_error& e) {
        auto is_loc_valid = (e.get_loc() != t_loc());
        if (is_loc_valid) {
            cerr << (e.line() + 1) << ":" << (e.column() + 1) << ": ";
        }
        cerr << "error: ";
        cerr << e.what() << "\n";
        if (is_loc_valid) {
            auto i = get_line_pos(src, e.line());
            for (auto j = i; j < src.size() and src[j] != '\n'; j++) {
                cerr << src[j];
            }
            cerr << "\n";
            for (size_t j = 0; j < e.column(); j++) {
                cerr << " ";
            }
            cerr << "^";
            cerr << "\n";
        }
        return 1;
    } catch (const exception& e) {
        cerr << "unknown error: " << e.what() << "\n";
    }
}
