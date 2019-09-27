#include <string>
#include <algorithm>
#include <iostream>
#include <vector>
#include <set>
#include <list>
#include <iterator>
#include <tuple>

#include "misc.hpp"
#include "lex.hpp"

const vec<str> operators = {
    "...", "<<=", ">>=",

    "&&", "||", "==", "!=", "<=", ">=", "++", "--", "<<", ">>",
    "=", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "->", "##",

    "<", ">", "=", "?", ":", "{", "}", "(", ")", ";", "-", "~", "!",
    "+", "/", "*", "%", "&", "|", "^", "[", "]", ",", ".", "#",
};

namespace {
    bool is_digit(int c) {
        return '0' <= c and c <= '9';
    }
    bool is_nondigit(int c) {
        return c == '_' or isalpha(c);
    }
}

class t_lexer {
    const str& src;
    size_t idx;
    t_loc cur_loc;
    t_loc lexeme_loc;
    bool in_include = false;
    std::list<t_lexeme> result;

    void err(const str& s) {
        throw t_compile_error("lexing error: " + s, lexeme_loc);
    }

    _ set_state(const _& x) {
        idx = std::get<0>(x);
        cur_loc = std::get<1>(x);
    }

    _ get_state() {
        return std::make_tuple(idx, cur_loc);
    }

    void check_if_in_include() {
        _ n = result.size();
        if (n < 2) {
            return;
        }
        _& l1 = *next(result.end(), -1);
        in_include = ((*next(result.end(), -2)).uu == "#"
                      and (l1.uu == "identifier" and l1.vv == "include")
                      and (n == 2
                           or (*next(result.end(), -3)).uu == "newline"));
    }
    void push(const str& x, const str& val = "") {
        result.push_back({x, val, lexeme_loc});
    }
    str advance(int d = 1) {
        str res;
        while (d != 0 and idx < src.length()) {
            res += src[idx];
            idx++;
            cur_loc.column++;
            if (src[idx-1] == '\n') {
                cur_loc.line++;
                cur_loc.column = 0;
            }
            d--;
        }
        return res;
    }
    bool end() {
        return idx >= src.length();
    }
    int peek(int n = 0) {
        if (idx + n < src.length()) {
            return src[idx + n];
        }
        return -1;
    }
    bool cmp(const str& x) {
        return src.compare(idx, x.length(), x);
    }
    bool cmp_or(const str& x) {
        _ res = x.find(peek()) != str::npos;
        return res;
    }
    bool match(const str& x) {
        if (src.compare(idx, x.length(), x) == 0) {
            advance(x.length());
            return true;
        } else {
            return false;
        }
    }
    bool operator_() {
        for (_& op : operators) {
            if (match(op)) {
                push(op);
                return true;
            }
        }
        return false;
    }
    bool pp_number() {
        str val;
        if (is_digit(peek())) {
            val += advance();
        } else if (peek() == '.' and is_digit(peek(1))) {
            val += advance();
            val += advance();
        } else {
            return false;
        }
        while (true) {
            if (is_nondigit(peek()) or is_digit(peek()) or peek() == '.') {
                val += advance();
            } else if ((peek() == 'e' or peek() == 'E')
                       and (peek(1) == '+' or peek(1) == '-')) {
                val += advance();
                val += advance();
            } else {
                break;
            }
        }
        push("pp_number", val);
        return true;
    }
    bool identifier() {
        if (not is_nondigit(peek())) {
            return false;
        }
        str val;
        val += advance();
        while (is_nondigit(peek()) or is_digit(peek())) {
            val += advance();
        }
        push("identifier", val);
        check_if_in_include();
        return true;
    }
    bool string_literal() {
        if (peek() != '"') {
            return false;
        }
        str val;
        val += advance();
        while (peek() != '"') {
            if (peek() == '\\' and peek(1) == '"') {
                val += '"';
            } else {
                if (not end() and peek() != '\n') {
                    val += advance();
                } else {
                    err("unbalanced quote");
                }
            }
        }
        val += advance();
        push("string_literal", val);
        return true;
    }
    bool char_constant() {
        if (peek() != '\'') {
            return false;
        }
        str val;
        val += advance();
        while (peek() != '\'') {
            if (peek() == '\\' and peek(1) == '\'') {
                val += '\'';
            } else {
                if (not end() and peek() != '\n') {
                    val += advance();
                } else {
                    err("unbalanced quote");
                }
            }
        }
        val += advance();
        if (val.empty()) {
            err("empty char constant");
        }
        push("char_constant", val);
        return true;
    }
    bool white_space() {
        if (cmp_or(" \n\t\v\f")) {
            if (peek() == '\n') {
                push("newline");
                in_include = false;
            }
            advance();
            return true;
        } else if (match("/*")) {
            while (not match("*/")) {
                if (end()) {
                    err("unterminated comment");
                }
                advance();
            }
            return true;
        } else {
            return false;
        }
    }
    bool header_name() {
        if (not in_include) {
            return false;
        }
        _ old_state = get_state();
        if (peek() == '<') {
            str val;
            val += advance();
            if (peek() == '>') {
                err("empty header name");
            }
            while (peek() != '>') {
                if (peek() == '\n') {
                    set_state(old_state);
                    return false;
                }
                val += advance();
            }
            val += advance();
            push("header_name", val);
            return true;
        } else if (peek() == '"') {
            str val;
            val += advance();
            if (peek() == '"') {
                err("empty header name");
            }
            while (peek() != '"') {
                if (peek() == '\n') {
                    set_state(old_state);
                    return false;
                }
                val += advance();
            }
            val += advance();
            push("header_name", val);
            return true;
        } else {
            return false;
        }
    }
    bool single() {
        push("single", advance());
        return true;
    }
public:
    void go() {
        while (not end()) {
            lexeme_loc = cur_loc;
            (white_space() or header_name() or pp_number() or operator_()
             or char_constant() or string_literal() or identifier()
             or single());
        }
        push("eof");
    }
    const std::list<t_lexeme>& get_result() {
        return result;
    }
    t_lexer(const str& _src)
        : src(_src)
        , idx(0)
        , cur_loc(0, 0)
        , lexeme_loc(cur_loc) {
    }
};

std::list<t_lexeme> lex(const str& src) {
    t_lexer lexer(src);
    lexer.go();
    return lexer.get_result();
}
