#include <unordered_map>

#include "pp.hpp"

typedef std::list<t_lexeme> t_lex_seq;

class t_preprocessor {
    t_lex_seq& lex_seq;
    t_lex_seq::iterator pos;
    std::unordered_map<str, t_lex_seq> macros;

    bool cmp(const str& s) {
        return pos != lex_seq.end() and (*pos).uu == s;
    }
    bool end() {
        return peek().uu == "eof" or pos == lex_seq.end();
    }
    void erase() {
        pos = lex_seq.erase(pos);
    }
    void skip_ws() {
        if (cmp("whitespace")) {
            advance();
        }
    }
    void advance_ws() {
        advance();
        skip_ws();
    }
    void insert(const t_lex_seq& ls) {
        lex_seq.insert(pos, ls.begin(), ls.end());
    }
    t_lexeme& peek() {
        return *pos;
    }
    void advance() {
        pos++;
    }

public:
    t_preprocessor(t_lex_seq& _lex_seq)
        : lex_seq(_lex_seq), pos(lex_seq.begin()) {
    }

    void go() {
        while (not end()) {
            if (cmp("#")) {
                _ line_begin = pos;
                advance_ws();
                _ cmd = peek().vv;
                if (cmd == "define") {
                    advance_ws();
                    if (not cmp("identifier")) {
                        throw t_compile_error("#define must be followed"
                                              " an identifier", peek().loc);
                    }
                    _ id = peek().vv;
                    advance_ws();
                    t_lex_seq replace_list;
                    while (not cmp("newline")) {
                        replace_list.push_back(peek());
                        advance();
                    }
                    advance();
                    if (not replace_list.empty()
                        and replace_list.back().uu == "whitespace") {
                        replace_list.pop_back();
                    }
                    macros[id] = replace_list;
                } else if (cmd == "include") {
                    while (not cmp("newline")) {
                        advance();
                    }
                    advance();
                }
                lex_seq.erase(line_begin, pos);
            } else {
                while (not end() and not cmp("newline")) {
                    if (cmp("identifier")) {
                        _& id = peek().vv;
                        _ it = macros.find(id);
                        if (it == macros.end()) {
                            advance();
                        } else {
                            erase();
                            insert((*it).second);
                        }
                    } else {
                        advance();
                    }
                }
                if (not end()) {
                    advance();
                }
            }
        }
    }
};

void preprocess(t_lex_seq& ls) {
    _ pp = t_preprocessor(ls);
    pp.go();
}
