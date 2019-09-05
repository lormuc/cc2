#include <vector>
#include <string>
#include <stdexcept>
#include <functional>
#include <iostream>

#include "misc.hpp"
#include "ast.hpp"
#include "type.hpp"
#include "lex.hpp"

using namespace std;

namespace {
    const vector<string> type_specifiers = {
        "void", "char", "short", "int", "long", "float", "double",
        "signed", "unsigned"
    };

    vector<t_lexeme> lexeme_list;
    size_t idx;

    t_ast exp();
    t_ast assign_exp();
    t_ast cast_exp();
    t_ast block_item();
    t_ast pointer_opt();
    t_ast statement();
    t_ast declarator();
    t_ast struct_declaration_list();
    t_ast identifier();

    auto init(vector<t_lexeme>& ll) {
        lexeme_list = ll;
        idx = 0;
    }

    auto& peek() {
        if (idx < lexeme_list.size()) {
            return lexeme_list[idx];
        } else {
            throw logic_error("unexpected end of lexeme list");
        }
    }

    auto advance() {
        // cout << peek().uu << " -- " << peek().vv << "\n";
        idx++;
    }

    auto cmp(const string& name) {
        return peek().uu == name;
    }

    auto end() {
        return idx >= lexeme_list.size() or cmp("eof");
    }

    auto kw(const string& str) {
        return peek().uu == "keyword" and peek().vv == str;
    }

    auto pop(const string& name) {
        if (not cmp(name)) {
            err(string() + "expected <" + name + ">", peek().loc);
        }
        auto res = peek().vv;
        advance();
        return res;
    }

    auto pop_kw(const string& name) {
        if (not (peek().uu == "keyword" and peek().vv == name)) {
            err(string() + "expected <" + name + ">", peek().loc);
        }
        advance();
    }

    auto prim_exp() {
        auto kind = peek().uu;
        auto value = peek().vv;
        auto loc = peek().loc;
        t_ast res;
        if (kind == "identifier") {
            advance();
            res.uu = kind;
            res.vv = value;
        } else if (kind == "integer_constant") {
            advance();
            res.uu = kind;
            res.vv = value;
        } else if (kind == "floating_constant") {
            advance();
            res.uu = kind;
            res.vv = value;
        } else if (kind == "(") {
            advance();
            res = exp();
            pop(")");
        } else if (kind == "string_literal") {
            advance();
            res.uu = kind;
            res.vv = value;
        } else {
            err("expected primary expression", loc);
        }
        res.loc = loc;
        return res;
    }

    auto postfix_exp() {
        auto res = prim_exp();
        while (true) {
            auto loc = peek().loc;
            if (cmp("(")) {
                advance();
                res = t_ast("function_call", {res});
                if (not cmp(")")) {
                    while (true) {
                        res.add_child(assign_exp());
                        if (peek().uu != ",") {
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
            } else {
                break;
            }
            res.loc = loc;
        }
        return res;
    }

    auto un_exp() {
        auto loc = peek().loc;
        t_ast res;
        auto kind = peek().uu;
        vector<string> un_ops = {"&", "*", "+", "-", "~", "!"};
        if (has(un_ops, kind)) {
            advance();
            auto e = cast_exp();
            res.uu = "un_op";
            res.vv = kind;
            res.children = {e};
        } else {
            res = postfix_exp();
        }
        res.loc = loc;
        return res;
    }

    t_ast cast_exp() {
        return un_exp();
    }

    auto left_assoc_bin_op(const vector<string>& ops,
                           function<t_ast()> subexp) {
        auto loc = peek().loc;
        auto res = subexp();
        while (true) {
            auto op = peek().uu;
            if (not has(ops, op)) {
                break;
            }
            loc = peek().loc;
            advance();
            auto t = subexp();
            res = t_ast("bin_op", op, {res, t});
        }
        res.loc = loc;
        return res;
    }

    auto mul_exp() {
        return left_assoc_bin_op({"*", "/", "%"}, cast_exp);
    }

    auto add_exp() {
        return left_assoc_bin_op({"+", "-"}, mul_exp);
    }

    auto shift_exp() {
        return add_exp();
    }

    auto rel_exp() {
        return left_assoc_bin_op({"<", "<=", ">", ">="}, shift_exp);
    }

    auto eql_exp() {
        return left_assoc_bin_op({"==", "!="}, rel_exp);
    }

    auto bit_and_exp() {
        return left_assoc_bin_op({"&"}, eql_exp);
    }

    auto bit_xor_exp() {
        return left_assoc_bin_op({"^"}, bit_and_exp);
    }

    auto bit_or_exp() {
        return left_assoc_bin_op({"|"}, bit_xor_exp);
    }

    auto and_exp() {
        return left_assoc_bin_op({"&&"}, bit_or_exp);
    }

    auto or_exp() {
        return left_assoc_bin_op({"||"}, and_exp);
    }

    t_ast cond_exp() {
        auto res = or_exp();
        auto loc = peek().loc;
        if (peek().uu == "?") {
            advance();
            auto y = exp();
            pop(":");
            auto z = cond_exp();
            res = t_ast("tern_op", "?:", {res, y, z});
        }
        res.loc = loc;
        return res;
    }

    t_ast assign_exp() {
        auto loc = peek().loc;
        auto old_idx = idx;
        t_ast res = un_exp();
        if (not end()) {
            auto op = peek().uu;
            vector<string> ops = {"="};
            if (has(ops, op)) {
                advance();
                auto y = assign_exp();
                res = t_ast("bin_op", op, {res, y});
            } else {
                idx = old_idx;
                res = cond_exp();
            }
        }
        res.loc = loc;
        return res;
    }

    t_ast exp() {
        return left_assoc_bin_op({","}, assign_exp);
    }

    t_ast const_exp() {
        return cond_exp();
    }

    t_ast opt_exp(const string& exp_end) {
        auto loc = peek().loc;
        auto res = t_ast("opt_exp");
        if (cmp(exp_end)) {
            advance();
        } else {
            res.add_child(exp());
            pop(exp_end);
        }
        res.loc = loc;
        return res;
    }

    auto exp_statement() {
        auto loc = peek().loc;
        auto children = opt_exp(";").children;
        t_ast res;
        res.uu = "exp_statement";
        res.children = children;
        res.loc = loc;
        return res;
    }

    t_ast pointer() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "pointer";
        pop("*");
        if (cmp("*")) {
            res.add_child(pointer());
        }
        res.loc = loc;
        return res;
    }

    t_ast pointer_opt() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "pointer_opt";
        if (cmp("*")) {
            res.add_child(pointer());
        }
        res.loc = loc;
        return res;
    }

    auto struct_declarator_list() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "struct_declarator_list";
        while (true) {
            res.add_child(declarator());
            if (not cmp(",")) {
                break;
            }
            advance();
        }
        res.loc = loc;
        return res;
    }

    auto struct_or_union_specifier() {
        auto loc = peek().loc;
        t_ast res;
        if (kw("struct")) {
            advance();
            res.uu = "struct_or_union_specifier";
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
        } else {
            err("expected struct_or_union_specifier", loc);
        }
        res.loc = loc;
        return res;
    }

    auto is_type_specifier() {
        auto res = false;
        if (peek().uu == "keyword" and has(type_specifiers, peek().vv)) {
            res = true;
        } else if (kw("struct")) {
            res = true;
        }
        return res;
    }

    auto type_specifier() {
        auto loc = peek().loc;
        t_ast res;
        if (has(type_specifiers, peek().vv)) {
            res.uu = "type_specifier";
            res.vv = peek().vv;
            advance();
        } else if (kw("struct")) {
            res = struct_or_union_specifier();
        } else {
            err("expected type_specifier", loc);
        }
        res.loc = loc;
        return res;
    }

    auto specifier_qualifier_list() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "specifier_qualifier_list";
        while (is_type_specifier()) {
            res.add_child(type_specifier());
        }
        res.loc = loc;
        return res;
    }

    auto struct_declaration() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "struct_declaration";
        res.add_child(specifier_qualifier_list());
        res.add_child(struct_declarator_list());
        pop(";");
        res.loc = loc;
        return res;
    }

    t_ast struct_declaration_list() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "struct_declaration_list";
        while (not cmp("}")) {
            res.add_child(struct_declaration());
        }
        res.loc = loc;
        return res;
    }

    t_ast identifier() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "identifier";
        res.vv = pop("identifier");
        res.loc = loc;
        return res;
    }

    auto array_size() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "array_size";
        pop("[");
        if (cmp("]")) {
            advance();
        } else {
            res.add_child(const_exp());
            pop("]");
        }
        res.loc = loc;
        return res;
    }

    auto direct_declarator() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "direct_declarator";
        if (cmp("identifier")) {
            res.add_child(identifier());
        } else {
            pop("(");
            res.add_child(declarator());
            pop(")");
        }
        while (true) {
            if (cmp("[")) {
                res.add_child(array_size());
            } else {
                break;
            }
        }
        res.loc = loc;
        return res;
    }

    t_ast declarator() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "declarator";
        res.add_child(pointer_opt());
        res.add_child(direct_declarator());
        res.loc = loc;
        return res;
    }

    auto initializer() {
        return assign_exp();
    }

    auto init_declarator() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "init_declarator";
        res.add_child(declarator());
        if (cmp("=")) {
            advance();
            res.add_child(initializer());
        }
        res.loc = loc;
        return res;
    }

    auto declaration_specifiers() {
        return specifier_qualifier_list();
    }

    auto declaration() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "declaration";
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
        advance();
        res.loc = loc;
        return res;
    }

    auto compound_statement() {
        auto loc = peek().loc;
        t_ast res;
        res.uu = "compound_statement";
        pop("{");
        while (not cmp("}")) {
            res.add_child(block_item());
        }
        advance();
        res.loc = loc;
        return res;
    }

    auto selection_statement() {
        auto loc = peek().loc;
        auto kind = peek().uu;
        auto value = peek().vv;
        t_ast res;
        if (kw("if")) {
            advance();
            pop("(");
            res.uu = "if";
            res.add_child(exp());
            pop(")");
            res.add_child(statement());
            if (kw("else")) {
                advance();
                res.add_child(statement());
            }
        }
        res.loc = loc;
        return res;
    }

    auto jump_statement() {
        auto loc = peek().loc;
        auto kind = peek().uu;
        auto value = peek().vv;
        t_ast res;
        if (kw("return")) {
            advance();
            auto child = exp();
            pop(";");
            res.uu = "return";
            res.children = {child};
        } else if (kw("break")) {
            advance();
            pop(";");
            res.uu = "break";
        } else if (kw("continue")) {
            advance();
            pop(";");
            res.uu = "continue";
        } else {
            err("expected jump statement", loc);
        }
        res.loc = loc;
        return res;
    }

    auto iteration_statement() {
        auto loc = peek().loc;
        auto kind = peek().uu;
        auto value = peek().vv;
        t_ast res;
        if (kw("while")) {
            advance();
            pop("(");
            res.uu = "while";
            res.add_child(exp());
            pop(")");
            res.add_child(statement());
        } else if (kw("for")) {
            advance();
            res.uu = "for";
            pop("(");
            res.add_child(opt_exp(";"));
            res.add_child(opt_exp(";"));
            res.add_child(opt_exp(")"));
            res.add_child(statement());
        } else {
            err("expected iteration statement", loc);
        }
        res.loc = loc;
        return res;
    }

    auto labeled_statement() {
        return t_ast();
    }

    t_ast statement() {
        if (cmp("{")) {
            return compound_statement();
        } else if (cmp("keyword")) {
            auto v = peek().vv;
            if (has({"if", "switch"}, v)) {
                return selection_statement();
            } else if (has({"while", "do", "for"}, v)) {
                return iteration_statement();
            } else if (has({"goto", "continue", "break", "return"}, v)) {
                return jump_statement();
            } else if (has({"case", "default"}, v)) {
                return labeled_statement();
            } else {
                return exp_statement();
            }
        } else {
            return exp_statement();
        }
    }

    t_ast block_item() {
        t_ast res;
        auto loc = peek().loc;
        if (is_type_specifier()) {
            res = declaration();
        } else {
            res = statement();
        }
        res.loc = loc;
        return res;
    }

    auto function_definition() {
        auto loc = peek().loc;
        pop_kw("int");
        auto func_name = pop("identifier");
        pop("(");
        pop(")");
        auto children = compound_statement().children;
        auto res = t_ast("function", func_name, children);
        res.loc = loc;
        return res;
    }
}

t_ast parse_program(vector<t_lexeme>& n_ll) {
    init(n_ll);
    auto loc = peek().loc;
    auto res = t_ast("program");
    while (not end()) {
        res.add_child(function_definition());
    }
    res.loc = loc;
    return res;
}
