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
#include "exp.hpp"

using namespace std;

t_prog prog;

namespace {
    void gen_compound_statement(const t_ast& ast, t_ctx ctx);

    void err(const string& str, t_loc loc) {
        throw t_compile_error(str, loc);
    }

    string make_anon_type_id() {
        static auto count = 0u;
        string res;
        res += "#";
        res += to_string(count);
        count++;
        return res;
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
        for (size_t i = 1; i < ast.children.size(); i++) {
            auto type = ctx.complete_type(base);
            auto name = unpack_declarator(type, ast[i][0]);
            auto asm_id = prog.def_var(ctx.asmt(type));
            auto val = t_val{asm_id, type, true};
            ctx.def_var(name, val);
            if (ast[i].children.size() > 1) {
                auto& ini = ast[i][1];
                if (is_scalar_type(type)) {
                    t_ast exp;
                    if (ini[0].uu != "initializer") {
                        exp = ini[0];
                    } else {
                        if (ini[0][0].uu == "initializer") {
                            err("expected expresion", ini[0].loc);
                        }
                        exp = ini[0][0];
                    }
                    auto e = gen_exp(exp, ctx);
                    gen_convert_assign(val, e, ctx);
                } else {
                    if (is_struct_type(type) and ini[0].uu != "initializer") {
                        gen_assign(val, gen_exp(ini[0], ctx), ctx);
                    } else {
                        gen_init(val, ini, ctx);
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
            prog.ret(ctx.asmv(val));
        } else if (c.uu == "compound_statement") {
            gen_compound_statement(c, ctx);
        } else if (c.uu == "while") {
            auto nctx = ctx;
            nctx.loop_end(make_label());
            nctx.loop_body_end(make_label());
            auto loop_begin = make_label();
            auto loop_body = make_label();
            put_label(loop_begin);
            auto cond_val = gen_exp(c.children[0], nctx);
            prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                         nctx.loop_end(), loop_body);
            put_label(loop_body);
            gen_statement(c.children[1], nctx);
            put_label(nctx.loop_body_end());
            prog.br(loop_begin);
            put_label(nctx.loop_end());
        } else if (c.uu == "do_while") {
            auto nctx = ctx;
            nctx.loop_end(make_label());
            nctx.loop_body_end(make_label());
            auto loop_begin = make_label();
            put_label(loop_begin);
            gen_statement(c.children[0], nctx);
            put_label(nctx.loop_body_end());
            auto cond_val = gen_exp(c.children[1], nctx);
            prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                         nctx.loop_end(), loop_begin);
            put_label(nctx.loop_end());
        } else if (c.uu == "for") {
            auto loop_begin = make_label();
            auto loop_body = make_label();
            auto nctx = ctx;
            nctx.loop_end(make_label());
            nctx.loop_body_end(make_label());
            auto init_exp = c[0];
            if (not init_exp.children.empty()) {
                gen_exp(init_exp[0], nctx);
            }
            put_label(loop_begin);
            auto& ctrl_exp = c[1];
            if (not ctrl_exp.children.empty()) {
                auto cond_val = gen_exp(ctrl_exp[0], nctx);
                prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                             nctx.loop_end(), loop_body);
                put_label(loop_body, false);
            } else {
                put_label(loop_body);
            }
            gen_statement(c[3], nctx);
            put_label(nctx.loop_body_end());
            auto& post_exp = c[2];
            if (not post_exp.children.empty()) {
                gen_exp(post_exp[0], nctx);
            }
            prog.br(loop_begin);
            put_label(nctx.loop_end());
        } else if (c.uu == "break") {
            if (ctx.loop_end() == "") {
                err("break not in loop", c.loc);
            }
            prog.br(ctx.loop_end());
        } else if (c.uu == "continue") {
            if (ctx.loop_body_end() == "") {
                err("continue not in loop", c.loc);
            }
            prog.br(ctx.loop_body_end());
        } else if (c.uu == "goto") {
            prog.br(ctx.get_label_data(c[0].vv));
        } else if (c.uu == "label") {
            put_label(ctx.get_label_data(c.vv));
            gen_statement(c[0], ctx);
        } else {
            err("unknown statement", c.loc);
        }
    }

    void gen_block_item(const t_ast& ast, t_ctx& ctx) {
        if (ast.uu == "declaration") {
            gen_declaration(ast, ctx);
        } else {
            gen_statement(ast, ctx);
        }
    }

    void gen_compound_statement(const t_ast& ast, t_ctx ctx) {
        ctx.enter_scope();
        if (ast.children.size() != 0) {
            for (auto& c : ast.children) {
                gen_block_item(c, ctx);
            }
        } else {
            prog.noop();
        }
    }

    void def_labels(const t_ast& ast, t_ctx& ctx) {
        if (ast.uu == "label") {
            try {
                ctx.def_label(ast.vv, make_label());
            } catch (t_redefinition_error) {
                err("label redefinition", ast.loc);
            }
        }
        for (auto& c : ast.children) {
            def_labels(c, ctx);
        }
    }

    auto gen_function(const t_ast& ast) {
        auto& func_name = ast.vv;
        assert(func_name == "main");
        t_ctx ctx;
        ctx.func_name(func_name);
        def_labels(ast, ctx);
        for (auto& c : ast.children) {
            gen_block_item(c, ctx);
        }
        prog.ret({"i32", "0"});
        prog.def_main();
    }
}

string t_ctx::asmt(const t_type& t, bool expand) const {
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
        res += asmt(t.get_pointee_type());
        res += "*";
    } else if (is_array_type(t)) {
        res += "[";
        res += to_string(t.get_length()) + " x ";
        res += asmt(t.get_element_type());
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
                res += asmt(m.type);
            }
            res += " }";
        } else {
            res += get_type_data(t.get_name()).asm_id;
        }
    } else if (is_enum_type(t)) {
        res += "i32";
    } else {
        throw logic_error("asmt " + stringify(t) + " fail");
    }
    return res;
}

t_asm_val t_ctx::asmv(const t_val& val) const {
    auto type = asmt(val.type);
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
        return get_type_data(t.get_name()).type;
    }
    auto mm = t.get_members();
    for (auto& m : mm) {
        m.type = complete_type(m.type);
    }
    return make_struct_type(t.get_name(), mm);
}

string make_label() {
    return prog.make_label();
}

void put_label(const std::string& s, bool f) {
    prog.put_label(s, f);
}

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

t_type make_base_type(const t_ast& ast, t_ctx& ctx) {
    t_type res;
    set<string> specifiers;
    auto ts = [](auto s) {
        return t_ast("type_specifier", s);
    };
    if (ast.children.size() == 1
        and ast[0].uu == "struct_or_union_specifier") {
        if (ast[0].children.empty()) {
            return make_struct_type(ast[0].vv);
        }
        vector<t_struct_member> members;
        for (auto& c : ast[0][0].children) {
            auto base = make_base_type(c[0], ctx);
            for (size_t i = 1; i < c.children.size(); i++) {
                auto type = base;
                auto name = unpack_declarator(type, c[i][0]);
                if (type.get_is_size_known() == false) {
                    auto& _type = ctx.get_type_data(type.get_name()).type;
                    type.set_size(_type.get_size(), _type.get_alignment());
                }
                members.push_back({name, type});
            }
        }
        string name;
        if (ast[0].vv.empty()) {
            name = make_anon_type_id();
        } else {
            name = ast[0].vv;
        }
        auto type = make_struct_type(name, members);
        auto id = prog.make_new_id();
        if (not members.empty()) {
            prog.def_struct(id, ctx.asmt(type, true));
        }
        ctx.def_type(name, {type, id});
        return type;
    } else if (ast.children.size() == 1 and ast[0].uu == "enum") {
        auto name = (ast[0].vv == "") ? make_anon_type_id() : ast[0].vv;
        if (ast[0].children.empty()) {
            try {
                auto type = ctx.get_type_data(name).type;
                if (not is_enum_type(type)) {
                    err(name + " is not an enumeration", ast[0].loc);
                }
                return type;
            } catch (t_undefined_name_error) {
                err("undefined enum", ast.loc);
            }
        }
        int cnt = 0;
        for (auto& e : ast[0].children) {
            if (e.children.size() == 1) {
                cnt = stoi(gen_exp(e[0], ctx).value);
            }
            ctx.def_var(e.vv, make_constant(cnt));
            cnt++;
        }
        auto type = make_enum_type(name);
        try {
            ctx.def_type(name, {type});
        } catch (t_redefinition_error) {
            err("redefinition of enum " + name, ast.loc);
        }
        return type;
    }
    for (auto& c : ast.children) {
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
    err("bad type ", ast.loc);
}

string gen_asm(const t_ast& ast) {
    for (auto& c : ast.children) {
        gen_function(c);
    }
    return prog.assemble();
}
