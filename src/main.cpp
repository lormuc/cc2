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

// auto& log = cout;
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
}

// string stringify(const t_type& type, string identifier = "") {
//     string res;
//     if (type.get_kind() == "basic_type") {
//         res += type.get_basic_type_kind();
//         if (identifier != "") {
//             res += " ";
//             res += identifier;
//         }
//     } else if (type.get_kind() == "function") {
//         res += stringify(type.get_function_return_type());
//         res += " (";
//         res += identifier;
//         res += ")(";
//         auto params = type.get_function_parameters();
//         if (not params.empty()) {
//             auto initial = true;
//             for (auto& p : params) {
//                 if (not initial) {
//                     res += ", ";
//                 }
//                 res += stringify(p);
//                 initial = false;
//             }
//         }
//         res += ")";
//     } else if (type.get_kind() == "pointer") {
//         auto new_id = string("*") + identifier;
//         res += stringify(type.get_referenced_type(), new_id);
//     } else if (type.get_kind() == "array") {
//         auto len = type.get_array_length();
//         string len_str;
//         if (len != array_unknown_length) {
//             len_str += to_string(len);
//         }
//         if (identifier != "") {
//             identifier = string("(") + identifier + ")";
//         }
//         auto new_id = identifier + "[" + len_str + "]";
//         res += stringify(type.get_array_element_type(), new_id);
//     }
//     return res;
// }

auto t_ptr(auto x) {
    return t_type("pointer", x);
}

const auto t_int = t_type("int");

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
    // auto f = ifstream("test/1.c");
    // auto s = read_file_into_string(f);
    // auto res = preprocess(delete_comments(s));
    // cout << res << "--------------\n";
    // return 0;

    // auto z = t_type(string("int"));
    // auto y = z;
    // t_type x(t_ptr(t_int), {t_ptr(t_type(t_int, {t_int})), t_int, t_ptr(t_int)});
    // x = t_type(x, array_unknown_length);
    // x = t_ptr(x);
    // cout << stringify(x, "xxx") << "\n";
    // return 0;

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
    } catch (const t_error& e) {
        cerr << (e.line() + 1) << ":" << (e.column() + 1) << ": error: ";
        cerr << e.what() << "\n";
        size_t i = 0;
        size_t line_count = 0;
        if (line_count != e.line()) {
            while (i < src.size()) {
                if (i != 0 and src[i-1] == '\n') {
                    line_count++;
                    if (line_count == e.line()) {
                        break;
                    }
                }
                i++;
            }
        }
        for (auto j = i; j < src.size() and src[j] != '\n'; j++) {
            cerr << src[j];
        }
        cerr << "\n";
        for (size_t j = 0; j < e.column(); j++) {
            cerr << " ";
        }
        cerr << "^";
        cerr << "\n";
        return 1;
    }
}
