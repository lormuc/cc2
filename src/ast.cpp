#include <vector>
#include <string>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <cassert>

#include "misc.hpp"
#include "ast.hpp"
#include "type.hpp"
#include "lex.hpp"

const auto debug = true;

using namespace std;

extern const vector<string> type_specifiers = {
    "void", "char", "short", "int", "long", "float", "double",
    "signed", "unsigned"
};

namespace {
    vector<t_lexeme> lexeme_list;
    size_t idx;

    t_ast exp();
    t_ast assign_exp();
    t_ast cast_exp();
    t_ast block_item();
    t_ast statement();
    t_ast declarator();
    t_ast struct_declaration_list();
    t_ast specifier_qualifier_list();
    t_ast identifier();
    t_ast abstract_declarator();
    t_ast declaration_specifiers();
    t_ast array_size();
    t_ast pointer();
    t_ast initializer();
    t_ast un_exp();
    t_ast cond_exp();
    t_ast type_name();

    auto init(vector<t_lexeme>& ll) {
        lexeme_list = ll;
        idx = 0;
    }

    auto& peek() {
        assert(idx < lexeme_list.size());
        return lexeme_list[idx];
    }

    auto get_state() {
        return idx;
    }

    auto set_state(auto _idx) {
        idx = _idx;
    }

    auto advance() {
        idx++;
    }

    auto add_child_opt(auto& res, auto f) {
        try {
            res.add_child(f());
        } catch (t_parse_error) {
        }
    }

    auto apply_rule(auto rule, const string& rule_name) {
        if (debug) {
            auto loc = peek().loc;
            cout << loc.line << ":" << loc.column << " " << rule_name << "\n";
        }
        auto old_state = get_state();
        try {
            return rule();
        } catch (const t_parse_error& e) {
            set_state(old_state);
            if (e.get_loc() == peek().loc) {
                throw t_parse_error("expected " + rule_name, peek().loc);
            } else {
                throw;
            }
        }
    }

#define def_rule(n) t_ast n () { return apply_rule(n##_, #n); }

    t_ast or_aux(t_parse_error max_err) {
        throw max_err;
    }

    template<typename tx, typename ... targs>
    t_ast or_aux(t_parse_error max_err, tx x, targs ... args) {
        try {
            return x();
        } catch (t_parse_error err) {
            if (err.get_loc() > max_err.get_loc()) {
                max_err = err;
            }
            return or_aux(max_err, args ...);
        }
    }

    template<typename ... targs>
    t_ast or_(targs ... args) {
        return or_aux(t_parse_error("", peek().loc), args ...);
    }

    auto opt(auto f) {
        auto res = t_ast("opt", peek().loc);
        try {
            res.add_child(f());
        } catch (t_parse_error) {
        }
        return res;
    }

    auto cmp(const string& name) {
        return peek().uu == name;
    }

    auto end() {
        return idx >= lexeme_list.size() or cmp("eof");
    }

    auto pop(const string& name) {
        if (not cmp(name)) {
            throw t_parse_error("expected " + name, peek().loc);
        }
        auto res = peek().vv;
        advance();
        return res;
    }

    auto prim_exp_() {
        auto kind = peek().uu;
        auto value = peek().vv;
        auto loc = peek().loc;
        t_ast res;
        if (kind == "identifier") {
            res = t_ast(kind, value, loc);
            advance();
        } else if (kind == "integer_constant") {
            res = t_ast(kind, value, loc);
            advance();
        } else if (kind == "floating_constant") {
            res = t_ast(kind, value, loc);
            advance();
        } else if (kind == "(") {
            advance();
            res = exp();
            pop(")");
        } else if (kind == "string_literal") {
            res = t_ast(kind, value, loc);
            advance();
        } else {
            throw t_parse_error(loc);
        }
        return res;
    }
    def_rule(prim_exp);

    auto postfix_exp_() {
        auto res = prim_exp();
        while (true) {
            auto loc = peek().loc;
            if (cmp("(")) {
                advance();
                res = t_ast("function_call", {res});
                if (cmp(")")) {
                    advance();
                } else {
                    while (true) {
                        res.add_child(assign_exp());
                        if (not cmp(",")) {
                            break;
                        }
                        advance();
                    }
                    pop(")");
                }
            } else if (cmp("[")) {
                advance();
                res = t_ast("array_subscript", {res, exp()});
                pop("]");
            } else if (cmp(".")) {
                advance();
                res = t_ast("struct_member", {res, identifier()});
            } else if (cmp("++")) {
                advance();
                res = t_ast("postfix_increment", {res});
            } else if (cmp("--")) {
                advance();
                res = t_ast("postfix_decrement", {res});
            } else {
                break;
            }
            res.loc = loc;
        }
        return res;
    }
    def_rule(postfix_exp);

    auto sizeof_exp_() {
        auto res = t_ast("sizeof_exp", peek().loc);
        pop("sizeof");
        res.add_child(un_exp());
        return res;
    }
    def_rule(sizeof_exp);

    auto sizeof_type_() {
        auto res = t_ast("sizeof_type", peek().loc);
        pop("sizeof");
        pop("(");
        res.add_child(type_name());
        pop(")");
        return res;
    }
    def_rule(sizeof_type);

    auto un_exp_() {
        t_ast res;
        vector<string> un_ops = {"&", "*", "+", "-", "~", "!"};
        if (cmp("++")) {
            res = t_ast("prefix_increment", peek().loc);
            advance();
            res.add_child(un_exp());
        } else if (cmp("--")) {
            res = t_ast("prefix_decrement", peek().loc);
            advance();
            res.add_child(un_exp());
        } else if (has(un_ops, peek().uu)) {
            res = t_ast(peek().uu, peek().loc);
            advance();
            res.add_child(cast_exp());
        } else if (cmp("sizeof")) {
            res = or_(sizeof_type, sizeof_exp);
        } else {
            res = postfix_exp();
        }
        return res;
    }
    def_rule(un_exp);

    auto parameter_declaration_() {
        auto res = t_ast("parameter_declaration", peek().loc);
        res.add_child(declaration_specifiers());
        try {
            res.add_child(declarator());
        } catch (t_parse_error) {
            try {
                res.add_child(abstract_declarator());
            } catch (t_parse_error) {
            }
        }
        return res;
    }
    def_rule(parameter_declaration);

    auto parameter_type_list_() {
        auto res = t_ast("parameter_type_list", peek().loc);
        res.add_child(parameter_declaration());
        while (true) {
            if (cmp(",")) {
                advance();
            } else {
                break;
            }
            try {
                res.add_child(parameter_declaration());
            } catch (t_parse_error) {
                if (cmp("...")) {
                    advance();
                    res.add_child(t_ast("...", peek().loc));
                    break;
                } else {
                    throw;
                }
            }
        }
        return res;
    }
    def_rule(parameter_type_list);

    auto function_params_opt_() {
        auto res = t_ast("function_params_opt", peek().loc);
        pop("(");
        add_child_opt(res, parameter_type_list);
        pop(")");
        return res;
    }
    def_rule(function_params_opt);

    auto direct_abstract_declarator_() {
        auto res = t_ast("direct_abstract_declarator", peek().loc);
        try {
            pop("(");
            res.add_child(abstract_declarator());
            pop(")");
        } catch (t_parse_error) {
        }
        while (true) {
            try {
                res.add_child(array_size());
            } catch (t_parse_error) {
                try {
                    res.add_child(function_params_opt());
                } catch (t_parse_error) {
                    break;
                }
            }
        }
        if (res.children.empty()) {
            throw t_parse_error(res.loc);
        }
        return res;
    }
    def_rule(direct_abstract_declarator);

    auto abstract_declarator_() {
        auto res = t_ast("abstract_declarator", peek().loc);
        res.add_child(opt(pointer));
        res.add_child(opt(direct_abstract_declarator));
        if (peek().loc == res.loc) {
            throw t_parse_error(res.loc);
        }
        return res;
    }
    def_rule(abstract_declarator);

    auto type_name_() {
        auto res = t_ast("type_name", peek().loc);
        res.add_child(specifier_qualifier_list());
        try {
            res.add_child(abstract_declarator());
        } catch (t_parse_error) {
        }
        return res;
    }
    def_rule(type_name);

    auto cast_() {
        auto res = t_ast("cast", peek().loc);
        pop("(");
        res.add_child(type_name());
        pop(")");
        res.add_child(cast_exp());
        return res;
    }
    def_rule(cast);

    auto cast_exp_() { return or_(un_exp, cast); }
    def_rule(cast_exp);

    auto left_assoc_bin_op(const vector<string>& ops,
                           function<t_ast()> subexp) {
        auto res = subexp();
        while (true) {
            auto op = peek().uu;
            if (not has(ops, op)) {
                break;
            }
            auto loc = peek().loc;
            advance();
            auto t = subexp();
            res = t_ast(op, {res, t});
            res.loc = loc;
        }
        return res;
    }

    auto mul_exp_() {
        return left_assoc_bin_op({"*", "/", "%"}, cast_exp);
    }
    def_rule(mul_exp);

    auto add_exp_() {
        return left_assoc_bin_op({"+", "-"}, mul_exp);
    }
    def_rule(add_exp);

    auto shift_exp_() {
        return left_assoc_bin_op({"<<", ">>"}, add_exp);
    }
    def_rule(shift_exp);

    auto rel_exp_() {
        return left_assoc_bin_op({"<", "<=", ">", ">="}, shift_exp);
    }
    def_rule(rel_exp);;

    auto eql_exp_() {
        return left_assoc_bin_op({"==", "!="}, rel_exp);
    }
    def_rule(eql_exp);

    auto bit_and_exp_() {
        return left_assoc_bin_op({"&"}, eql_exp);
    }
    def_rule(bit_and_exp);

    auto bit_xor_exp_() {
        return left_assoc_bin_op({"^"}, bit_and_exp);
    }
    def_rule(bit_xor_exp);

    auto bit_or_exp_() {
        return left_assoc_bin_op({"|"}, bit_xor_exp);
    }
    def_rule(bit_or_exp);

    auto and_exp_() {
        return left_assoc_bin_op({"&&"}, bit_or_exp);
    }
    def_rule(and_exp);

    auto or_exp_() {
        return left_assoc_bin_op({"||"}, and_exp);
    }
    def_rule(or_exp);

    auto cond_exp_() {
        auto res = or_exp();
        if (peek().uu == "?") {
            auto loc = peek().loc;
            advance();
            auto y = exp();
            pop(":");
            auto z = cond_exp();
            res = t_ast("?:", {res, y, z});
            res.loc = loc;
        }
        return res;
    }
    def_rule(cond_exp);

    auto assign_() {
        auto x = un_exp();
        vector<string> ops = {
            "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=",
            "^=", "|="
        };
        auto op = peek().uu;
        if (not has(ops, op)) {
            throw t_parse_error("expected assignment operator",
                                peek().loc);
        }
        auto res = t_ast(op, peek().loc);
        advance();
        res.add_child(x);
        res.add_child(assign_exp());
        return res;
    }
    def_rule(assign);

    auto assign_exp_() { return or_(assign, cond_exp); }
    def_rule(assign_exp);

    auto exp_() {
        return left_assoc_bin_op({","}, assign_exp);
    }
    def_rule(exp);

    t_ast const_exp() {
        return cond_exp();
    }

    auto exp_statement_() {
        auto res = t_ast("exp_statement", peek().loc);
        add_child_opt(res, exp);
        pop(";");
        return res;
    }
    def_rule(exp_statement);

    auto pointer_() {
        auto res = t_ast("pointer", peek().loc);
        pop("*");
        add_child_opt(res, pointer);
        return res;
    }
    def_rule(pointer);

    auto struct_declarator_list_() {
        auto res = t_ast("struct_declarator_list", peek().loc);
        while (true) {
            res.add_child(declarator());
            if (not cmp(",")) {
                break;
            }
            advance();
        }
        return res;
    }
    def_rule(struct_declarator_list);

    auto struct_or_union_specifier_() {
        auto res = t_ast("struct_or_union_specifier", peek().loc);
        pop("struct");
        if (cmp("identifier")) {
            res.vv = peek().vv;
            advance();
            if (cmp("{")) {
                advance();
                res.add_child(struct_declaration_list());
                pop("}");
            }
        } else {
            pop("{");
            res.add_child(struct_declaration_list());
            pop("}");
        }
        return res;
    }
    def_rule(struct_or_union_specifier);

    auto type_specifier_() {
        if (has(type_specifiers, peek().uu)) {
            auto res = t_ast("type_specifier", peek().uu, peek().loc);
            advance();
            return res;
        } else {
            return struct_or_union_specifier();
        }
    }
    def_rule(type_specifier);

    auto specifier_qualifier_list_() {
        auto res = t_ast("specifier_qualifier_list", peek().loc);
        res.add_child(type_specifier());
        while (true) {
            try {
                res.add_child(type_specifier());
            } catch (t_parse_error) {
                break;
            }
        }
        return res;
    }
    def_rule(specifier_qualifier_list);

    auto struct_declaration_() {
        auto res = t_ast("struct_declaration", peek().loc);
        res.add_child(specifier_qualifier_list());
        res.add_child(struct_declarator_list());
        pop(";");
        return res;
    }
    def_rule(struct_declaration);

    auto struct_declaration_list_() {
        auto res = t_ast("struct_declaration_list", peek().loc);
        while (true) {
            try {
                res.add_child(struct_declaration());
            } catch (t_parse_error) {
                break;
            }
        }
        return res;
    }
    def_rule(struct_declaration_list);

    auto identifier_() {
        auto res = t_ast("identifier", peek().loc);
        res.vv = pop("identifier");
        return res;
    }
    def_rule(identifier);

    auto array_size_() {
        auto res = t_ast("array_size", peek().loc);
        pop("[");
        add_child_opt(res, const_exp);
        pop("]");
        return res;
    }
    def_rule(array_size);

    auto func_ids_() {
        auto res = t_ast("func_ids", peek().loc);
        pop("(");
        if (not cmp(")")) {
            while (true) {
                res.add_child(identifier());
                if (not cmp(",")) {
                    break;
                }
                advance();
            }
        }
        pop(")");
        return res;
    }
    def_rule(func_ids);

    auto subdeclarator_() {
        pop("(");
        auto e = declarator();
        pop(")");
        return e;
    }
    def_rule(subdeclarator);

    auto direct_declarator_() {
        auto res = t_ast("direct_declarator", peek().loc);
        res.add_child(or_(subdeclarator,
                          identifier));
        while (true) {
            try {
                res.add_child(or_(function_params_opt,
                                  func_ids,
                                  array_size));
            } catch (t_parse_error) {
                break;
            }
        }
        return res;
    }
    def_rule(direct_declarator);

    t_ast declarator_() {
        auto res = t_ast("declarator", peek().loc);
        res.add_child(opt(pointer));
        res.add_child(direct_declarator());
        return res;
    }
    def_rule(declarator);

    auto initializer_() {
        auto res = t_ast("initializer", peek().loc);
        try {
            res.add_child(assign_exp());
        } catch (t_parse_error) {
            pop("{");
            while (true) {
                res.add_child(initializer());
                if (not cmp(",")) {
                    break;
                }
                advance();
                if (cmp("}")) {
                    break;
                }
            }
            pop("}");
        }
        return res;
    }
    def_rule(initializer);

    auto init_declarator_() {
        auto res = t_ast("init_declarator", peek().loc);
        res.add_child(declarator());
        if (cmp("=")) {
            advance();
            res.add_child(initializer());
        }
        return res;
    }
    def_rule(init_declarator);

    auto declaration_specifiers_() {
        return specifier_qualifier_list();
    }
    def_rule(declaration_specifiers);

    auto declaration_() {
        auto res = t_ast("declaration", peek().loc);
        res.add_child(declaration_specifiers());
        if (not cmp(";")) {
            while (true) {
                res.add_child(init_declarator());
                if (not cmp(",")) {
                    break;
                }
                advance();
            }
        }
        pop(";");
        return res;
    }
    def_rule(declaration);

    auto compound_statement_() {
        auto res = t_ast("compound_statement", peek().loc);
        pop("{");
        while (not cmp("}")) {
            res.add_child(block_item());
        }
        advance();
        return res;
    }
    def_rule(compound_statement);

    auto if_statement_() {
        auto res = t_ast("if", peek().loc);
        pop("if");
        pop("(");
        res.add_child(exp());
        pop(")");
        res.add_child(statement());
        if (cmp("else")) {
            advance();
            res.add_child(statement());
        }
        return res;
    }
    def_rule(if_statement);

    auto label_() {
        auto id = pop("identifier");
        auto res = t_ast("label", id, peek().loc);
        pop(":");
        res.add_child(statement());
        return res;
    }
    def_rule(label);

    auto while_statement_() {
        auto res = t_ast("while", peek().loc);
        pop("while");
        pop("(");
        res.add_child(exp());
        pop(")");
        res.add_child(statement());
        return res;
    }
    def_rule(while_statement);

    auto do_while_statement_() {
        auto res = t_ast("do_while", peek().loc);
        pop("do");
        res.add_child(statement());
        pop("while");
        pop("(");
        res.add_child(exp());
        pop(")");
        pop(";");
        return res;
    }
    def_rule(do_while_statement);

    auto for_statement_() {
        auto res = t_ast("for", peek().loc);
        pop("for");
        pop("(");
        res.add_child(opt(exp));
        pop(";");
        res.add_child(opt(exp));
        pop(";");
        res.add_child(opt(exp));
        pop(")");
        res.add_child(statement());
        return res;
    }
    def_rule(for_statement);

    auto goto_statement_() {
        auto res = t_ast("goto", peek().loc);
        pop("goto");
        res.add_child(identifier());
        pop(";");
        return res;
    }
    def_rule(goto_statement);

    auto continue_statement_() {
        auto res = t_ast("continue", peek().loc);
        pop("continue");
        pop(";");
        return res;
    }
    def_rule(continue_statement);

    auto break_statement_() {
        auto res = t_ast("break", peek().loc);
        pop("break");
        pop(";");
        return res;
    }
    def_rule(break_statement);

    auto return_statement_() {
        auto res = t_ast("return", peek().loc);
        pop("return");
        res.add_child(exp());
        pop(";");
        return res;
    }
    def_rule(return_statement);

    auto labeled_statement() {
        return t_ast();
    }

    auto statement_() {
        return or_(label,
                   compound_statement,
                   if_statement,
                   while_statement,
                   do_while_statement,
                   for_statement,
                   goto_statement,
                   continue_statement,
                   break_statement,
                   return_statement,
                   exp_statement);
    }
    def_rule(statement);

    auto block_item_() { return or_(statement, declaration); }
    def_rule(block_item);

    auto function_definition_() {
        auto loc = peek().loc;
        pop("int");
        auto func_name = pop("identifier");
        pop("(");
        pop(")");
        auto children = compound_statement().children;
        auto res = t_ast("function", func_name, children);
        res.loc = loc;
        return res;
    }
    def_rule(function_definition);
}

t_ast parse_program(vector<t_lexeme>& n_ll) {
    init(n_ll);
    auto res = t_ast("program", peek().loc);
    while (not end()) {
        res.add_child(function_definition());
    }
    return res;
}
