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
#include <set>

#include "ast.hpp"
#include "asm_gen.hpp"
#include "lex.hpp"
#include "misc.hpp"
#include "type.hpp"

using namespace std;

ofstream log;

void sep() {
    log << "\n";
    log << "-------------\n";
    log << "\n";
}

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

void print(const list<t_lexeme>& ll) {
    for (auto& l : ll) {
        log << l.uu;
        if (not l.vv.empty()) {
            log << " ||| " << print_bytes(l.vv);
        }
        log << "\n";
    }
    log.flush();
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

auto preprocess(list<t_lexeme>& ll) {
    auto it = ll.begin();
    while (it != ll.end()) {
        auto in_pp = ((*it).uu == "#");
        while (it != ll.end() and (*it).uu != "newline") {
            if (in_pp) {
                it = ll.erase(it);
            } else {
                it++;
            }
        }
        if (it != ll.end()) {
            it++;
        }
    }
}

auto escape_seqs(list<t_lexeme>& ll) {
    for (auto& l : ll) {
        if (l.uu == "char_constant" or l.uu == "string_literal") {
            string new_str;
            size_t i = 0;
            while (i < l.vv.length()) {
                if (i+1 < l.vv.length()
                    and l.vv[i] == '\\' and l.vv[i+1] == 'n') {
                    new_str += "\n";
                    i += 2;
                } else {
                    new_str += l.vv[i];
                    i++;
                }
            }
            l.vv = new_str;
        }
    }
}

const set<string> keywords = {
    "int", "return", "if", "else", "while", "for", "do",
    "continue", "break", "struct", "float",
    "char", "unsigned", "void", "auto", "case", "const",
    "default", "double", "enum", "extern", "goto", "long",
    "register", "short", "signed", "sizeof", "static", "struct",
    "switch", "typedef", "union", "volatile"
};

auto convert_lexemes(list<t_lexeme>& ll) {
    auto it = ll.begin();
    while (it != ll.end()) {
        if ((*it).uu == "newline") {
            it = ll.erase(it);
            continue;
        }
        if ((*it).uu == "identifier") {
            if (keywords.count((*it).vv) != 0) {
                (*it).uu = (*it).vv;
                (*it).vv = "";
            }
        } else if ((*it).uu == "pp_number") {
            if ((*it).vv.find('.') != string::npos or
                (((*it).vv.find('e') != string::npos
                  or (*it).vv.find('E') != string::npos)
                 and not ((*it).vv.length() >= 2
                          and ((*it).vv[1] == 'x'
                               or (*it).vv[1] == 'X')))) {
                (*it).uu = "floating_constant";
            } else {
                (*it).uu = "integer_constant";
            }
        } else if ((*it).uu == "string_literal"
                   or (*it).uu == "char_constant") {
            (*it).vv = (*it).vv.substr(1, (*it).vv.length() - 2);
        }
        it++;
    }
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
    auto src = read_file_into_string(is);

    try {
        auto ll = lex(src);
        print(ll);
        sep();
        preprocess(ll);
        print(ll);
        escape_seqs(ll);
        convert_lexemes(ll);
        print(ll);
        sep();
        auto ast = parse_program(ll);
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
