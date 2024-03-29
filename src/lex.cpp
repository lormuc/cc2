#include <string>
#include <algorithm>
#include <iostream>
#include <vector>
#include <set>
#include <list>
#include <iterator>
#include <tuple>
#include <ostream>

#include "misc.hpp"
#include "lex.hpp"

const vec<str> punctuators = {
    "...", "<<=", ">>=",

    "&&", "||", "==", "!=", "<=", ">=", "++", "--", "<<", ">>",
    "=", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "->", "##",

    "<", ">", "=", "?", ":", "{", "}", "(", ")", ";", "-", "~", "!",
    "+", "/", "*", "%", "&", "|", "^", "[", "]", ",", ".", "#",
};

bool is_digit(int c) {
    return '0' <= c and c <= '9';
}

bool is_nondigit(int c) {
    return c == '_' or isalpha(c);
}

bool is_whitespace(char ch) {
    return ch == ' ' or ch == '\t' or ch == '\v' or ch == '\f' or ch == '\n';
}

void print(const std::list<t_pp_lexeme>& ls, std::ostream& os,
           const str& separator) {
    _ initial = true;
    for (_& lx : ls) {
        if (not initial) {
            os << separator;
        }
        initial = false;
        os << lx.val;
    }
}

str pp_kind(const str& val) {
    _ ch = val[0];
    if (is_nondigit(ch)) {
        return "identifier";
    }
    if (ch == '"') {
        return "string_literal";
    }
    if (ch == '\'') {
        return "char_constant";
    }
    if (ch == '\n') {
        return "newline";
    }
    if (is_whitespace(ch)) {
        return "whitespace";
    }
    _ it = std::find(punctuators.begin(), punctuators.end(), val);
    if (it != punctuators.end()) {
        return val;
    }
    if (ch == '.' or is_digit(ch)) {
        return "pp_number";
    }
    return "single";
}

class t_lexer {
    const str& src;
    size_t idx;
    t_loc cur_loc;
    t_loc lexeme_loc;
    bool in_include = false;
    std::list<t_pp_lexeme> result;

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
        _& l1 = (*next(result.end(), -1)).val;
        _& l2 = (*next(result.end(), -2)).val;
        _ nl3 = (n == 2 or (*next(result.end(), -3)).val == "\n");
        in_include = (nl3 and l2 == "#" and l1 == "include");
    }
    void push(const str& x, const str& val) {
        result.push_back({x, val, lexeme_loc, {}});
    }
    _ advance_real() {
        idx++;
        cur_loc.inc(src[idx-1] == '\n');
    }
    str advance(int d = 1) {
        str res;
        while (d != 0) {
            while (src.compare(idx, 2, "\\\n") == 0) {
                advance_real();
                advance_real();
            }
            if (idx >= src.length()) {
                break;
            }
            res += src[idx];
            advance_real();
            d--;
        }
        return res;
    }
    bool end() {
        return idx >= src.length();
    }
    int peek(int n = 0) {
        _ i = 0;
        _ j = idx;
        while (true) {
            while (src.compare(j, 2, "\\\n") == 0) {
                j += 2;
            }
            if (j >= src.length()) {
                return -1;
            }
            if (i == n) {
                return src[j];
            }
            i++;
            j++;
        }
    }
    bool compare(const str& x) {
        _ old_idx = idx;
        _ old_loc = cur_loc;
        _ success = true;
        for (_ ch : x) {
            if (end() or peek() != ch) {
                success = false;
            }
            advance();
        }
        idx = old_idx;
        cur_loc = old_loc;
        return success;
    }
    bool match(const str& x) {
        if (compare(x)) {
            advance(x.length());
            return true;
        } else {
            return false;
        }
    }
    bool punctuator() {
        for (_& pu : punctuators) {
            if (match(pu)) {
                push(pu, pu);
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
            if (compare("\\\"") or compare("\\\\")) {
                val += advance(2);
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
            if (compare("\\\'") or compare("\\\\")) {
                val += advance(2);
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
    bool whitespace() {
        if (peek() == '\n') {
            push("newline", advance());
            in_include = false;
            return true;
        } else if ((is_whitespace(peek()) and peek() != '\n')
                   or compare("/*") or compare("//")) {
            str val;
            while (true) {
                if (match("/*")) {
                    while (not match("*/")) {
                        if (end()) {
                            err("unterminated comment");
                        }
                        advance();
                    }
                    val += ' ';
                } else if (match("//")) {
                    while (peek() != '\n') {
                        if (end()) {
                            err("unterminated comment");
                        }
                        advance();
                    }
                    val += ' ';
                } else if (is_whitespace(peek()) and peek() != '\n') {
                    val += advance();
                } else {
                    break;
                }
            }
            push("whitespace", val);
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
            (whitespace() or header_name() or pp_number() or punctuator()
             or char_constant() or string_literal() or identifier()
             or single());
        }
        if (not result.empty() and result.back().kind != "newline") {
            push("newline", "\n");
        }
        push("eof", "");
        for (_ it = result.begin(); (*it).kind != "eof";) {
            if ((*it).val == "L"
                and ((*next(it)).val[0] == '\''
                     or ((*next(it)).val[0] == '"'))) {
                it = result.erase(it);
            } else {
                it++;
            }
        }
    }
    std::list<t_pp_lexeme> get_result() {
        return std::move(result);
    }
    t_lexer(size_t file_idx, const t_file_manager& fm)
        : src(fm.get_file_contents(file_idx))
        , idx(0)
        , cur_loc(file_idx, 1, 0)
        , lexeme_loc(cur_loc) {
    }
};

std::list<t_pp_lexeme> lex(size_t file_idx, const t_file_manager& fm) {
    t_lexer lexer(file_idx, fm);
    lexer.go();
    return lexer.get_result();
}
