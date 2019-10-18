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
            os << " || \"";
            print_bytes(lx.vv, os);
            os << "\"";
        }
        os << "\n";
    }
    os.flush();
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

_ concatenate_string_literals(std::list<t_pp_lexeme>& ls) {
    _ it = ls.begin();
    while (it != ls.end()) {
        if ((*it).kind == "string_literal") {
            while (next(it) != ls.end()
                   and (*next(it)).kind == "string_literal") {
                (*it).val.pop_back();
                (*it).val.append((*next(it)).val, 1);
                ls.erase(next(it));
            }
        }
        it++;
    }
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
    is.good() or die("could not open '" + input_file + "'");

    std::ofstream log;
    log.open("log.txt");
    _ src = read_file_into_string(is);

    try {
        _ phase_cnt = 0;

        title("lex", log);
        _ pp_ls = lex(input_file, src);
        phase_cnt++;
        print(pp_ls, log);
        separator(log);
        if (phase_cnt == phase) {
            print(pp_ls, cout);
            cout << "\n";
            return 0;
        }

        title("preprocess", log);
        preprocess(pp_ls);
        phase_cnt++;
        print(pp_ls, log);
        separator(log);
        if (phase_cnt == phase) {
            print(pp_ls, cout);
            cout << "\n";
            return 0;
        }

        escape_seqs(pp_ls.begin(), pp_ls.end());
        concatenate_string_literals(pp_ls);
        // phase_cnt++;
        // if (phase_cnt == phase) {
        //     print(pp_ls, cout);
        //     cout << "\n";
        //     return 0;
        // }

        _ ls = convert_lexemes(pp_ls.begin(), pp_ls.end());
        phase_cnt++;
        if (phase_cnt == phase) {
            print(ls, cout);
            cout << "\n";
            return 0;
        }

        _ ast = parse_program(ls.cbegin());
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
        _& loc = e.loc();
        _ is_loc_valid = loc.is_valid();
        if (is_loc_valid) {
            std::cerr << loc.filename() << ":";
            std::cerr << loc.line() << ":" << loc.column() << ": ";
        }
        std::cerr << "error: ";
        std::cerr << e.what() << "\n";
        if (is_loc_valid) {
            _ i = get_line_pos(src, loc.line() - 1);
            for (_ j = i; j < src.size() and src[j] != '\n'; j++) {
                std::cerr << src[j];
            }
            std::cerr << "\n";
            for (_ j = 0; j < loc.column(); j++) {
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
