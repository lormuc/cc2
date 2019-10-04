#include <unordered_map>

#include "pp.hpp"

typedef std::list<t_lexeme> t_lex_seq;

_ pr(const t_lex_seq& ls) {
    cout << "(";
    if (not ls.empty()) {
        _ initial = true;
        for (_& lx : ls) {
            if (not initial) {
                cout << "`";
            }
            initial = false;
            print_bytes(lx.vv, cout);
        }
    }
    cout << ")";
}

class t_preprocessor {
    t_lex_seq& lex_seq;
    t_lex_seq::iterator pos;

    struct t_macro {
        bool is_func_like;
        std::unordered_map<str, size_t> params;
        t_lex_seq replacement;
    };
    std::unordered_map<str, t_macro> macros;

    bool end() {
        return peek().uu == "eof" or pos == lex_seq.end();
    }
    void skip_ws() {
        if (kind() == "whitespace") {
            advance();
        }
    }
    void advance_ws() {
        advance();
        skip_ws();
    }
    t_lexeme& peek() {
        return *pos;
    }
    void advance() {
        pos++;
    }
    void expect(const str& x) {
        if (kind() != x) {
            throw t_compile_error("expected " + x, peek().loc);
        }
    }
    void err(const str& msg) {
        throw t_compile_error(msg, peek().loc);
    }
    str& val() {
        return peek().vv;
    }
    str& kind() {
        return peek().uu;
    }
    _ skip_ws_nl(_ it) {
        while ((*it).uu == "whitespace" or (*it).uu == "newline") {
            it++;
        }
    }

public:
    t_preprocessor(t_lex_seq& _lex_seq)
        : lex_seq(_lex_seq), pos(lex_seq.begin()) {
    }

    _ define() {
        advance_ws();
        expect("identifier");
        _& id = val();
        advance();
        _ is_func_like = false;
        std::unordered_map<str, size_t> params;
        if (kind() == "(") {
            is_func_like = true;
            advance_ws();
            size_t i = 0;
            while (kind() != ")") {
                if (not params.empty()) {
                    expect(",");
                    advance_ws();
                }
                expect("identifier");
                params[val()] = i;
                advance_ws();
                i++;
            }
            advance();
        }
        skip_ws();
        t_lex_seq replace_list;
        while (kind() != "newline") {
            replace_list.push_back(peek());
            advance();
        }
        advance();
        if (not replace_list.empty()
            and replace_list.back().uu == "whitespace") {
            replace_list.pop_back();
        }
        macros[id] = {is_func_like, params, replace_list};
    }

    _ undef() {
        advance_ws();
        expect("identifier");
        _& id = val();
        macros.erase(id);
        advance_ws();
        expect("newline");
        advance();
    }

    _ directive() {
        _ line_begin = pos;
        advance_ws();
        expect("identifier");
        _& cmd = val();
        if (cmd == "define") {
            define();
        } else if (cmd == "undef") {
            undef();
        } else if (cmd == "include") {
            while (kind() != "newline") {
                advance();
            }
            advance();
        }
        lex_seq.erase(line_begin, pos);
    }

    _ substitute_args(const t_macro& macro, const vec<t_lex_seq>& args) {
        _ res = macro.replacement;
        _ it = res.begin();
        while (it != res.end()) {
            _ _next = next(it);
            if ((*it).uu == "identifier") {
                _ k = macro.params.find((*it).vv);
                if (k != macro.params.end()) {
                    _ idx = (*k).second;
                    it = res.erase(it);
                    res.insert(it, args[idx].begin(), args[idx].end());
                }
            }
            it = _next;
        }
        return res;
    }

    _ collect_args(_& j) {
        vec<t_lex_seq> res;
        t_lex_seq arg;
        _ paren_cnt = 0;
        j++;
        while (true) {
            _& lx = (*j).uu;
            if (lx == ")" and paren_cnt == 0) {
                if (not arg.empty()) {
                    res.push_back(arg);
                    arg.clear();
                }
                break;
            }
            if (paren_cnt == 0 and lx == ",") {
                res.push_back(arg);
                arg.clear();
            }
            if (lx == "(") {
                paren_cnt++;
            }
            if (lx == ")") {
                paren_cnt--;
            }
            if (lx != ",") {
                arg.push_back(*j);
            }
            j++;
        }
        return res;
    }

    _ expand_func_like_macro(_ j, const t_macro& macro) {
        _ args = collect_args(j);
        if (macro.params.size() != args.size()) {
            err("wrong number of arguments");
        }
        j++;
        _ replacement = substitute_args(macro, args);
        pos = lex_seq.erase(pos, j);
        lex_seq.insert(pos, replacement.begin(), replacement.end());
    }

    _ expand() {
        while (not end() and kind() != "newline") {
            if (kind() == "identifier") {
                _& id = peek().vv;
                _ it = macros.find(id);
                if (it == macros.end()) {
                    advance();
                } else {
                    _& macro = (*it).second;
                    if (macro.is_func_like) {
                        _ j = pos;
                        j++;
                        skip_ws_nl(j);
                        if ((*j).uu == "(") {
                            expand_func_like_macro(j, macro);
                        } else {
                            advance();
                        }
                    } else {
                        pos = lex_seq.erase(pos);
                        _& r = macro.replacement;
                        lex_seq.insert(pos, r.begin(), r.end());
                    }
                }
            } else {
                advance();
            }
        }
        if (not end()) {
            advance();
        }
    }

    void scan() {
        while (not end()) {
            if (kind() == "#") {
                directive();
            } else {
                expand();
            }
        }
    }
};

void preprocess(t_lex_seq& ls) {
    _ pp = t_preprocessor(ls);
    pp.scan();
}
