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
#include "pp.hpp"

void print(const t_ast& ast, std::ostream& os, unsigned level = 0) {
    for (_ i = 0u; i < 1 * level; i++) {
        os << " ";
    }
    os << "(" << ast.uu << ")";
    if (ast.vv.size() > 0) {
        os << " || ";
        print_bytes(ast.vv, os);
    }
    os << "\n";
    for (_& child : ast.children) {
        print(child, os, level + 1);
    }
    os.flush();
}

_ print(const std::list<t_lexeme>& ls, std::ostream& os) {
    for (_& lx : ls) {
        os << lx.uu;
        if (not lx.vv.empty()) {
            os << " || ";
            print_bytes(lx.vv, os);
        }
        os << "\n";
    }
    os.flush();
}

_ print_as_text(const std::list<t_lexeme>& ls, std::ostream& os) {
    for (_& lx : ls) {
        os << lx.vv;
    }
}

_ escape_seqs(std::list<t_lexeme>& ls) {
    for (_& lx : ls) {
        _& kind = lx.uu;
        _& val = lx.vv;
        if (kind == "char_constant" or kind == "string_literal") {
            str new_str;
            size_t i = 0;
            while (i < val.length()) {
                if (i+1 < val.length()
                    and val[i] == '\\' and val[i+1] == 'n') {
                    new_str += "\n";
                    i += 2;
                } else {
                    new_str += val[i];
                    i++;
                }
            }
            val = new_str;
        }
    }
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

const std::set<str> keywords = {
    "int", "return", "if", "else", "while", "for", "do",
    "continue", "break", "struct", "float",
    "char", "unsigned", "void", "auto", "case", "const",
    "default", "double", "enum", "extern", "goto", "long",
    "register", "short", "signed", "sizeof", "static", "struct",
    "switch", "typedef", "union", "volatile"
};

_ convert_lexemes(std::list<t_lexeme>& ll) {
    _ it = ll.begin();
    while (it != ll.end()) {
        _& name = (*it).uu;
        _& val = (*it).vv;
        if (name == "newline" or name == "whitespace") {
            it = ll.erase(it);
            continue;
        }
        if (name == "identifier") {
            if (keywords.count(val) != 0) {
                name = val;
            }
        } else if (name == "pp_number") {
            if (val.find('.') != str::npos or
                ((val.find('e') != str::npos or val.find('E') != str::npos)
                 and not (val.length() >= 2
                          and (val[1] == 'x' or val[1] == 'X')))) {
                name = "floating_constant";
            } else {
                name = "integer_constant";
            }
        } else if (name == "string_literal" or name == "char_constant") {
            val = val.substr(1, val.length() - 2);
        }
        it++;
    }
}

_ die(const str& msg) {
    std::cerr << "error: " << msg << "\n";
    exit(1);
    return 0;
}

_ separator(std::ostream& os) {
    os << "\n";
    os << "-------------\n";
    os << "\n";
}

_ title(const str& x, std::ostream& os) {
    os << "\n";
    os << x << ":\n";
    os << "\n";
}

int main(int argc, char** argv) {
    str input_file;
    str output_file = "o.ll";
    _ phase = -1;
    if (argc == 2) {
        input_file = argv[1];
    } else if (argc == 3 and argv[1][0] == '-') {
        phase = stoi(str(argv[1]).substr(1));
        input_file = argv[2];
    } else if (argc == 3) {
        input_file = argv[1];
        output_file = argv[2];
    } else {
        die("incorrect usage");
    }

    std::ifstream is(input_file);
    is.good() or die("could not open input file");

    std::ofstream log;
    log.open("log.txt");
    _ src = read_file_into_string(is);

    try {
        _ phase_cnt = 0;

        title("lex", log);
        _ ls = lex(src);
        phase_cnt++;
        print(ls, log);
        separator(log);
        if (phase_cnt == phase) {
            print(ls, cout);
            cout << "\n";
            return 0;
        }

        title("preprocess", log);
        preprocess(ls);
        phase_cnt++;
        print(ls, log);
        separator(log);
        if (phase_cnt == phase) {
            print_as_text(ls, cout);
            cout << "\n";
            return 0;
        }

        escape_seqs(ls);
        phase_cnt++;
        if (phase_cnt == phase) {
            print(ls, cout);
            cout << "\n";
            return 0;
        }

        convert_lexemes(ls);
        phase_cnt++;
        if (phase_cnt == phase) {
            print(ls, cout);
            cout << "\n";
            return 0;
        }

        _ ast = parse_program(ls);
        phase_cnt++;
        title("ast", log);
        print(ast, log);
        if (phase_cnt == phase) {
            print(ast, cout);
            cout << "\n";
            return 0;
        }

        _ res = gen_asm(ast);
        _ os = std::ofstream(output_file);
        os.good() or die("could not open output file");
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
        die(e.what());
    }
}
