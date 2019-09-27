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
#include "gen.hpp"
#include "lex.hpp"
#include "misc.hpp"
#include "type.hpp"

std::ofstream log;

void sep() {
    log << "\n";
    log << "-------------\n";
    log << "\n";
}

void print(const t_ast& t, unsigned level = 0) {
    _ print_spaces = [&](unsigned n) {
        for (_ i = 0u; i < n; i++) {
            log << " ";
        }
    };
    print_spaces(1 * level);
    log << "(" << t.uu << ")";
    if (t.vv.size() > 0) {
        log << " : " << print_bytes(t.vv);
    }
    log << "\n";
    for (_& c : t.children) {
        print(c, level + 1);
    }
    log.flush();
}

void print(const std::list<t_lexeme>& ll) {
    for (_& l : ll) {
        log << l.uu;
        if (not l.vv.empty()) {
            log << " ||| " << print_bytes(l.vv);
        }
        log << "\n";
    }
    log.flush();
}

_ get_line_pos(const str& src, int line) {
    size_t i = 0;
    _ line_count = 0;
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

_ preprocess(std::list<t_lexeme>& ll) {
    _ it = ll.begin();
    while (it != ll.end()) {
        _ in_pp = ((*it).uu == "#");
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

_ escape_seqs(std::list<t_lexeme>& ll) {
    for (_& l : ll) {
        if (l.uu == "char_constant" or l.uu == "string_literal") {
            str new_str;
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

const std::set<str> keywords = {
    "int", "return", "if", "else", "while", "for", "do",
    "continue", "break", "struct", "float",
    "char", "unsigned", "void", "_", "case", "const",
    "default", "double", "enum", "extern", "goto", "long",
    "register", "short", "signed", "sizeof", "static", "struct",
    "switch", "typedef", "union", "volatile"
};

_ convert_lexemes(std::list<t_lexeme>& ll) {
    _ it = ll.begin();
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
            if ((*it).vv.find('.') != str::npos or
                (((*it).vv.find('e') != str::npos
                  or (*it).vv.find('E') != str::npos)
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
        std::cerr << "error : bad argument list\n";
        return 1;
    }
    std::ifstream is(argv[1]);
    if (!is.good()) {
        std::cerr << "error : could not open input file\n";
        return 1;
    }

    log.open("log.txt");
    _ src = read_file_into_string(is);

    try {
        _ ll = lex(src);
        print(ll);
        sep();
        preprocess(ll);
        print(ll);
        escape_seqs(ll);
        sep();
        convert_lexemes(ll);
        print(ll);
        sep();
        _ ast = parse_program(ll);
        print(ast);
        sep();
        _ res = gen_asm(ast);
        log << res;
        std::ofstream os(argv[2]);
        if (!os.good()) {
            std::cerr << "error : could not open output file\n";
            return 1;
        }
        os << res;
    } catch (const t_compile_error& e) {
        _ is_loc_valid = (e.get_loc() != t_loc());
        if (is_loc_valid) {
            std::cerr << (e.line() + 1) << ":" << (e.column() + 1) << ": ";
        }
        std::cerr << "error: ";
        std::cerr << e.what() << "\n";
        if (is_loc_valid) {
            _ i = get_line_pos(src, e.line());
            for (_ j = i; j < src.size() and src[j] != '\n'; j++) {
                std::cerr << src[j];
            }
            std::cerr << "\n";
            for (_ j = 0; j < e.column(); j++) {
                std::cerr << " ";
            }
            std::cerr << "^";
            std::cerr << "\n";
        }
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "unknown error: " << e.what() << "\n";
    }
}
