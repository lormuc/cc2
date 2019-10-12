#include <vector>
#include <string>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <cassert>
#include <list>

#include "misc.hpp"
#include "ast.hpp"
#include "lex.hpp"
#include "gen.hpp"

const _ debug = true;

extern const vec<str> simple_type_specifiers = {
    "void", "char", "short", "int", "long", "float", "double",
    "signed", "unsigned"
};

class t_ast_ctx {
    vec<std::unordered_map<str, bool>> typedef_names;
    std::list<t_lexeme>::const_iterator pos;
    t_ast ast;
public:
    t_ast_ctx()
        : typedef_names({{}}) {
    }
    void init(std::list<t_lexeme>::const_iterator start) {
        pos = start;
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
    void advance() {
        pos++;
    }
    bool at_end() {
        return (*pos).uu == "eof";
    }
};

namespace {
    t_ast exp();
    t_ast assign_exp();
    t_ast cast_exp();
    t_ast block_item();
    t_ast statement();
    t_ast declarator();
    t_ast struct_declaration_list();
    t_ast identifier();
    t_ast abstract_declarator();
    t_ast declaration_specifiers();
    t_ast pointer();
    t_ast initializer();
    t_ast un_exp();
    t_ast cond_exp();
    t_ast type_name();
    t_ast const_exp();

    t_ast_ctx ctx;

    _ init(_ pos) {
        ctx.init(pos);
    }

    _& peek() {
        return ctx.peek();
    }

    _ get_state() {
        return ctx;
    }

    _ set_state(const _& x) {
        ctx = x;
    }

    _ advance() {
        ctx.advance();
    }

    _ at_end() {
        return ctx.at_end();
    }

    _ add_child_opt(_& res, _ f) {
        try {
            res.add_child(f());
        } catch (t_parse_error) {
        }
    }

    _ apply_rule(_ rule, const str& rule_name) {
        if (debug) {
            _ loc = peek().loc;
            cout << loc.line << ":" << loc.column << " " << rule_name << "\n";
        }
        _ old_state = get_state();
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

    _ opt(_ f) {
        _ res = t_ast("opt", peek().loc);
        try {
            res.add_child(f());
        } catch (t_parse_error) {
        }
        return res;
    }

    _ cmp(const str& name) {
        return peek().uu == name;
    }

    _ pop(const str& name) {
        if (not cmp(name)) {
            throw t_parse_error("expected " + name, peek().loc);
        }
        _ res = peek().vv;
        advance();
        return res;
    }

    _ prim_exp_() {
        _ kind = peek().uu;
        _ value = peek().vv;
        _ loc = peek().loc;
        t_ast res;
        if (kind == "identifier") {
            res = t_ast(kind, value, loc);
            advance();
        } else if (kind == "integer_constant" or kind == "floating_constant"
                   or kind == "char_constant") {
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

    _ postfix_exp_() {
        _ res = prim_exp();
        while (true) {
            _ loc = peek().loc;
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
            } else if (cmp("->")) {
                advance();
                res = t_ast("arrow", {res, identifier()});
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

    _ sizeof_exp_() {
        _ res = t_ast("sizeof_exp", peek().loc);
        pop("sizeof");
        res.add_child(un_exp());
        return res;
    }
    def_rule(sizeof_exp);

    _ sizeof_type_() {
        _ res = t_ast("sizeof_type", peek().loc);
        pop("sizeof");
        pop("(");
        res.add_child(type_name());
        pop(")");
        return res;
    }
    def_rule(sizeof_type);

    _ un_exp_() {
        t_ast res;
        vec<str> un_ops = {"&", "*", "+", "-", "~", "!"};
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

    _ parameter_declaration_() {
        _ res = t_ast("parameter_declaration", peek().loc);
        res.add_child(declaration_specifiers());
        try {
            res.add_child(abstract_declarator());
        } catch (t_parse_error) {
            try {
                res.add_child(declarator());
            } catch (t_parse_error) {
            }
        }
        return res;
    }
    def_rule(parameter_declaration);

    _ parameter_type_list_() {
        _ res = t_ast("parameter_type_list", peek().loc);
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

    _ abstract_subdeclarator_() {
        pop("(");
        _ e = abstract_declarator();
        pop(")");
        return e;
    }
    def_rule(abstract_subdeclarator);

    _ direct_abstract_declarator_() {
        _ res = t_ast();
        _ empty = true;
        try {
            res = abstract_subdeclarator();
            empty = false;
        } catch (t_parse_error) {
            res = t_ast("identifier", "", peek().loc);
        }
        while (true) {
            _ loc = peek().loc;
            if (cmp("[")) {
                advance();
                res = t_ast("array", {res});
                if (cmp("]")) {
                    advance();
                } else {
                    res.add_child(const_exp());
                    pop("]");
                }
                empty = false;
            } else if (cmp("(")) {
                advance();
                res = t_ast("function", {res});
                if (cmp(")")) {
                    advance();
                } else {
                    res.add_child(parameter_type_list());
                    pop(")");
                }
                empty = false;
            } else {
                break;
            }
            res.loc = loc;

        }
        if (empty) {
            throw t_parse_error(res.loc);
        }
        return res;
    }
    def_rule(direct_abstract_declarator);

    _ abstract_declarator_() {
        if (cmp("*")) {
            _ res = t_ast("pointer", peek().loc);
            advance();
            try {
                res.add_child(abstract_declarator());
            } catch (t_parse_error) {
                if (not (peek().uu == "eof" or peek().uu == ")"
                         or peek().uu == ",")) {
                    throw;
                }
                res.add_child(t_ast("identifier", "", peek().loc));
            }
            return res;
        } else {
            return direct_abstract_declarator();
        }
    }
    def_rule(abstract_declarator);

    _ type_name_() {
        _ res = t_ast("type_name", peek().loc);
        res.add_child(declaration_specifiers());
        try {
            res.add_child(abstract_declarator());
        } catch (t_parse_error) {
        }
        return res;
    }
    def_rule(type_name);

    _ cast_() {
        _ res = t_ast("cast", peek().loc);
        pop("(");
        res.add_child(type_name());
        pop(")");
        res.add_child(cast_exp());
        return res;
    }
    def_rule(cast);

    _ cast_exp_() { return or_(un_exp, cast); }
    def_rule(cast_exp);

    _ left_assoc_bin_op(const vec<str>& ops,
                        std::function<t_ast()> subexp) {
        _ res = subexp();
        while (true) {
            _ op = peek().uu;
            if (not has(ops, op)) {
                break;
            }
            _ loc = peek().loc;
            advance();
            _ t = subexp();
            res = t_ast(op, {res, t});
            res.loc = loc;
        }
        return res;
    }

    _ mul_exp_() {
        return left_assoc_bin_op({"*", "/", "%"}, cast_exp);
    }
    def_rule(mul_exp);

    _ add_exp_() {
        return left_assoc_bin_op({"+", "-"}, mul_exp);
    }
    def_rule(add_exp);

    _ shift_exp_() {
        return left_assoc_bin_op({"<<", ">>"}, add_exp);
    }
    def_rule(shift_exp);

    _ rel_exp_() {
        return left_assoc_bin_op({"<", "<=", ">", ">="}, shift_exp);
    }
    def_rule(rel_exp);;

    _ eql_exp_() {
        return left_assoc_bin_op({"==", "!="}, rel_exp);
    }
    def_rule(eql_exp);

    _ bit_and_exp_() {
        return left_assoc_bin_op({"&"}, eql_exp);
    }
    def_rule(bit_and_exp);

    _ bit_xor_exp_() {
        return left_assoc_bin_op({"^"}, bit_and_exp);
    }
    def_rule(bit_xor_exp);

    _ bit_or_exp_() {
        return left_assoc_bin_op({"|"}, bit_xor_exp);
    }
    def_rule(bit_or_exp);

    _ and_exp_() {
        return left_assoc_bin_op({"&&"}, bit_or_exp);
    }
    def_rule(and_exp);

    _ or_exp_() {
        return left_assoc_bin_op({"||"}, and_exp);
    }
    def_rule(or_exp);

    _ cond_exp_() {
        _ res = or_exp();
        if (peek().uu == "?") {
            _ loc = peek().loc;
            advance();
            _ y = exp();
            pop(":");
            _ z = cond_exp();
            res = t_ast("?:", {res, y, z});
            res.loc = loc;
        }
        return res;
    }
    def_rule(cond_exp);

    _ assign_() {
        _ x = un_exp();
        vec<str> ops = {
            "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=",
            "^=", "|="
        };
        _ op = peek().uu;
        if (not has(ops, op)) {
            throw t_parse_error("expected assignment operator",
                                peek().loc);
        }
        _ res = t_ast(op, peek().loc);
        advance();
        res.add_child(x);
        res.add_child(assign_exp());
        return res;
    }
    def_rule(assign);

    _ assign_exp_() { return or_(assign, cond_exp); }
    def_rule(assign_exp);

    _ exp_() {
        return left_assoc_bin_op({","}, assign_exp);
    }
    def_rule(exp);

    t_ast const_exp() {
        return cond_exp();
    }

    _ exp_statement_() {
        _ res = t_ast("exp_statement", peek().loc);
        add_child_opt(res, exp);
        pop(";");
        return res;
    }
    def_rule(exp_statement);

    _ pointer_() {
        _ res = t_ast("pointer", peek().loc);
        pop("*");
        add_child_opt(res, pointer);
        return res;
    }
    def_rule(pointer);

    _ struct_declarator_list_() {
        _ res = t_ast("struct_declarator_list", peek().loc);
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

    _ struct_or_union_specifier_() {
        _ res = t_ast("struct_or_union_specifier", peek().loc);
        pop("struct");
        if (cmp("identifier")) {
            res.vv = peek().vv;
            advance();
            if (cmp("{")) {
                advance();
                res.children = struct_declaration_list().children;
                pop("}");
            }
        } else {
            pop("{");
            res.children = struct_declaration_list().children;
            pop("}");
        }
        return res;
    }
    def_rule(struct_or_union_specifier);

    _ enumerator_() {
        _ res = t_ast("enumerator", peek().loc);
        res.vv = pop("identifier");
        if (cmp("=")) {
            advance();
            res.add_child(const_exp());
        }
        return res;
    }
    def_rule(enumerator);

    _ enum_specifier_() {
        _ res = t_ast("enum", peek().loc);
        pop("enum");
        if (cmp("identifier")) {
            res.vv = peek().vv;
            advance();
        }
        if (cmp("{")) {
            advance();
            while (true) {
                res.add_child(enumerator());
                if (not cmp(",")) {
                    break;
                }
                advance();
            }
            pop("}");
        }
        if (peek().loc == res.loc) {
            throw t_parse_error(res.loc);
        }
        return res;
    }
    def_rule(enum_specifier);

    _ type_specifier_() {
        if (has(simple_type_specifiers, peek().uu)) {
            _ res = t_ast("type_specifier", peek().uu, peek().loc);
            advance();
            return res;
        } else if (cmp("struct") or cmp("union")) {
            return struct_or_union_specifier();
        } else if (cmp("enum")) {
            return enum_specifier();
        } else {
            throw t_parse_error(peek().loc);
        }
    }
    def_rule(type_specifier);

    _ typedef_name_() {
        if (cmp("identifier") and ctx.is_typedef_name(peek().vv)) {
            _ res = t_ast("typedef_name", peek().vv, peek().loc);
            advance();
            return res;
        } else {
            throw t_parse_error(peek().loc);
        }
    }
    def_rule(typedef_name);

    _ struct_declaration_() {
        _ res = t_ast("struct_declaration", peek().loc);
        res.add_child(declaration_specifiers());
        res.add_child(struct_declarator_list());
        pop(";");
        return res;
    }
    def_rule(struct_declaration);

    _ struct_declaration_list_() {
        _ res = t_ast("struct_declaration_list", peek().loc);
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

    _ identifier_() {
        _ res = t_ast("identifier", peek().loc);
        res.vv = pop("identifier");
        return res;
    }
    def_rule(identifier);

    _ subdeclarator_() {
        pop("(");
        _ e = declarator();
        pop(")");
        return e;
    }
    def_rule(subdeclarator);

    _ direct_declarator_() {
        _ res = or_(subdeclarator,
                    identifier);
        while (true) {
            _ loc = peek().loc;
            if (cmp("[")) {
                advance();
                res = t_ast("array", {res});
                if (cmp("]")) {
                    advance();
                } else {
                    res.add_child(const_exp());
                    pop("]");
                }
            } else if (cmp("(")) {
                advance();
                res = t_ast("function", {res});
                if (cmp(")")) {
                    advance();
                } else {
                    res.add_child(parameter_type_list());
                    pop(")");
                }
            } else {
                break;
            }
            res.loc = loc;
        }
        return res;
    }
    def_rule(direct_declarator);

    _ declarator_() {
        if (cmp("*")) {
            _ res = t_ast("pointer", peek().loc);
            advance();
            res.add_child(declarator());
            return res;
        } else {
            return direct_declarator();
        }
    }
    def_rule(declarator);

    _ initializer_() {
        try {
            return assign_exp();
        } catch (t_parse_error) {
            _ res = t_ast("initializer_list", peek().loc);
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
            return res;
        }
    }
    def_rule(initializer);

    _ init_declarator_() {
        _ res = t_ast("init_declarator", peek().loc);
        res.add_child(declarator());
        if (cmp("=")) {
            advance();
            res.add_child(initializer());
        }
        return res;
    }
    def_rule(init_declarator);

    _ storage_class_specifier_() {
        if (cmp("static") or cmp("extern") or cmp("register") or cmp("auto")
            or cmp("typedef")) {
            _ res = t_ast("storage_class_specifier", peek().uu, peek().loc);
            advance();
            return res;
        } else {
            throw t_parse_error(peek().loc);
        }
    }
    def_rule(storage_class_specifier);

    _ declaration_specifiers_() {
        _ res = t_ast("declaration_specifiers", peek().loc);
        _ can_be_typedef_name = true;
        while (true) {
            try {
                if (can_be_typedef_name
                    and cmp("identifier") and ctx.is_typedef_name(peek().vv)) {
                    res.add_child(typedef_name());
                    break;
                } else {
                    _ x = or_(type_specifier, storage_class_specifier);
                    res.add_child(x);
                    if (x.uu != "storage_class_specifier"
                        and x.uu != "type_qualifier") {
                        can_be_typedef_name = false;
                    }
                }
            } catch (t_parse_error) {
                break;
            }
        }
        if (res.children.empty()) {
            throw t_parse_error(res.loc);
        }
        return res;
    }
    def_rule(declaration_specifiers);

    _ find_id(const _& ast) {
        if (ast.uu == "identifier") {
            return ast.vv;
        } else {
            return find_id(ast[0]);
        }
    }

    _ declaration_() {
        _ res = t_ast("declaration", peek().loc);
        res.add_child(declaration_specifiers());
        _ is_typedef = (storage_class(res[0]) == t_storage_class::_typedef);
        if (not cmp(";")) {
            while (true) {
                _ ini_decl = init_declarator();
                _ id = find_id(ini_decl);
                ctx.put(id, is_typedef);
                res.add_child(ini_decl);
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

    _ compound_statement_() {
        ctx.enter_scope();
        _ res = t_ast("compound_statement", peek().loc);
        pop("{");
        while (not cmp("}")) {
            res.add_child(block_item());
        }
        advance();
        ctx.leave_scope();
        return res;
    }
    def_rule(compound_statement);

    _ if_statement_() {
        _ res = t_ast("if", peek().loc);
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

    _ label_() {
        _ id = pop("identifier");
        _ res = t_ast("label", id, peek().loc);
        pop(":");
        res.add_child(statement());
        return res;
    }
    def_rule(label);

    _ while_statement_() {
        _ res = t_ast("while", peek().loc);
        pop("while");
        pop("(");
        res.add_child(exp());
        pop(")");
        res.add_child(statement());
        return res;
    }
    def_rule(while_statement);

    _ do_while_statement_() {
        _ res = t_ast("do_while", peek().loc);
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

    _ for_statement_() {
        _ res = t_ast("for", peek().loc);
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

    _ goto_statement_() {
        _ res = t_ast("goto", peek().loc);
        pop("goto");
        res.add_child(identifier());
        pop(";");
        return res;
    }
    def_rule(goto_statement);

    _ continue_statement_() {
        _ res = t_ast("continue", peek().loc);
        pop("continue");
        pop(";");
        return res;
    }
    def_rule(continue_statement);

    _ break_statement_() {
        _ res = t_ast("break", peek().loc);
        pop("break");
        pop(";");
        return res;
    }
    def_rule(break_statement);

    _ return_statement_() {
        _ res = t_ast("return", peek().loc);
        pop("return");
        if (not cmp(";")) {
            res.add_child(exp());
        }
        pop(";");
        return res;
    }
    def_rule(return_statement);

    _ case_statement_() {
        _ res = t_ast("case", peek().loc);
        pop("case");
        res.add_child(const_exp());
        pop(":");
        res.add_child(statement());
        return res;
    }
    def_rule(case_statement);

    _ default_statement_() {
        _ res = t_ast("default", peek().loc);
        pop("default");
        pop(":");
        res.add_child(statement());
        return res;
    }
    def_rule(default_statement);

    _ switch_statement_() {
        _ res = t_ast("switch", peek().loc);
        pop("switch");
        pop("(");
        res.add_child(exp());
        pop(")");
        res.add_child(statement());
        return res;
    }
    def_rule(switch_statement);

    _ statement_() {
        return or_(switch_statement,
                   case_statement,
                   default_statement,
                   label,
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

    _ block_item_() { return or_(statement, declaration); }
    def_rule(block_item);

    _ function_definition_() {
        _ res = t_ast("function_definition", peek().loc);
        res.add_child(declaration_specifiers());
        res.add_child(declarator());
        res.add_child(compound_statement());
        return res;
    }
    def_rule(function_definition);

    _ external_declaration_() {
        return or_(declaration, function_definition);
    }
    def_rule(external_declaration);
}

t_ast parse_program(std::list<t_lexeme>::const_iterator start) {
    init(start);
    _ res = t_ast("program", peek().loc);
    while (not at_end()) {
        res.add_child(external_declaration());
    }
    return res;
}
