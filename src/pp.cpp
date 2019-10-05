#include <unordered_map>

#include "pp.hpp"

typedef std::list<t_pp_lexeme> t_lex_seq;

struct t_macro {
    bool is_func_like;
    std::unordered_map<str, size_t> params;
    t_lex_seq replacement;
};

namespace {
    _ collect_args(_& j, _ finish) {
        vec<t_lex_seq> res;
        t_lex_seq arg;
        _ paren_cnt = 0;
        _ loc = (*j).loc;
        while (true) {
            if (j == finish) {
                throw t_compile_error("unmatched (", loc);
            }
            _& lx = (*j).kind;
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
            if (not (lx == "," and paren_cnt == 0)) {
                arg.push_back(*j);
            }
            j++;
        }
        return res;
    }

    void expand(t_lex_seq& ls, t_lex_seq::iterator i,
                t_lex_seq::iterator finish,
                const std::unordered_map<str, t_macro>& macros);

    void substitute(_ i, _ finish, const std::unordered_map<str, size_t>& fp,
                    const vec<t_lex_seq>& ap, const _& hs, _& os,
                    const _& macros) {
        if (i == finish) {
            for (_& x : os) {
                x.hide_set.insert(hs.begin(), hs.end());
            }
            return;
        }
        _ j = fp.find((*i).val);
        if (j != fp.end()) {
            _ arg = ap[(*j).second];
            expand(arg, arg.begin(), arg.end(), macros);
            os.splice(os.end(), arg);
            i++;
            substitute(i, finish, fp, ap, hs, os, macros);
            return;
        }
        os.push_back(*i);
        i++;
        substitute(i, finish, fp, ap, hs, os, macros);
    }

    void expand(t_lex_seq& ls, t_lex_seq::iterator i,
                t_lex_seq::iterator finish,
                const std::unordered_map<str, t_macro>& macros) {
        if (i == finish) {
            return;
        }
        _& hs = (*i).hide_set;
        if ((*i).kind != "identifier" or hs.count((*i).val) != 0) {
            i++;
            expand(ls, i, finish, macros);
            return;
        }
        _ macro_it = macros.find((*i).val);
        if (macro_it == macros.end()) {
            i++;
            expand(ls, i, finish, macros);
            return;
        }
        _& macro = (*macro_it).second;
        if (macro.is_func_like) {
            _ j = i;
            j++;
            while (j != finish and ((*j).kind == "whitespace"
                                    or (*j).kind == "newline")) {
                j++;
            }
            if (j == finish or (*j).kind != "(") {
                i++;
                expand(ls, i, finish, macros);
                return;
            }
            j++;
            _ args = collect_args(j, finish);
            if (macro.params.size() != args.size()) {
                throw t_compile_error("wrong number of arguments", (*i).loc);
            }
            _& rparen_hs = (*j).hide_set;
            j++;
            std::set<str> nhs;
            std::set_intersection(hs.begin(), hs.end(),
                                  rparen_hs.begin(), rparen_hs.end(),
                                  std::inserter(nhs, nhs.begin()));
            nhs.insert((*i).val);
            t_lex_seq r;
            _& mr = macro.replacement;
            substitute(mr.begin(), mr.end(), macro.params, args, nhs, r,
                       macros);
            i = ls.erase(i, j);
            i = ls.insert(i, r.begin(), r.end());
            expand(ls, i, finish, macros);
        } else {
            _ nhs = hs;
            nhs.insert((*i).val);
            t_lex_seq r;
            _& mr = macro.replacement;
            substitute(mr.begin(), mr.end(), {}, {}, nhs, r, macros);
            i = ls.erase(i);
            i = ls.insert(i, r.begin(), r.end());
            expand(ls, i, finish, macros);
        }
    }
}

class t_preprocessor {
    t_lex_seq& lex_seq;
    t_lex_seq::iterator pos;

    std::unordered_map<str, t_macro> macros;

    bool end() {
        return peek().kind == "eof" or pos == lex_seq.end();
    }
    void skip_ws() {
        if (peek().kind == "whitespace") {
            advance();
        }
    }
    void advance_ws() {
        advance();
        skip_ws();
    }
    t_pp_lexeme& peek() {
        return *pos;
    }
    void advance() {
        pos++;
    }
    void expect(const str& x) {
        if (peek().kind != x) {
            throw t_compile_error("expected " + x, peek().loc);
        }
    }
    void err(const str& msg) {
        throw t_compile_error(msg, peek().loc);
    }
    str& val() {
        return peek().val;
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
        if (peek().kind == "(") {
            is_func_like = true;
            advance_ws();
            size_t i = 0;
            while (peek().kind != ")") {
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
        while (peek().kind != "newline") {
            replace_list.push_back(peek());
            advance();
        }
        advance();
        if (not replace_list.empty()
            and replace_list.back().kind == "whitespace") {
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
            while (peek().kind != "newline") {
                advance();
            }
            advance();
        }
        lex_seq.erase(line_begin, pos);
    }

    void scan() {
        while (not end()) {
            if (peek().kind == "#") {
                directive();
            } else {
                _ k = pos;
                while (true) {
                    if ((*k).kind == "eof") {
                        break;
                    }
                    if (k != lex_seq.begin() and (*k).kind == "#"
                        and (*std::next(k, -1)).kind == "newline") {
                        break;
                    }
                    k++;
                }
                expand(lex_seq, pos, k, macros);
                pos = k;
            }
        }
    }
};

void preprocess(t_lex_seq& ls) {
    _ pp = t_preprocessor(ls);
    pp.scan();
}
