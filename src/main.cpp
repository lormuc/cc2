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
    for (_ i = 0u; i < 4 * level; i++) {
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

_ concatenate_string_literals(std::list<t_pp_lexeme>& ls) {
    _ it = ls.begin();
    while ((*it).kind != "eof") {
        if ((*it).kind == "string_literal") {
            while (true) {
                _ jt = it;
                jt++;
                while ((*jt).kind == "whitespace" or (*jt).kind == "newline") {
                    jt++;
                }
                if ((*jt).kind != "string_literal") {
                    break;
                }
                (*it).val.pop_back();
                (*it).val.append((*jt).val, 1);
                ls.erase(next(it), next(jt));
            }
        }
        it++;
    }
}

namespace {
    _ usage() {
        cout << "usage: ./build/program [option] <input-file>\n";
        cout << "options:\n";
        cout << "--lex        print the preprocessing tokens\n";
        cout << "--pp         print the preprocessed source file\n";
        cout << "--pre-ast    print the tokens after preprocessing\n";
        cout << "--ast        print the abstract syntax tree\n";
        cout << "-o <file>    place the llvm output into <file>\n";
    }
}

int main(int argc, char** argv) {
    str input_file;
    str output_file;
    str end_phase;
    if (argc == 2) {
        input_file = argv[1];
        output_file = replace_extension(argv[1], ".ll");
    } else if (argc == 3) {
        _ option = str(argv[1]);
        if (option == "--lex") {
            end_phase = "lex";
        } else if (option == "--pp") {
            end_phase = "pp";
        } else if (option == "--pre-ast") {
            end_phase = "pre-ast";
        } else if (option == "--ast") {
            end_phase = "ast";
        } else {
            usage();
            return 1;
        }
        input_file = argv[2];
    } else if (argc == 4) {
        if (str(argv[1]) == "-o") {
            output_file = argv[2];
            input_file = argv[3];
        } else {
            usage();
            return 1;
        }
    } else {
        usage();
        return 1;
    }

    _ fm = t_file_manager();
    size_t input_file_idx;
    try {
        input_file_idx = fm.read_file(input_file);
    } catch (const std::exception& e) {
        die("could not open " + input_file);
    }

    try {
        _ pp_ls = lex(input_file_idx, fm);
        if (end_phase == "lex") {
            print(pp_ls, cout);
            return 0;
        }

        preprocess(pp_ls, fm);
        if (end_phase == "pp") {
            print(pp_ls, cout);
            return 0;
        }

        escape_seqs(pp_ls.begin(), pp_ls.end());
        concatenate_string_literals(pp_ls);

        _ ls = convert_lexemes(pp_ls.begin(), pp_ls.end());

        for (_ it = ls.begin(); (*it).uu != "eof";) {
            if ((*it).uu == "const" or (*it).uu == "volatile") {
                it = ls.erase(it);
            } else {
                it++;
            }
        }

        if (end_phase == "pre-ast") {
            print(ls, cout);
            return 0;
        }

        _ ast = parse_program(ls.cbegin());
        if (end_phase == "ast") {
            print(ast, cout);
            return 0;
        }

        _ res = gen_asm(ast);
        _ os = std::ofstream(output_file);
        os.good() or die("could not open output file" + output_file);
        os << res;
    } catch (const t_compile_error& e) {
        _ src = fm.get_file_contents(e.loc().file_idx());
        _& loc = e.loc();
        _ is_loc_valid = loc.is_valid();
        if (is_loc_valid) {
            std::cerr << fm.get_path(e.loc().file_idx()) << ":";
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
            for (_ j = i; j < i + loc.column(); j++) {
                if (src[j] == ' ' or src[j] == '\t') {
                    std::cerr << src[j];
                } else {
                    std::cerr << " ";
                }
            }
            std::cerr << "^";
            std::cerr << "\n";
        }
        return 1;
    } catch (const std::exception& e) {
        die(e.what());
    }
}
