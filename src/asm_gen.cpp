#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cctype>
#include <set>
#include <cassert>

#include "asm_gen.hpp"
#include "type.hpp"
#include "misc.hpp"
#include "prog.hpp"

using namespace std;

auto err(const string& str, t_loc loc = t_loc()) {
    throw t_compile_error(str, loc);
}

t_prog prog;

string make_anon_struct_id() {
    static auto count = 0u;
    string res;
    res += "#";
    res += to_string(count);
    count++;
    return res;
}

string make_label() {
    return prog.make_label();
}

void put_label(const std::string& s, bool f) {
    prog.put_label(s, f);
}

string func_line(const string& x) {
    return string("    ") + x + "\n";
}

void t_ctx::define_var(const std::string& name, t_type type) {
    type = complete_type(type);
    auto id = prog.def_var(get_asm_type(type));
    var_map.map[name] = {type, id};
}

void t_ctx::add_label(const t_ast& ast) {
    auto it = label_map.find(ast.vv);
    if (it != label_map.end()) {
        throw t_redefinition_error();
    }
    label_map[ast.vv] = make_label();
}

std::string t_ctx::get_asm_label(const t_ast& ast) const {
    auto it = label_map.find(ast.vv);
    if (it == label_map.end()) {
        throw t_unknown_id_error();
    }
    return (*it).second;
}

string t_ctx::get_asm_type(const t_type& t, bool expand) const {
    string res;
    if (t == char_type or t == u_char_type
        or t == s_char_type) {
        res = "i8";
    } else if (t == short_type or t == u_short_type) {
        res = "i16";
    } else if (t == int_type or t == u_int_type) {
        res = "i32";
    } else if (t == long_type or t == u_long_type) {
        res = "i64";
    } else if (t == float_type) {
        res = "float";
    } else if (t == double_type) {
        res = "double";
    } else if (t == long_double_type) {
        res = "double";
    } else if (t == void_pointer_type) {
        res += "i8*";
    } else if (is_pointer_type(t)) {
        res += get_asm_type(t.get_pointee_type());
        res += "*";
    } else if (is_array_type(t)) {
        res += "[";
        res += to_string(t.get_length()) + " x ";
        res += get_asm_type(t.get_element_type());
        res += "]";
    } else if (is_struct_type(t)) {
        if (expand) {
            res += "{ ";
            auto start = true;
            for (auto& m : t.get_members()) {
                if (not start) {
                    res += ", ";
                }
                start = false;
                res += get_asm_type(m.type);
            }
            res += " }";
        } else {
            res += type_map.get_asm_var(t.get_name());
        }
    } else {
        throw logic_error("get_asm_type " + stringify(t) + " fail");
    }
    return res;
}

t_asm_val t_ctx::get_asm_val(const t_val& val) const {
    auto type = get_asm_type(val.type);
    if (val.is_lvalue) {
        type += "*";
    }
    return t_asm_val{type, val.value};
}

t_type t_ctx::complete_type(const t_type& t) const {
    if (is_array_type(t)) {
        auto et = complete_type(t.get_element_type());
        return make_array_type(et, t.get_length());
    }
    if (not is_struct_type(t)) {
        return t;
    }
    if (not t.is_complete()) {
        return type_map.get_type(t.get_name());
    }
    auto mm = t.get_members();
    for (auto& m : mm) {
        m.type = complete_type(m.type);
    }
    return make_struct_type(t.get_name(), mm);
}

string t_ctx::define_struct(t_type type) {
    assert(is_struct_type(type));
    auto res = prog.make_new_id();
    auto& name = type.get_name();
    if (type_map.map.count(name) != 0) {
        if (not type_map.map[name].type.is_complete()) {
            type_map.map[name].type = type;
            res = type_map.map[name].asm_var;
        }
    } else {
        type_map.map[name] = {type, res};
    }
    auto& mm = type.get_members();
    if (not mm.empty()) {
        for (auto& m : mm) {
            if (is_struct_type(m.type)
                and not m.type.get_members().empty()) {
                define_struct(m.type);
            }
        }
        prog.def_struct(res, get_asm_type(type, true));
    }
    return res;
}

t_type make_base_type(const t_ast&, const t_ctx&);

void gen_compound_statement(const t_ast&, const t_ctx&);

string unpack_declarator(t_type& type, const t_ast& t) {
    auto p = &(t[0]);
    while (not (*p).children.empty()) {
        type = make_pointer_type(type);
        p = &((*p)[0]);
    }
    if (t[1].uu == "opt" and t[1].children.empty()) {
        return "";
    }
    auto& dd = (t[1].uu == "opt") ? t[1][0] : t[1];
    assert(dd.children.size() >= 1);
    auto i = dd.children.size() - 1;
    while (i != 0) {
        auto& op = dd[i].uu;
        if (op == "array_size") {
            if (dd[i].children.size() != 0) {
                assert (dd[i][0].uu == "integer_constant");
                auto arr_len = stoull(dd[i][0].vv);
                type = make_array_type(type, arr_len);
            } else {
                type = make_array_type(type);
            }
        }
        i--;
    }
    if (dd[0].uu == "identifier") {
        return dd[0].vv;
    } else {
        return unpack_declarator(type, dd[0]);
    }
}

t_type make_base_type(const t_ast& t, const t_ctx& ctx) {
    t_type res;
    set<string> specifiers;
    auto ts = [](auto s) {
        return t_ast("type_specifier", s);
    };
    if (t.children.size() == 1
        and t[0].uu == "struct_or_union_specifier") {
        vector<t_struct_member> members;
        if (t[0].children.empty()) {
            return make_struct_type(t[0].vv);
        }
        for (auto& c : t[0][0].children) {
            auto base = make_base_type(c[0], ctx);
            for (size_t i = 1; i < c.children.size(); i++) {
                auto type = base;
                auto id = unpack_declarator(type, c[i][0]);
                if (type.get_is_size_known() == false) {
                    auto _type = ctx.get_type(type.get_name());
                    type.set_size(_type.get_size(), _type.get_alignment());
                }
                members.push_back({id, type});
            }
        }
        string id;
        if (t[0].vv.empty()) {
            id = make_anon_struct_id();
        } else {
            id = t[0].vv;
        }
        return make_struct_type(id, members);
    }
    for (auto& c : t.children) {
        if (not (c.uu == "type_specifier"
                 and has(type_specifiers, c.vv))) {
            err("bad type specifier", c.loc);
        }
        if (specifiers.count(c.vv) != 0) {
            err("duplicate type specifier", c.loc);
        }
        specifiers.insert(c.vv);
    }
    auto cmp = [&specifiers](const set<string>& x) {
        return specifiers == x;
    };
    if (cmp({"char"})) {
        return char_type;
    } else if (cmp({"signed", "char"})) {
        return s_char_type;
    } else if (cmp({"unsigned", "char"})) {
        return u_char_type;
    } else if (cmp({"short"}) or cmp({"signed", "short"})
               or cmp({"short", "int"})
               or cmp({"signed", "short", "int"})) {
        return short_type;
    } else if (cmp({"unsigned", "short"})
               or cmp({"unsigned", "short", "int"})) {
        return u_short_type;
    } else if (cmp({"int"}) or cmp({"signed"}) or cmp({"signed", "int"})) {
        return int_type;
    } else if (cmp({"unsigned"}) or cmp({"unsigned", "int"})) {
        return u_int_type;
    } else if (cmp({"long"}) or cmp({"signed", "long"})
               or cmp({"long", "int"}) or cmp({"signed", "long", "int"})) {
        return long_type;
    } else if (cmp({"unsigned", "long"})
               or cmp({"unsigned", "long", "int"})) {
        return u_long_type;
    } else if (cmp({"float"})) {
        return float_type;
    } else if (cmp({"double"})) {
        return double_type;
    } else if (cmp({"long", "double"})) {
        return long_double_type;
    } else if (cmp({"void"})) {
        return void_type;
    }
    err("bad type ", t.loc);
}

void gen_init(const t_val& v, const t_ast& ini, t_ctx& ctx) {
    if (is_struct_type(v.type)) {
        auto i = 0;
        for (auto& c : v.type.get_members()) {
            gen_init(gen_struct_member(v, i, ctx), ini[i], ctx);
            i++;
        }
    } else if (is_array_type(v.type)) {
        auto i = 0;
        while (i < ini.children.size()) {
            gen_init(gen_array_elt(v, i, ctx), ini[i], ctx);
            i++;
        }
    } else {
        gen_convert_assign(v, gen_exp(ini[0], ctx), ctx);
    }
}

void gen_declaration(const t_ast& ast, t_ctx& ctx) {
    auto base = make_base_type(ast[0], ctx);
    if (is_struct_type(base)) {
        ctx.define_struct(base);
    }
    for (size_t i = 1; i < ast.children.size(); i++) {
        auto type = base;
        auto id = unpack_declarator(type, ast[i][0]);
        ctx.define_var(id, type);
        if (ast[i].children.size() > 1) {
            auto& ini = ast[i][1];
            auto v = t_val{ctx.get_var_asm_var(id), type, true};
            if (is_scalar_type(type)) {
                t_ast exp;
                if (ini[0].uu != "initializer") {
                    exp = ini[0];
                } else {
                    if (ini[0][0].uu == "initializer") {
                        err("expected expresion, got initializer list",
                            ini[0].loc);
                    }
                    exp = ini[0][0];
                }
                auto e = gen_exp(exp, ctx);
                gen_convert_assign(v, e, ctx);
            } else {
                if (is_struct_type(type)
                    and ini[0].uu != "initializer") {
                    gen_assign(v, gen_exp(ini[0], ctx), ctx);
                } else {
                    v.type = ctx.complete_type(v.type);
                    gen_init(v, ini, ctx);
                }
            }
        }
    }
}

void gen_statement(const t_ast& c, t_ctx& ctx) {
    if (c.uu == "if") {
        auto cond_true = make_label();
        auto cond_false = make_label();
        auto end = make_label();
        auto cond_val = gen_exp(c.children[0], ctx);
        if (not is_scalar_type(cond_val.type)) {
            err("controlling expression must have scalar type",
                c.children[0].loc);
        }
        auto cmp_res = prog.make_new_id();
        prog.cond_br(gen_is_zero_i1(cond_val, ctx), cond_false, cond_true);
        put_label(cond_true, false);
        gen_statement(c.children[1], ctx);
        prog.br(end);
        put_label(cond_false, false);
        if (c.children.size() == 3) {
            gen_statement(c.children[2], ctx);
        } else {
            prog.noop();
        }
        put_label(end);
        prog.noop();
    } else if (c.uu == "exp_statement") {
        if (not c.children.empty()) {
            gen_exp(c.children[0], ctx);
        }
    } else if (c.uu == "return") {
        auto val = gen_exp(c.children[0], ctx);
        prog.ret(ctx.get_asm_val(val));
    } else if (c.uu == "compound_statement") {
        gen_compound_statement(c, ctx);
    } else if (c.uu == "while") {
        auto nctx = ctx;
        nctx.set_loop_end(make_label());
        nctx.set_loop_body_end(make_label());
        auto loop_begin = make_label();
        auto loop_body = make_label();
        put_label(loop_begin);
        auto cond_val = gen_exp(c.children[0], nctx);
        prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                     nctx.get_loop_end(), loop_body);
        put_label(loop_body);
        gen_statement(c.children[1], nctx);
        put_label(nctx.get_loop_body_end());
        prog.br(loop_begin);
        put_label(nctx.get_loop_end());
    } else if (c.uu == "do_while") {
        auto nctx = ctx;
        nctx.set_loop_end(make_label());
        nctx.set_loop_body_end(make_label());
        auto loop_begin = make_label();
        put_label(loop_begin);
        gen_statement(c.children[0], nctx);
        put_label(nctx.get_loop_body_end());
        auto cond_val = gen_exp(c.children[1], nctx);
        prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                     nctx.get_loop_end(), loop_begin);
        put_label(nctx.get_loop_end());
    } else if (c.uu == "for") {
        auto loop_begin = make_label();
        auto loop_body = make_label();
        auto nctx = ctx;
        nctx.set_loop_end(make_label());
        nctx.set_loop_body_end(make_label());
        auto init_exp = c[0];
        if (not init_exp.children.empty()) {
            gen_exp(init_exp[0], nctx);
        }
        put_label(loop_begin);
        auto& ctrl_exp = c[1];
        if (not ctrl_exp.children.empty()) {
            auto cond_val = gen_exp(ctrl_exp[0], nctx);
            prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                         nctx.get_loop_end(), loop_body);
            put_label(loop_body, false);
        } else {
            put_label(loop_body);
        }
        gen_statement(c[3], nctx);
        put_label(nctx.get_loop_body_end());
        auto& post_exp = c[2];
        if (not post_exp.children.empty()) {
            gen_exp(post_exp[0], nctx);
        }
        prog.br(loop_begin);
        put_label(nctx.get_loop_end());
    } else if (c.uu == "break") {
        if (ctx.get_loop_end() == "") {
            err("break not in loop", c.loc);
        }
        prog.br(ctx.get_loop_end());
    } else if (c.uu == "continue") {
        if (ctx.get_loop_body_end() == "") {
            err("continue not in loop", c.loc);
        }
        prog.br(ctx.get_loop_body_end());
    } else if (c.uu == "goto") {
        prog.br(ctx.get_asm_label(c[0]));
    } else if (c.uu == "label") {
        put_label(ctx.get_asm_label(c));
        gen_statement(c[0], ctx);
    } else {
        throw runtime_error("unknown statement type");
    }
}

void gen_block_item(const t_ast& ast, t_ctx& ctx) {
    if (ast.uu == "declaration") {
        gen_declaration(ast, ctx);
    } else {
        gen_statement(ast, ctx);
    }
}

void gen_compound_statement(const t_ast& ast, const t_ctx& ctx) {
    auto nctx = ctx;
    if (ast.children.size() != 0) {
        for (auto& c : ast.children) {
            gen_block_item(c, nctx);
        }
    } else {
        prog.noop();
    }
}

void add_labels(const t_ast& ast, t_ctx& ctx) {
    if (ast.uu == "label") {
        try {
            ctx.add_label(ast);
        } catch (t_redefinition_error) {
            err("label redefinition", ast.loc);
        }
    }
    for (auto& c : ast.children) {
        add_labels(c, ctx);
    }
}

auto gen_function(const t_ast& ast) {
    auto& func_name = ast.vv;
    assert(func_name == "main");
    t_ctx ctx;
    ctx.set_func_name(func_name);
    add_labels(ast, ctx);
    for (auto& c : ast.children) {
        gen_block_item(c, ctx);
    }
    prog.ret({"i32", "0"});
    prog.def_main();
}

string gen_asm(const t_ast& ast) {
    for (auto& c : ast.children) {
        gen_function(c);
    }
    return prog.assemble();
}
