#include <string>
#include <algorithm>
#include <iostream>
#include <vector>

#include "misc.hpp"
#include "lex.hpp"

using namespace std;

namespace {
    auto err(const string& str, t_loc loc = t_loc()) {
        throw t_compile_error(str, loc);
    }

    auto is_id_char(char ch) {
        return ch == '_' or isalnum(ch);
    }

    auto is_dec_digit(char ch) {
        return '0' <= ch and ch <= '9';
    }

    auto in(char ch, char a, char b) {
        return a <= ch and ch <= b;
    }

    auto is_hex_digit(char ch) {
        return in(ch, '0', '9') or in(ch, 'a', 'f') or in(ch, 'A', 'F');
    }

    auto is_oct_digit(char ch) {
        return in(ch, '0', '7');
    }
}

vector<t_lexeme> lex(const string& source) {
    vector<t_lexeme> res;
    auto i = source.begin();
    t_loc loc = {0, 0};
    auto old_loc = loc;
    auto push = [&](const string& x, const string& y = "") {
        t_lexeme l;
        l.uu = x;
        l.vv = y;
        l.loc.line = old_loc.line;
        l.loc.column = old_loc.column;
        res.push_back(l);
    };
    auto advance = [&](unsigned d = 1) {
        while (d != 0) {
            i++;
            loc.column++;
            if (*(i-1) == '\n') {
                loc.line++;
                loc.column = 0;
            }
            d--;
        }
    };
    while (i != source.end()) {
        old_loc = loc;
        vector<string> tt = {
            "...",
            "&&", "||", "==", "!=", "<=", ">=", "++", "--", "<<", ">>",
            "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|=",
            "<", ">", "=", "?", ":", "{", "}", "(", ")", ";", "-", "~", "!",
            "+", "/", "*", "%", "&", "[", "]", ",", ".",
        };
        auto found = false;
        for (auto& t : tt) {
            if (equal(t.begin(), t.end(), i)) {
                push(t, t);
                found = true;
                advance(t.size());
                break;
            }
        }
        if (found) {
            continue;
        }
        auto ch = *i;
        advance();
        string val;
        val += ch;
        if (is_dec_digit(ch)) {
            while (i != source.end() and isdigit(*i)) {
                val += *i;
                advance();
            }
            if (i != source.end()) {
                if (*i == '.') {
                    val += *i;
                    advance();
                    while (i != source.end() and isdigit(*i)) {
                        val += *i;
                        advance();
                    }
                    push("floating_constant", val);
                } else {
                    push("integer_constant", val);
                }
            }
        } else if (is_id_char(ch)) {
            while (i != source.end() and is_id_char(*i)) {
                val += *i;
                advance();
            }
            vector<string> keywords = {
                "int", "return", "if", "else", "while", "for", "do",
                "continue", "break", "struct", "float",
                "char", "unsigned", "void", "auto", "case", "const",
                "default", "double", "enum", "extern", "goto", "long",
                "register", "short", "signed", "sizeof", "static", "struct",
                "switch", "typedef", "union", "volatile"
            };
            if (has(keywords, val)) {
                push(val, val);
            } else {
                push("identifier", val);
            }
        } else if (ch == '"') {
            string val;
            while (*i != '"') {
                if (*i == '\\') {
                    advance();
                    auto j = i;
                    if (*j == 'x') {
                        j++;
                        while (is_hex_digit(*j)) {
                            j++;
                        }
                        val += char(stoul(string(i, j)));
                    } else if (is_oct_digit(*j)) {
                        while (is_oct_digit(*j)) {
                                j++;
                        }
                        val += char(stoul(string(i, j)));
                    } else if (*j == '\'' or *j == '\"' or *j == '\\') {
                        val += *j;
                        j++;
                    } else if (*j == 'n') {
                        val += '\n';
                        j++;
                    }
                    advance(j - i);
                } else {
                    val += *i;
                    advance();
                }
            }
            push("string_literal", val);
            advance();
        }
    }
    push("eof");
    return res;
}
