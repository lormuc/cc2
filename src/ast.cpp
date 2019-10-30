#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <cassert>
#include <list>
#include <stack>

#include "misc.hpp"
#include "ast.hpp"
#include "lex.hpp"
#include "gen.hpp"

extern const vec<str> simple_type_specifiers = {
    "void", "char", "short", "int", "long", "float", "double",
    "signed", "unsigned"
};

using t_rule = bool(bool);
using t_dyn_rule = std::function<bool(bool)>;

class t_ast_ctx {
    vec<std::unordered_map<str, bool>> typedef_names;
    std::list<t_lexeme>::const_iterator pos;
    std::stack<str> rule_names;
    std::stack<t_ast*> node_ptrs;
    t_ast result;
    t_ast* m_cur_node = nullptr;
public:
    t_ast_ctx() {
    }
    void init(std::list<t_lexeme>::const_iterator start) {
        typedef_names.clear();
        typedef_names.push_back({});
        pos = start;
        rule_names = std::stack<str>();
        node_ptrs = std::stack<t_ast*>();
        result = t_ast();
        m_cur_node = nullptr;
    }
    void enter_scope() {
        typedef_names.push_back({});
    }
    void leave_scope() {
        typedef_names.pop_back();
    }
    void put(const str& id, bool x) {
        typedef_names.back()[id] = x;
    }
    bool is_typedef_name(const str& id) {
        for (_ i = typedef_names.size(); i > 0; i--) {
            _ x = typedef_names[i-1].find(id);
            if (x != typedef_names[i-1].end()) {
                return (*x).second;
            }
        }
        return false;
    }
    const t_lexeme& peek() {
        return *pos;
    }
    void advance(int n = 1) {
        if ((*pos).uu != "eof") {
            std::advance(pos, n);
        }
    }
    str cur_rule() const {
        if (rule_names.empty()) {
            return "";
        }
        return rule_names.top();
    }
    void add_leaf(const str& kind, const str& val) {
        assert(m_cur_node != nullptr);
        (*m_cur_node).add_child(t_ast(kind, val, peek().loc));
    }
    void enter_rule(const str& name) {
        // cout << "enter " << name << "\n";
        rule_names.push(name);
    }
    void leave_rule() {
        rule_names.pop();
    }
    void create_node() {
        // cout << "create " << cur_rule() << "\n";
        node_ptrs.push(m_cur_node);
        if (m_cur_node == nullptr) {
            result = t_ast(cur_rule(), peek().loc);
            m_cur_node = &result;
        } else {
            (*m_cur_node).children.push_back(t_ast(cur_rule(), peek().loc));
            m_cur_node = &((*m_cur_node).children.back());
        }
    }
    void replace_node() {
        // cout << "create " << cur_rule() << "\n";
        assert(m_cur_node != nullptr);
        assert(not (*m_cur_node).children.empty());
        _& last = (*m_cur_node).children.back();
        _ x = t_ast(cur_rule(), peek().loc);
        x.add_child(last);
        last = x;
        node_ptrs.push(m_cur_node);
        m_cur_node = &last;
    }
    void leave_node() {
        // cout << "leave  " << cur_rule() << "\n";
        assert(not node_ptrs.empty());
        m_cur_node = node_ptrs.top();
        node_ptrs.pop();
    }
    const t_ast& last_child() const {
        assert(not (*m_cur_node).children.empty());
        return (*m_cur_node).children.back();
    }
    t_ast get_result() {
        return std::move(result);
    }
};

namespace {
    t_ast_ctx ctx;

    _& peek() { return ctx.peek(); }
    _ advance(int n = 1) { ctx.advance(n); }
    _ cmp(const str& name) { return peek().uu == name; }

    bool syms_0(const char* sym) {
        if (cmp(sym)) {
            advance();
            return true;
        }
        return false;
    }

    bool syms_0(t_rule sym) {
        return sym(false);
    }

    bool syms_0(t_dyn_rule sym) {
        return sym(false);
    }

    bool apply_sym(bool only_check, const char* sym) {
        if (only_check) {
            return cmp(sym);
        } else {
            return syms_0(sym);
        }
    }

    bool apply_sym(bool only_check, t_rule sym) {
        return sym(only_check);
    }

    bool apply_sym(bool only_check, t_dyn_rule sym) {
        return sym(only_check);
    }

    bool check(_ sym) {
        return apply_sym(true, sym);
    }

    template<bool aux = false>
    void syms_() {
    }

    template<typename t_sym, typename ... t_syms>
    void syms_(t_sym s, t_syms ... ss) {
        if (not syms_0(s)) {
            _ msg = ctx.cur_rule();
            if (not msg.empty()) {
                msg += ": ";
            }
            msg += "unexpected symbol";
            throw t_parse_error(msg, peek().loc);
        }
        syms_(ss ...);
    }

    template<typename t_sym, typename ... t_syms>
    bool syms(t_sym s, t_syms ... ss) {
        if (syms_0(s)) {
            syms_(ss ...);
            return true;
        }
        return false;
    }

    template<typename t_sym, typename ... t_syms>
    bool apply_rule(bool only_check, const str& name, t_sym s, t_syms ... ss) {
        ctx.enter_rule(name);
        _ res = false;
        if (not check(s)) {
            res = false;
        } else if (only_check) {
            res = true;
        } else {
            ctx.create_node();
            syms_(s, ss ...);
            ctx.leave_node();
            res = true;
        }
        ctx.leave_rule();
        return res;
    }

    template<typename t_sym, typename ... t_syms>
    bool apply_aux_rule(bool only_check, t_sym s, t_syms ... ss) {
        if (not check(s)) {
            return false;
        }
        if (only_check) {
            return true;
        }
        syms_(s, ss ...);
        return true;
    }

    template<typename t_sym, typename ... t_syms>
    bool apply_l_rule(bool only_check, const str& name,
                      t_sym s, t_syms ... ss) {
        ctx.enter_rule(name);
        _ res = false;
        if (not check(s)) {
            res = false;
        } else if (only_check) {
            res = true;
        } else {
            ctx.replace_node();
            syms_(s, ss ...);
            ctx.leave_node();
            res = true;
        }
        ctx.leave_rule();
        return res;
    }

#define def_l(n, ...) bool n(bool c){return apply_l_rule(c,#n,__VA_ARGS__);}
#define def(n, ...) bool n(bool c){return apply_rule(c,#n,__VA_ARGS__);}
#define def_aux(n, ...) bool n(bool c){return apply_aux_rule(c,__VA_ARGS__);}

    template<typename ... t_syms>
    t_dyn_rule opt(t_syms ... ss) {
        _ ff = [=](bool only_check) {
            if (only_check) {
                return true;
            }
            syms(ss ...);
            return true;
        };
        return t_dyn_rule(ff);
    }

    bool apply_bar(bool) {
        return false;
    }

    template<typename t_sym, typename ... t_syms>
    bool apply_bar(bool only_check, t_sym sym, t_syms ... syms) {
        if (not apply_sym(only_check, sym)) {
            return apply_bar(only_check, syms ...);
        }
        return true;
    }

    template<typename ... t_syms>
    t_dyn_rule bar(t_syms ... syms) {
        _ f = [=](bool only_check) {
            return apply_bar(only_check, syms ...);
        };
        return t_dyn_rule(f);
    }

    template<typename ... t_syms>
    t_dyn_rule __(t_syms ... syms) {
        _ ff = [=](bool only_check) {
            return apply_aux_rule(only_check, syms ...);
        };
        return t_dyn_rule(ff);
    }

    template<typename ... t_syms>
    t_dyn_rule comma_seq(t_syms ... ss) {
        _ ff = [=](bool only_check) {
            _ x = check(__(ss ...));
            if (only_check or not x) {
                return x;
            }
            syms_(ss ...);
            while (true) {
                if (not cmp(",")) {
                    break;
                }
                advance();
                if (not syms(ss ...)) {
                    advance(-1);
                    break;
                }
            }
            return true;
        };
        return t_dyn_rule(ff);
    }

    template<typename ... t_syms>
    t_dyn_rule seq(t_syms ... ss) {
        _ ff = [=](bool only_check) {
            _ x = check(__(ss ...));
            if (only_check or not x) {
                return x;
            }
            while (syms(ss ...)) {
            }
            return true;
        };
        return t_dyn_rule(ff);
    }

    t_rule subexp;
    t_rule assign_exp;
    t_rule stmt;
    t_rule decltor;
    t_rule type_name;
    t_rule un_exp;
    t_rule param_types;
    t_rule init_decltor;
    t_rule initzers;
    t_rule type_spec;
    t_rule cast_exp;
    t_rule exp;
    t_rule block_item;
    t_rule or_exp;
    t_rule enumtor;

    bool identifier(bool only_check) {
        if (only_check) {
            return cmp("identifier");
        }
        if (cmp("identifier")) {
            ctx.add_leaf("identifier", peek().vv);
            advance();
            return true;
        }
        return false;
    }

    bool empty_identifier(bool only_check) {
        if (only_check) {
            return true;
        }
        ctx.add_leaf("identifier", "");
        return true;
    }

    bool prim_exp_0(bool only_check) {
        _ kind = peek().uu;
        _ val = peek().vv;
        if ((kind == "identifier" and not ctx.is_typedef_name(val))
            or kind == "integer_constant" or kind == "floating_constant"
            or kind == "char_constant" or kind == "string_literal") {
            if (only_check) {
                return true;
            }
            ctx.add_leaf(kind, val);
            advance();
            return true;
        } else {
            return false;
        }
    }

    bool type_name_in_parens(bool only_check) {
        if (not cmp("(")) {
            return false;
        }
        advance();
        if (not check(type_name)) {
            advance(-1);
            return false;
        }
        advance(-1);
        if (only_check) {
            return true;
        }
        syms_("(", type_name, ")");
        return true;
    }

    _ left_assoc_op(const vec<str>& ops, _ e) {
        _ ff = [=](bool only_check) {
            _ x = check(e);
            if (not x) {
                return false;
            }
            if (only_check) {
                return true;
            }
            syms_(e);
            while (true) {
                _ op = peek().uu;
                if (not has(ops, op)) {
                    break;
                }
                advance();
                ctx.enter_rule(op);
                ctx.replace_node();
                syms_(e);
                ctx.leave_node();
                ctx.leave_rule();
            }
            return true;
        };
        return t_dyn_rule(ff);
    }

    bool simple_type_spec(bool only_check) {
        if (not has(simple_type_specifiers, peek().uu)) {
            return false;
        }
        if (only_check) {
            return true;
        }
        ctx.add_leaf("simple_type_spec", peek().uu);
        advance();
        return true;
    }

    bool typedef_name(bool only_check) {
        ctx.enter_rule(__func__);
        if (not (cmp("identifier") and ctx.is_typedef_name(peek().vv))) {
            ctx.leave_rule();
            return false;
        }
        if (only_check) {
            ctx.leave_rule();
            return true;
        }
        ctx.create_node();
        syms_(identifier);
        ctx.leave_node();
        ctx.leave_rule();
        return true;
    }

    bool storage_class_specifier(bool only_check) {
        if (not (cmp("static") or cmp("extern") or cmp("register")
                 or cmp("auto") or cmp("typedef"))) {
            return false;
        }
        if (only_check) {
            return true;
        }
        ctx.add_leaf("storage_class_specifier", peek().uu);
        advance();
        return true;
    }

    bool decl_specs(bool only_check) {
        ctx.enter_rule(__func__);
        if (not (check(bar(typedef_name,
                           storage_class_specifier,
                           type_spec)))) {
            ctx.leave_rule();
            return false;
        }
        if (only_check) {
            ctx.leave_rule();
            return true;
        }
        ctx.create_node();
        _ can_be_typedef_name = true;
        while (true) {
            if (can_be_typedef_name and syms(typedef_name)) {
                break;
            } else {
                if (syms(type_spec)) {
                    can_be_typedef_name = false;
                } else if (not syms(storage_class_specifier)) {
                    break;
                }
            }
        }
        ctx.leave_node();
        ctx.leave_rule();
        return true;
    }

    _ find_id(const _& ast) {
        if (ast.uu == "identifier") {
            return ast.vv;
        } else {
            return find_id(ast[0]);
        }
    }

    bool compound_stmt(bool only_check) {
        ctx.enter_scope();
        _ res = apply_rule(only_check, __func__,
                           "{", opt(seq(block_item)), "}");
        ctx.leave_scope();
        return res;
    }

    bool declaration(bool only_check) {
        ctx.enter_rule(__func__);
        _ x = check(decl_specs);
        if (only_check or not x) {
            ctx.leave_rule();
            return x;
        }
        ctx.create_node();
        syms_(decl_specs);
        _ is_typedef = (storage_class(ctx.last_child())
                        == t_storage_class::_typedef);
        if (not cmp(";")) {
            while (true) {
                syms_(init_decltor);
                ctx.put(find_id(ctx.last_child()), is_typedef);
                if (not syms(",")) {
                    break;
                }
            }
        }
        syms_(bar(";", compound_stmt));
        ctx.leave_node();
        ctx.leave_rule();
        return true;
    }

    bool label_stmt(bool only_check) {
        ctx.enter_rule(__func__);
        if (not cmp("identifier")) {
            ctx.leave_rule();
            return false;
        }
        advance();
        if (not cmp(":")) {
            advance(-1);
            ctx.leave_rule();
            return false;
        }
        advance(-1);
        if (only_check) {
            ctx.leave_rule();
            return true;
        }
        ctx.create_node();
        syms_(identifier, ":", stmt);
        ctx.leave_node();
        ctx.leave_rule();
        return true;
    }

    bool cast(bool only_check) {
        ctx.enter_rule(__func__);
        if (not check(type_name_in_parens)) {
            ctx.leave_rule();
            return false;
        }
        if (only_check) {
            ctx.leave_rule();
            return true;
        }
        ctx.create_node();
        syms_(type_name_in_parens, cast_exp);
        ctx.leave_node();
        ctx.leave_rule();
        return true;
    }

    bool cond_exp(bool only_check) {
        if (not check(or_exp)) {
            return false;
        }
        if (only_check) {
            return true;
        }
        syms_(or_exp);
        if (cmp("?")) {
            ctx.enter_rule("?:");
            ctx.replace_node();
            advance();
            syms_(subexp, ":", cond_exp);
            ctx.leave_node();
            ctx.leave_rule();
        }
        return true;
    }

    bool assign_exp(bool only_check) {
        if (not check(cond_exp)) {
            return false;
        }
        if (only_check) {
            return true;
        }
        const _ assign_ops = vec<str>{
            "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|="
        };
        syms_(cond_exp);
        _ n = 0;
        while (true) {
            _ op = peek().uu;
            if (not has(assign_ops, op)) {
                break;
            }
            ctx.enter_rule(op);
            ctx.replace_node();
            advance();
            syms_(cond_exp);
            n++;
        }
        while (n > 0) {
            ctx.leave_node();
            ctx.leave_rule();
            n--;
        }
        return true;

    }

    bool enumtor_put(bool only_check) {
        if (only_check) {
            return enumtor(true);
        }
        _ res = enumtor(false);
        ctx.put(ctx.last_child()[0].vv, false);
        return res;
    }

    def_aux(prim_exp,
            bar(prim_exp_0, __("(", subexp, ")")));

    def_aux(cast_exp, bar(cast, un_exp));
    def_aux(mul_exp, left_assoc_op({"*", "/", "%"}, cast_exp));
    def_aux(add_exp, left_assoc_op({"+", "-"},  mul_exp));
    def_aux(shift_exp, left_assoc_op({"<<", ">>"}, add_exp));
    def_aux(rel_exp, left_assoc_op({"<", ">", "<=", ">="}, shift_exp));
    def_aux(eql_exp, left_assoc_op({"==", "!="}, rel_exp));
    def_aux(bit_and_exp, left_assoc_op({"&"}, eql_exp));
    def_aux(bit_xor_exp, left_assoc_op({"^"}, bit_and_exp));
    def_aux(bit_or_exp, left_assoc_op({"|"}, bit_xor_exp));
    def_aux(and_exp, left_assoc_op({"&&"}, bit_or_exp));
    def_aux(or_exp, left_assoc_op({"||"}, and_exp));

    def_l(array_subscript,
          "[", subexp, "]");
    def_l(func_call,
          "(", opt(comma_seq(assign_exp)), ")");
    def_l(member,
          ".", identifier);
    def_l(arrow,
          "->", identifier);
    def_l(postfix_inc,
          "++");
    def_l(postfix_dec,
          "--");
    def_aux(postfix_exp,
            prim_exp, opt(seq(bar(array_subscript,
                                  func_call,
                                  member,
                                  arrow,
                                  postfix_inc,
                                  postfix_dec))));

    def(adr_op,
        "&", cast_exp);
    def(ind_op,
        "*", cast_exp);
    def(un_plus,
        "+", cast_exp);
    def(un_minus,
        "-", cast_exp);
    def(bit_not_op,
        "~", cast_exp);
    def(not_op,
        "!", cast_exp);
    def(prefix_inc,
        "++", un_exp);
    def(prefix_dec,
        "--", un_exp);
    def(sizeof_op,
        "sizeof", bar(type_name_in_parens,
                      un_exp));
    def_aux(un_exp,
            bar(postfix_exp,
                prefix_inc,
                prefix_dec,
                adr_op,
                ind_op,
                un_plus,
                un_minus,
                bit_not_op,
                not_op,
                sizeof_op));

    def_aux(subexp, left_assoc_op({","}, assign_exp));
    def(exp, subexp);
    def(const_exp, cond_exp);

    def_aux(opt_id, bar(identifier, empty_identifier));

    def_l(array_decltor,
          "[", opt(const_exp), "]");
    def_l(func_decltor,
          "(", opt(param_types), ")");
    def_aux(direct_decltor,
            bar(__("(", decltor, ")"),
                identifier,
                empty_identifier),
            opt(seq(bar(array_decltor,
                        func_decltor))));
    def(ptr_decltor,
        "*", decltor);
    def_aux(decltor,
            bar(ptr_decltor, direct_decltor));

    def(type_name, decl_specs, decltor);

    def(param_decl,
        decl_specs, decltor);
    def(ellipsis,
        "...");
    def(param_types,
        comma_seq(param_decl), opt(",", ellipsis));

    def(if_stmt,
        "if", "(", exp, ")", stmt, opt("else", stmt));
    def(opt_exp,
        opt(exp));
    def(for_stmt,
        "for", "(", opt_exp, ";", opt_exp, ";", opt_exp, ")", stmt);
    def(break_stmt,
        "break", ";");
    def(continue_stmt,
        "continue", ";");
    def(switch_stmt,
        "switch", "(", exp, ")", stmt);
    def(case_stmt,
        "case", const_exp, ":", stmt);
    def(goto_stmt,
        "goto", identifier, ";");
    def(default_stmt,
        "default", ":", stmt);
    def(do_while_stmt,
        "do", stmt, "while", "(", exp, ")", ";");
    def(while_stmt,
        "while", "(", exp, ")", stmt);
    def(return_stmt,
        "return", opt(exp), ";");
    def(exp_stmt,
        bar(";", __(exp, ";")));

    def(struct_decltors,
        comma_seq(decltor));
    def(struct_decl,
        decl_specs, struct_decltors, ";");
    def(struct_decls,
        seq(struct_decl));
    def(struct_spec,
        "struct", opt_id, opt("{", struct_decls, "}"));
    def(union_spec,
        "union", opt_id, opt("{", struct_decls, "}"));

    def(enumtor,
        identifier, opt("=", const_exp));

    def(enumtors,
        "{", comma_seq(enumtor_put), "}");
    def(enum_spec,
        "enum", opt_id, opt(enumtors));
    def_aux(type_spec,
            bar(simple_type_spec, struct_spec, union_spec, enum_spec));

    def_aux(initzer,
            bar(assign_exp, initzers));
    def(initzers,
        "{", comma_seq(initzer), opt(","), "}");
    def(init_decltor,
        decltor, opt("=", initzer));

    def_aux(block_item,
            bar(declaration,
                stmt));

    def_aux(stmt,
            bar(switch_stmt,
                case_stmt,
                default_stmt,
                label_stmt,
                compound_stmt,
                if_stmt,
                while_stmt,
                do_while_stmt,
                for_stmt,
                goto_stmt,
                continue_stmt,
                break_stmt,
                return_stmt,
                exp_stmt));
    def(program, opt(seq(declaration)), "eof");
}

t_ast parse_exp(std::list<t_lexeme>::const_iterator start) {
    ctx.init(start);
    syms_(const_exp, "eof");
    return ctx.get_result();
}

t_ast parse_program(std::list<t_lexeme>::const_iterator start) {
    ctx.init(start);
    syms_(program);
    return ctx.get_result();
}
