#include <unordered_map>
#include <unordered_set>
#include <cassert>

#include "pp.hpp"
#include "ast.hpp"
#include "exp.hpp"

std::list<t_lexeme> convert_lexemes(t_pp_c_iter it, t_pp_c_iter fin) {
    static const std::unordered_set<str> keywords = {
        "int", "return", "if", "else", "while", "for", "do",
        "continue", "break", "struct", "float",
        "char", "unsigned", "void", "auto", "case", "const",
        "default", "double", "enum", "extern", "goto", "long",
        "register", "short", "signed", "sizeof", "static", "struct",
        "switch", "typedef", "union", "volatile"
    };

    std::list<t_lexeme> res;
    for (; it != fin; it++) {
        _ kind = (*it).kind;
        _ val = (*it).val;
        if (kind == "newline" or kind == "whitespace") {
            continue;
        }
        if (kind == "identifier") {
            if (keywords.count(val) != 0) {
                kind = val;
            }
        } else if (kind == "pp_number") {
            if (val.find('.') != str::npos or
                ((val.find('e') != str::npos or val.find('E') != str::npos)
                 and not (val.length() >= 2
                          and (val[1] == 'x' or val[1] == 'X')))) {
                kind = "floating_constant";
            } else {
                kind = "integer_constant";
            }
        } else if (kind == "char_constant" or kind == "string_literal") {
            val = val.substr(1, val.length() - 2);
        }
        res.push_back({kind, val, (*it).loc});
    }
    return res;
}

namespace {
    _ match(const str& s0, _& idx, const str& s1) {
        if (s0.compare(idx, s1.length(), s1) == 0) {
            idx += s1.length();
            return true;
        }
        return false;
    }

    _ is_octal_digit(char digit) {
        return '0' <= digit and digit <= '7';
    }

    _ is_hex_digit(char digit) {
        return (('0' <= digit and digit <= '9')
                or ('a' <= digit and digit <= 'f')
                or ('A' <= digit and digit <= 'F'));
    }

    _ octal_digit_to_int(char digit) {
        return digit - '0';
    }

    _ hex_digit_to_int(char digit) {
        if (digit >= 'a' and digit <= 'f') {
            return digit - 'a';
        }
        if (digit >= 'A' and digit <= 'F') {
            return digit - 'A';
        }
        return digit - '0';
    }
}

void escape_seqs(t_pp_iter it, t_pp_iter fin) {
    for (; it != fin; it++) {
        _& val = (*it).val;
        if ((*it).kind == "char_constant" or (*it).kind == "string_literal") {
            str new_str;
            size_t i = 0;
            while (i < val.length()) {
                if (match(val, i, "\\")) {
                    if (i < val.length() and is_octal_digit(val[i])) {
                        _ ch = octal_digit_to_int(val[i]);
                        i++;
                        for (_ j = 1; j < 3; j++) {
                            if (not (i < val.length()
                                     and is_octal_digit(val[i]))) {
                                break;
                            }
                            ch = 8 * ch + octal_digit_to_int(val[i]);
                            i++;
                        }
                        new_str += char(ch);
                    } else if (i < val.length() and val[i] == 'x') {
                        i++;
                        _ ch = 0;
                        while (i < val.length() and is_hex_digit(val[i])) {
                            ch = 16 * ch + hex_digit_to_int(val[i]);
                            i++;
                        }
                        new_str += char(ch);
                    } else if (match(val, i, "n")) {
                        new_str += "\n";
                    } else if (match(val, i, "a")) {
                        new_str += "\a";
                    } else if (match(val, i, "b")) {
                        new_str += "\b";
                    } else if (match(val, i, "f")) {
                        new_str += "\f";
                    } else if (match(val, i, "r")) {
                        new_str += "\r";
                    } else if (match(val, i, "t")) {
                        new_str += "\t";
                    } else if (match(val, i, "v")) {
                        new_str += "\v";
                    } else if (match(val, i, "\"")) {
                        new_str += "\"";
                    } else if (match(val, i, "\\")) {
                        new_str += "\\";
                    } else if (match(val, i, "'")) {
                        new_str += "'";
                    } else if (match(val, i, "?")) {
                        new_str += "?";
                    }
                } else {
                    new_str += val[i];
                    i++;
                }
            }
            val = new_str;
        }
    }
}

struct t_macro {
    bool is_func_like;
    std::unordered_map<str, size_t> params;
    t_pp_seq replacement;
};

namespace {
    _ step(_& it) {
        it++;
        if ((*it).kind == "whitespace") {
            it++;
        }
    }

    _ expand_defined(t_pp_seq& ls, t_pp_iter i, t_pp_iter fin,
                     const _& macros) {
        while (i != fin) {
            if ((*i).val == "defined") {
                _ loc = (*i).loc;
                _ j = i;
                step(i);
                _ in_parens = ((*i).val == "(");
                if (in_parens) {
                    step(i);
                }
                _ id = (*i).val;
                if (in_parens) {
                    step(i);
                }
                i++;
                _ m_it = macros.find(id);
                j = ls.erase(j, i);
                if (m_it == macros.end()) {
                    ls.insert(i, {"pp_number", "0", loc, {}});
                } else {
                    ls.insert(i, {"pp_number", "1", loc, {}});
                }
            } else {
                i++;
            }
        }
    }

    _ is_eof(_ i) {
        return (*i).val == "";
    }

    _ skip_ws(_ i, _ fin) {
        if (i != fin and (*i).kind == "whitespace") {
            i++;
        }
        return i;
    }

    _ collect_args(_& j, _ finish) {
        vec<t_pp_seq> res;
        t_pp_seq arg;
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
                _ prv_ws = false;
                if (not arg.empty()) {
                    if (arg.back().kind == "whitespace") {
                        if (lx == "newline" or lx == "whitespace") {
                            prv_ws = true;
                        }
                    }
                }
                if (not prv_ws) {
                    arg.push_back(*j);
                    if (arg.back().val == "\n") {
                        arg.back().kind = "whitespace";
                    }
                    if (arg.back().kind == "whitespace") {
                        arg.back().val = " ";
                    }
                }
            }
            j++;
        }
        return res;
    }

    void expand(t_pp_seq& ls, t_pp_iter i, t_pp_iter finish,
                const std::unordered_map<str, t_macro>& macros);

    _ glue(t_pp_seq& ls, t_pp_seq rs) {
        if (ls.back().kind == "whitespace") {
            ls.pop_back();
        }
        if (rs.front().kind == "whitespace") {
            rs.pop_front();
        }
        _& x = ls.back();
        _& y = rs.front();
        std::set<str> hs;
        std::set_intersection(x.hide_set.begin(), x.hide_set.end(),
                              y.hide_set.begin(), y.hide_set.end(),
                              std::inserter(hs, hs.begin()));
        x.hide_set = hs;
        x.val += y.val;
        x.kind = pp_kind(x.val);
        rs.pop_front();
        ls.splice(ls.end(), rs);
    }

    _ stringize(_ ls) {
        if (not ls.empty() and ls.back().kind == "whitespace") {
            ls.pop_back();
        }
        if (not ls.empty() and ls.front().kind == "whitespace") {
            ls.pop_front();
        }
        assert(not ls.empty());
        str val;
        val += "\"";
        for (_& lx : ls) {
            if (lx.kind == "char_constant" or lx.kind == "string_literal") {
                for (_ ch : lx.val) {
                    if (ch == '\\') {
                        val += "\\\\";
                    } else if (ch == '"') {
                        val += "\\\"";
                    } else {
                        val += ch;
                    }
                }
            } else {
                val += lx.val;
            }
        }
        val += "\"";
        return t_pp_lexeme{"string_literal", val, ls.front().loc, {}};
    }

    void substitute(_ i, _ finish, const std::unordered_map<str, size_t>& fp,
                    const vec<t_pp_seq>& ap, const _& hs, _& os,
                    const _& macros) {
        if (i == finish) {
            for (_& x : os) {
                x.hide_set.insert(hs.begin(), hs.end());
            }
            return;
        }
        if ((*i).val == "#") {
            _ i1 = skip_ws(next(i), finish);
            if (i1 != finish) {
                _ p_it = fp.find((*i1).val);
                if (p_it != fp.end()) {
                    _ idx = (*p_it).second;
                    os.push_back(stringize(ap[idx]));
                    i1++;
                    substitute(i1, finish, fp, ap, hs, os, macros);
                    return;
                }
            }
        }
        if ((*i).val == "##") {
            _ i1 = skip_ws(next(i), finish);
            if (i1 != finish) {
                _ p_it = fp.find((*i1).val);
                if (os.empty()) {
                    throw t_compile_error("## cannot occur at the beginning or"
                                          " at the end of a replacement list",
                                          (*i).loc);
                }
                if (p_it != fp.end()) {
                    _ idx = (*p_it).second;
                    glue(os, ap[idx]);
                } else {
                    glue(os, {*i1});
                }
                i1++;
                substitute(i1, finish, fp, ap, hs, os, macros);
                return;
            }
        }

        _ p_it = fp.find((*i).val);
        if (p_it != fp.end()) {
            _ idx = (*p_it).second;
            _ arg = ap[idx];
            _ i1 = skip_ws(next(i), finish);
            if (i1 != finish and (*i1).val == "##") {
                os.splice(os.end(), arg);
                substitute(i1, finish, fp, ap, hs, os, macros);
            } else {
                expand(arg, arg.begin(), arg.end(), macros);
                os.splice(os.end(), arg);
                i++;
                substitute(i, finish, fp, ap, hs, os, macros);
            }
            return;
        }
        os.push_back(*i);
        i++;
        substitute(i, finish, fp, ap, hs, os, macros);
    }

    void expand(t_pp_seq& ls, t_pp_iter i, t_pp_iter finish,
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
            t_pp_seq r;
            _& mr = macro.replacement;
            substitute(mr.begin(), mr.end(), macro.params, args, nhs, r,
                       macros);
            i = ls.erase(i, j);
            i = ls.insert(i, r.begin(), r.end());
            expand(ls, i, finish, macros);
        } else {
            _ nhs = hs;
            nhs.insert((*i).val);
            t_pp_seq r;
            _& mr = macro.replacement;
            substitute(mr.begin(), mr.end(), {}, {}, nhs, r, macros);
            i = ls.erase(i);
            i = ls.insert(i, r.begin(), r.end());
            expand(ls, i, finish, macros);
        }
    }

    _ find_newline(_ it) {
        while ((*it).kind != "newline") {
            it++;
        }
        return it;
    }

    _ pp_hash(_& it) {
        _ jt = it;
        if ((*jt).kind == "whitespace") {
            jt++;
        }
        if ((*jt).val != "#") {
            return false;
        }
        step(jt);
        it = jt;
        return true;
    }

    _ cmd(_& it) {
        if (not pp_hash(it)) {
            return str();
        }
        _ res = (*it).val;
        step(it);
        return res;
    }
}

class t_preprocessor {
    t_pp_seq& lex_seq;
    t_pp_iter pos;
    std::unordered_map<str, t_macro> macros;

    void advance() {
        step(pos);
    }
    void skip(bool ws = true) {
        pos = lex_seq.erase(pos);
        if (ws and (*pos).kind == "whitespace") {
            pos = lex_seq.erase(pos);
        }
    }
    void expect(const str& x) {
        if ((*pos).kind != x) {
            throw t_compile_error("expected " + x, (*pos).loc);
        }
    }
    void err(const str& msg) {
        throw t_compile_error(msg, (*pos).loc);
    }

    _ command(const str& name, bool erase = true) {
        _ it = pos;
        if (not pp_hash(it)) {
            return false;
        }
        if (not ((*it).kind == "identifier" and (*it).val == name)) {
            return false;
        }
        step(it);
        if (erase) {
            pos = lex_seq.erase(pos, it);
        } else {
            pos = it;
        }
        return true;
    }

    _ define() {
        if (not command("define")) {
            return false;
        }
        _ id = (*pos).val;
        skip(false);
        _ is_func_like = false;
        std::unordered_map<str, size_t> params;
        if ((*pos).kind == "(") {
            is_func_like = true;
            skip();
            size_t i = 0;
            while ((*pos).kind != ")") {
                if (not params.empty()) {
                    expect(",");
                    skip();
                }
                expect("identifier");
                params[(*pos).val] = i;
                skip();
                i++;
            }
            skip();
        } else {
            if ((*pos).kind == "whitespace") {
                skip(false);
            }
        }
        t_pp_seq replace_list;
        while ((*pos).kind != "newline") {
            replace_list.push_back(*pos);
            skip(false);
        }
        skip(false);
        if (not replace_list.empty()
            and replace_list.back().kind == "whitespace") {
            replace_list.pop_back();
        }
        macros[id] = {is_func_like, params, replace_list};
        return true;
    }

    _ undef() {
        if (not command("undef")) {
            return false;
        }
        macros.erase((*pos).val);
        skip();
        skip();
        return true;
    }

    _ simple_lines() {
        _ it = pos;
        if (is_eof(it) or pp_hash(it)) {
            return false;
        }
        while (not is_eof(it)) {
            _ jt = it;
            if (pp_hash(jt)) {
                break;
            }
            it = find_newline(it);
            it++;
        }
        expand(lex_seq, pos, it, macros);
        pos = it;
        return true;
    }

    _ include() {
        if (not command("include")) {
            return false;
        }
        while ((*pos).kind != "newline") {
            skip(false);
        }
        return true;
    }

    _ empty_directive() {
        if (not command("\n")) {
            return false;
        }
        return true;
    }

    _ control_line() {
        return include() or define() or undef() or empty_directive();
    }

    _ if_group() {
        _ line_begin = pos;
        if (not command("if", false)) {
            return false;
        }
        _ line_end = find_newline(pos);
        pos--;
        expand_defined(lex_seq, next(pos), line_end, macros);
        expand(lex_seq, next(pos), line_end, macros);
        pos++;
        escape_seqs(pos, line_end);
        for (_ it = pos; it != line_end; it++) {
            if ((*it).kind == "identifier") {
                (*it).kind = "pp_number";
                (*it).val = "0";
            }
        }
        _ exp_ls = convert_lexemes(pos, line_end);
        exp_ls.push_back({"eof", "", (*line_end).loc});
        _ exp_ast = parse_exp(exp_ls.begin());
        t_ctx exp_ctx;
        _ val = gen_exp(exp_ast, exp_ctx);
        _ cond_is_true = (val.u_val() != 0);
        pos = lex_seq.erase(line_begin, line_end);
        skip(false);
        if (cond_is_true) {
            group();
        } else {
            _ block_begin = pos;
            _ level = 0;
            while (true) {
                _ i = pos;
                _ cmd_ = cmd(i);
                if (cmd_ == "if") {
                    level++;
                } else if (cmd_ == "endif") {
                    if (level == 0) {
                        break;
                    } else {
                        level--;
                    }
                }
                pos = find_newline(pos);
                pos++;
            }
            lex_seq.erase(block_begin, pos);
        }
        return true;
    }

    _ endif_line() {
        if (command("endif")) {
            return false;
        }
        skip();
        return true;
    }

    _ if_section() {
        if (not if_group()) {
            return false;
        }
        // while (elif_group()) {
        // }
        // else_group();
        endif_line();
        return true;
    }

    _ group_part() {
        return if_section() or control_line() or simple_lines();
    }

public:
    t_preprocessor(t_pp_seq& ls)
        : lex_seq(ls), pos(ls.begin()) {
    }

    void group() {
        while (group_part()) {
        }
    }

    void scan() {
        group();
        expect("eof");
    }
};

void preprocess(t_pp_seq& ls) {
    _ pp = t_preprocessor(ls);
    pp.scan();
}
