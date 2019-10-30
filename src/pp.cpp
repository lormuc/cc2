#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <ctime>

#include "pp.hpp"
#include "ast.hpp"
#include "exp.hpp"
#include "lex.hpp"

namespace {
    _ unwrap(const str& x) {
        return x.substr(1, x.length() - 2);
    }
}

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
            val = unwrap(val);
        }
        res.push_back({kind, val, (*it).loc});
    }
    return res;
}

_ print_line(t_pp_iter pos) {
    cout << "`";
    while ((*pos).kind != "newline") {
        cout << (*pos).val;
        pos++;
    }
    cout << "`\n";
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
            return digit - 'a' + 10;
        }
        if (digit >= 'A' and digit <= 'F') {
            return digit - 'A' + 10;
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

namespace {
    _ str_lit(const str& x) {
        str res;
        for (_ ch : x) {
            if (ch == '\\') {
                res += "\\\\";
            } else if (ch == '"') {
                res += "\\\"";
            } else {
                res += ch;
            }
        }
        return res;
    }
}

struct t_macro {
    t_pp_seq replacement;
    bool is_func_like = false;
    std::unordered_map<str, size_t> params = {};
};

struct t_macros_find_result {
    bool success;
    t_macro macro = t_macro();
};

class t_macros {
    std::unordered_map<str, t_macro> macros;
    t_file_manager& file_manager;

public:
    t_macros(t_file_manager& file_manager_)
        : file_manager(file_manager_) {
        macros["__STDC__"] = {{t_pp_lexeme{"pp_number", "1"}}};
        macros["__x86_64__"] = {{}};
        macros["__STRICT_ANSI__"] = {{}};
    }

    void erase(const t_pp_lexeme& lx) {
        macros.erase(lx.val);
    }

    void put(const str& id, bool is_func_like,
             const std::unordered_map<str, size_t>& params,
             const t_pp_seq& replace_list) {
        macros[id] = {replace_list, is_func_like, params};
    }

    t_macros_find_result find(const t_pp_lexeme& lx) const {
        _ it = macros.find(lx.val);
        if (it != macros.end()) {
            return t_macros_find_result{true, (*it).second};
        }
        _& id = lx.val;
        str val;
        if (id == "__LINE__") {
            val = std::to_string(lx.loc.line());
        } else if (id == "__FILE__") {
            _ path = file_manager.get_path(lx.loc.file_idx());
            val = "\"" + str_lit(path) + "\"";
        } else if (id == "__TIME__") {
            _ raw_time = std::time(0);
            _ ti = std::localtime(&raw_time);
            char buf[64];
            strftime(buf, sizeof(buf), "%T", ti);
            val = "\"" + str(buf) + "\"";
        } else if (id == "__DATE__") {
            _ raw_time = std::time(0);
            _ ti = std::localtime(&raw_time);
            char buf[64];
            strftime(buf, sizeof(buf), "%b %e %Y", ti);
            val = "\"" + str(buf) + "\"";
        } else {
            return t_macros_find_result{false};
        }
        _ res_lx = t_pp_lexeme{pp_kind(val), val, lx.loc};
        return t_macros_find_result{true, {{res_lx}}};
    }
};

namespace {
    _ step(_& it) {
        it++;
        if ((*it).kind == "whitespace") {
            it++;
        }
    }

    _ expect(const str& kind, t_pp_iter it) {
        constrain((*it).kind == kind,
                  "expected " + kind + ", got " + (*it).kind,
                  (*it).loc);
    }

    _ is_eof(_ i) {
        return (*i).kind == "eof";
    }

    _ skip_ws(_ i, _ fin) {
        if (i != fin and (*i).kind == "whitespace") {
            i++;
        }
        return i;
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

    t_pp_iter expand_defined(t_pp_seq& ls, t_pp_iter i, t_pp_iter fin,
                             const _& macros) {
        _ new_i = i;
        _ initial = true;
        while (i != fin) {
            if ((*i).val == "defined") {
                _ loc = (*i).loc;
                _ j = i;
                step(i);
                _ in_parens = ((*i).val == "(");
                if (in_parens) {
                    step(i);
                }
                expect("identifier", i);
                _ id = *i;

                if (in_parens) {
                    step(i);
                }
                i++;
                _ macro_find_res = macros.find(id);
                _ is_defined_str = str(macro_find_res.success ? "1" : "0");
                j = ls.erase(j, i);
                _ k = ls.insert(i, {"pp_number", is_defined_str, loc, {}});
                if (initial) {
                    new_i = k;
                }
            } else {
                i++;
            }
            initial = false;
        }
        return new_i;
    }

    _ collect_args(_& j, _ finish) {
        vec<t_pp_seq> res;
        t_pp_seq arg;
        _ paren_cnt = 0;
        _ loc = (*j).loc;
        while (true) {
            constrain(j != finish, "unmatched (", loc);
            _& lx = (*j).kind;
            if (paren_cnt == 0) {
                if (lx == "," or lx == ")") {
                    if (arg.empty()) {
                        _ pm = t_pp_lexeme{"placemarker", "", (*j).loc};
                        arg.push_back(pm);
                    } else if (arg.size() == 1
                               and arg.front().kind == "whitespace") {
                        _ pm = t_pp_lexeme{"placemarker", "", arg.front().loc};
                        arg.clear();
                        arg.push_back(pm);
                    }
                    res.push_back(arg);
                    arg.clear();
                }
                if (lx == ")") {
                    break;
                }
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

    t_pp_iter expand(t_pp_seq& ls, t_pp_iter i, t_pp_iter finish,
                     const t_macros& macros);

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
        if (x.val == "") {
            x.kind = "placemarker";
        } else {
            x.kind = pp_kind(x.val);
        }
        rs.pop_front();
        ls.splice(ls.end(), rs);
    }

    _ stringize(t_pp_seq ls) {
        if (not ls.empty() and ls.back().kind == "whitespace") {
            ls.pop_back();
        }
        if (not ls.empty() and ls.front().kind == "whitespace") {
            ls.pop_front();
        }
        assert(not ls.empty());
        str val;
        val += "\"";
        if (ls.front().kind != "placemarker") {
            for (_& lx : ls) {
                if (lx.kind == "char_constant"
                    or lx.kind == "string_literal") {
                    val += str_lit(lx.val);
                } else {
                    val += lx.val;
                }
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
                constrain(not os.empty(),
                          "## cannot occur at the beginning or at the end of "
                          "a replacement list", (*i).loc);
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

    t_pp_iter expand(t_pp_seq& ls, t_pp_iter i, t_pp_iter finish,
                     const t_macros& macros) {
        _ macro = t_macro();
        _ i0 = i;
        _ initial = true;
        while (true) {
            if (i == finish) {
                return i0;
            }
            if ((*i).kind == "identifier"
                and (*i).hide_set.count((*i).val) == 0) {
                _ macro_find_res = macros.find(*i);
                if (macro_find_res.success) {
                    macro = macro_find_res.macro;
                    break;
                }
            }
            i++;
            initial = false;
        }
        _& hs = (*i).hide_set;
        if (macro.is_func_like) {
            _ j = i;
            j++;
            while (j != finish and ((*j).kind == "whitespace"
                                    or (*j).kind == "newline")) {
                j++;
            }
            if (j == finish or (*j).kind != "(") {
                expand(ls, next(i), finish, macros);
                return i0;
            }
            j++;
            _ args = collect_args(j, finish);
            if (macro.params.size() == 0 and args.size() == 1
                and args.front().front().kind == "placemarker") {
                args.clear();
            }
            constrain(macro.params.size() == args.size(),
                      "wrong number of arguments", (*i).loc);
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
            for (_ m = r.begin(); m != r.end();) {
                if ((*m).kind == "placemarker") {
                    m = r.erase(m);
                } else {
                    m++;
                }
            }
            i = ls.erase(i, j);
            i = ls.insert(i, r.begin(), r.end());
            expand(ls, i, finish, macros);
            if (initial) {
                i0 = i;
            }
            return i0;
        } else {
            _ nhs = hs;
            nhs.insert((*i).val);
            t_pp_seq r;
            _& mr = macro.replacement;
            substitute(mr.begin(), mr.end(), {}, {}, nhs, r, macros);
            i = ls.erase(i);
            i = ls.insert(i, r.begin(), r.end());
            expand(ls, i, finish, macros);
            if (initial) {
                i0 = i;
            }
            return i0;
        }
    }

    _ eval_condition(t_pp_seq& ls, t_pp_iter& pos, t_pp_iter line_end,
                     const _& macros) {
        if (pos == line_end) {
            return true;
        }
        pos = expand_defined(ls, pos, line_end, macros);
        pos = expand(ls, pos, line_end, macros);
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
        return val.u_val() != 0;
    }
}

class t_preprocessor {
    t_pp_seq& lex_seq;
    t_pp_iter pos;
    t_macros macros;
    t_file_manager& file_manager;

    void skip(bool ws = true) {
        pos = lex_seq.erase(pos);
        if (ws and (*pos).kind == "whitespace") {
            pos = lex_seq.erase(pos);
        }
    }
    void skip_newline() {
        expect("newline", pos);
        pos = lex_seq.erase(pos);
    }
    void skip_until_next_line() {
        while ((*pos).kind != "newline") {
            skip();
        }
        pos = lex_seq.erase(pos);
    }

    _ command(const str& name) {
        _ it = pos;
        if (not pp_hash(it)) {
            return false;
        }
        if (not ((*it).kind == "identifier" and (*it).val == name)) {
            return false;
        }
        step(it);
        pos = lex_seq.erase(pos, it);
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
        pos = expand(lex_seq, pos, it, macros);
        pos = it;
        return true;
    }

    _ define() {
        if (not command("define")) {
            return false;
        }
        expect("identifier", pos);
        _ id = (*pos).val;
        constrain(id != "defined",
                  "'defined' cannot be used as a a macro name", (*pos).loc);
        skip(false);
        _ is_func_like = false;
        std::unordered_map<str, size_t> params;
        if ((*pos).kind == "(") {
            is_func_like = true;
            skip();
            size_t i = 0;
            while ((*pos).kind != "newline" and (*pos).kind != ")") {
                if (not params.empty()) {
                    expect(",", pos);
                    skip();
                }
                expect("identifier", pos);
                params[(*pos).val] = i;
                skip();
                i++;
            }
            if ((*pos).kind == "newline") {
                expect(")", pos);
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
        macros.put(id, is_func_like, params, replace_list);
        return true;
    }

    _ undef() {
        if (not command("undef")) {
            return false;
        }
        expect("identifier", pos);
        macros.erase(*pos);
        skip();
        skip_newline();
        return true;
    }

    _ include_search(const vec<str>& dirs, const str& rel_path) {
        for (_& dir : dirs) {
            _ abs_path = dir + "/" + rel_path;
            // cout << "search " << abs_path << "\n";
            try {
                return file_manager.read_file(abs_path, rel_path);
            } catch (const std::ifstream::failure&) {
            }
        }
        return size_t(-1);
    }

    _ include() {
        if (not command("include")) {
            return false;
        }
        _ arg_loc = (*pos).loc;
        _ end = find_newline(pos);
        constrain((*pos).kind != "newline",
                  "expected <filename> or \"filename\"", arg_loc);
        if ((*next(end, -1)).kind == "whitespace") {
            end--;
        }
        pos = expand(lex_seq, pos, end, macros);
        str val;
        for (_ it = pos; it != end; it++) {
            val += (*it).val;
        }
        constrain(((val[0] == '<' and val.back() == '>')
                   or (val[0] == '"' and val.back() == '"')),
                  "expected <filename> or \"filename\"", arg_loc);
        const _ rel_path = unwrap(val);
        _ file_idx = size_t(-1);
        if (val[0] == '"') {
            _ cur_path = file_manager.get_abs_path(arg_loc.file_idx());
            file_idx = include_search({get_file_dir(cur_path)}, rel_path);
        }
        if (file_idx == size_t(-1)) {
            _ dirs = vec<str>{
                "/usr/local/include",
                get_abs_path(".") + "/include",
                "/usr/include/x86_64-linux-gnu",
                "/include",
                "/usr/include",
            };
            file_idx = include_search(dirs, rel_path);
            constrain(file_idx != size_t(-1),
                      "could not open " + rel_path, arg_loc);
            // cout << "incl " << file_manager.get_abs_path(file_idx) << "\n";
        }
        skip_until_next_line();
        _ pp_ls = lex(file_idx, file_manager);
        assert(not pp_ls.empty() and pp_ls.back().kind == "eof");
        pp_ls.pop_back();
        if (not pp_ls.empty()) {
            _ inc_start = pp_ls.begin();
            lex_seq.splice(pos, pp_ls);
            pos = inc_start;
        }
        return true;
    }

    _ empty_directive() {
        _ it = pos;
        if (not pp_hash(it)) {
            return false;
        }
        if ((*it).val != "\n") {
            return false;
        }
        pos = lex_seq.erase(pos, it);
        skip_newline();
        return true;
    }

    _ pragma() {
        if (not command("pragma")) {
            return false;
        }
        skip_until_next_line();
        return true;
    }

    _ error() {
        if (not command("error")) {
            return false;
        }
        _ loc = (*pos).loc;
        str msg;
        while ((*pos).kind != "newline") {
            if (msg != "") {
                msg += " ";
            }
            msg += (*pos).val;
            skip();
        }
        throw t_compile_error(msg, loc);
        return true;
    }

    _ if_block(bool cond_is_true) {
        if (cond_is_true) {
            group();
        } else {
            _ block_begin = pos;
            _ level = 0;
            while (not is_eof(pos)) {
                _ i = pos;
                if (pp_hash(i)) {
                    _ cmd_ = (*i).val;
                    if (cmd_ == "if" or cmd_ == "ifdef" or cmd_ == "ifndef") {
                        level++;
                    } else if (cmd_ == "endif" or cmd_ == "elif"
                               or cmd_ == "else") {
                        if (level == 0) {
                            break;
                        } else if (cmd_ == "endif") {
                            level--;
                        }
                    }
                }
                pos = find_newline(pos);
                pos++;
            }
            lex_seq.erase(block_begin, pos);
        }
    }

    _ if_aux(bool& found_true, t_pp_iter line_end) {
        _ cond_is_true = (not found_true
                          and eval_condition(lex_seq, pos, line_end, macros));
        pos = lex_seq.erase(pos, line_end);
        skip(false);
        if_block(cond_is_true);
        if (cond_is_true) {
            found_true = true;
        }
    }

    _ if_group(bool& found_true) {
        if (command("if")) {
        } else if (command("ifdef")) {
            pos = lex_seq.insert(pos, {"identifier", "defined"});
        } else if (command("ifndef")) {
            pos = lex_seq.insert(pos, {"identifier", "defined"});
            pos = lex_seq.insert(pos, {"!", "!"});
        } else {
            return false;
        }
        _ line_end = find_newline(pos);
        if_aux(found_true, line_end);
        return true;
    }

    _ elif_group(bool& found_true) {
        if (not command("elif")) {
            return false;
        }
        _ line_end = find_newline(pos);
        if_aux(found_true, line_end);
        return true;
    }

    _ else_group(bool& found_true) {
        if (not command("else")) {
            return false;
        }
        if_aux(found_true, pos);
        return true;
    }

    _ endif_line() {
        if (not command("endif")) {
            return false;
        }
        skip_newline();
        return true;
    }

    _ if_section() {
        _ if_loc = (*pos).loc;
        _ found_true = false;
        if (not if_group(found_true)) {
            return false;
        }
        while (elif_group(found_true)) {
        }
        else_group(found_true);
        constrain(endif_line(), "unterminated #if", if_loc);
        return true;
    }

    _ control_line() {
        return (include() or define() or undef() or error() or pragma()
                or empty_directive());
    }

    _ group_part() {
        return if_section() or control_line() or simple_lines();
    }

    void group() {
        while (group_part()) {
        }
    }

    void kill_consecutive_blank_lines() {
        _ it = lex_seq.begin();
        _ last_line_empty = false;
        while ((*it).kind != "eof") {
            if (last_line_empty and (*it).kind == "newline") {
                it = lex_seq.erase(it);
            } else {
                if ((*it).kind == "newline") {
                    last_line_empty = true;
                } else {
                    while ((*it).kind != "newline") {
                        it++;
                    }
                    last_line_empty = false;
                }
                it++;
            }
        }
    }
public:
    t_preprocessor(t_pp_seq& ls, t_file_manager& file_manager_)
        : lex_seq(ls)
        , pos(ls.begin())
        , macros(file_manager_)
        , file_manager(file_manager_) {
    }

    void scan() {
        group();
        expect("eof", pos);
        kill_consecutive_blank_lines();
    }
};

void preprocess(t_pp_seq& ls, t_file_manager& fm) {
    _ pp = t_preprocessor(ls, fm);
    pp.scan();
}
