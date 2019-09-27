#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cctype>
#include <set>
#include <cassert>

#include "gen.hpp"
#include "type.hpp"
#include "misc.hpp"
#include "prog.hpp"
#include "exp.hpp"
#include "ctx.hpp"

t_prog prog;

namespace {
    void gen_compound_statement(const t_ast& ast, t_ctx ctx);
    void gen_statement(const t_ast& c, t_ctx& ctx);

    void err(const str& str, t_loc loc) {
        throw t_compile_error(str, loc);
    }

    _ is_integral_constant(const t_val& x) {
        return x.is_constant() and x.type().is_integral();
    }

    str make_anon_type_id() {
        static _ count = 0u;
        str res;
        res += "#";
        res += std::to_string(count);
        count++;
        return res;
    }

    void gen_init(const t_val& v, const t_ast& ini, t_ctx& ctx) {
        if (v.type().is_struct()) {
            for (size_t i = 0; i < v.type().length(); i++) {
                gen_init(gen_struct_member(v, i, ctx), ini[i], ctx);
            }
        } else if (v.type().is_array()) {
            for (size_t i = 0; i < v.type().length(); i++) {
                gen_init(gen_array_elt(v, i, ctx), ini[i], ctx);
            }
        } else {
            gen_convert_assign(v, gen_exp(ini[0], ctx), ctx);
        }
    }

    void gen_declaration(const t_ast& ast, t_ctx& ctx) {
        _ base = make_base_type(ast[0], ctx);
        for (size_t i = 1; i < ast.children.size(); i++) {
            _ type = base;
            _ name = unpack_declarator(type, ast[i][0], ctx);
            type = ctx.complete_type(type);
            if (not type.size()) {
                err("type has an unknown size", ast[i].loc);
            }
            _ asm_id = prog.def_var(ctx.asmt(type));
            _ val = t_val(asm_id, type, true);
            ctx.def_var(name, val);
            if (ast[i].children.size() > 1) {
                _& ini = ast[i][1];
                if (type.is_scalar()) {
                    t_ast exp;
                    if (ini[0].uu != "initializer") {
                        exp = ini[0];
                    } else {
                        if (ini[0][0].uu == "initializer") {
                            err("expected expresion", ini[0].loc);
                        }
                        exp = ini[0][0];
                    }
                    _ e = gen_exp(exp, ctx);
                    gen_convert_assign(val, e, ctx);
                } else {
                    if (type.is_struct() and ini[0].uu != "initializer") {
                        gen_assign(val, gen_exp(ini[0], ctx), ctx);
                    } else {
                        gen_init(val, ini, ctx);
                    }
                }
            }
        }
    }

    _ def_switch_labels(const t_type& t, const t_ast& ast, t_ctx& ctx) {
        if (ast.uu == "switch") {
            return;
        }
        if (ast.uu == "case") {
            try {
                _ x = gen_exp(ast[0], ctx);
                if (not is_integral_constant(x)) {
                    err("case value is not an integral constant", ast.loc);
                }
                x = gen_conversion(t, x, ctx);
                ctx.def_case(x, make_label());
            } catch (t_redefinition_error) {
                err("duplicate case value", ast.loc);
            }
        } else if (ast.uu == "default") {
            if (ctx.default_label() != "") {
                err("duplicate default label", ast.loc);
            }
            ctx.default_label(make_label());
        }
        for (_& c : ast.children) {
            def_switch_labels(t, c, ctx);
        }
    }

    void gen_switch(const t_ast& ast, t_ctx ctx) {
        ctx.enter_switch();
        ctx.break_label(make_label());
        _ x = gen_exp(ast[0], ctx);
        if (not x.type().is_integral()) {
            err("switch controlling exp must be integral", ast[0].loc);
        }
        gen_int_promotion(x, ctx);
        def_switch_labels(x.type(), ast[1], ctx);
        _ default_label = ctx.default_label();
        if (default_label == "") {
            default_label = ctx.break_label();
        }
        prog.switch_(ctx.asmv(x), default_label, ctx.get_asm_cases());
        gen_statement(ast[1], ctx);
        put_label(ctx.break_label());
    }

    void gen_statement(const t_ast& c, t_ctx& ctx) {
        if (c.uu == "case") {
            put_label(ctx.get_case_label());
            gen_statement(c[1], ctx);
        } else if (c.uu == "default") {
            put_label(ctx.default_label());
            gen_statement(c[0], ctx);
        } else if (c.uu == "switch") {
            gen_switch(c, ctx);
        } else if (c.uu == "if") {
            _ cond_true = make_label();
            _ cond_false = make_label();
            _ end = make_label();
            _ cond_val = gen_exp(c.children[0], ctx);
            if (not cond_val.type().is_scalar()) {
                err("controlling expression must have scalar type",
                    c.children[0].loc);
            }
            _ cmp_res = prog.make_new_id();
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
            _ val = gen_exp(c.children[0], ctx);
            prog.ret(ctx.asmv(val));
        } else if (c.uu == "compound_statement") {
            gen_compound_statement(c, ctx);
        } else if (c.uu == "while") {
            _ nctx = ctx;
            nctx.break_label(make_label());
            nctx.loop_body_end(make_label());
            _ loop_begin = make_label();
            _ loop_body = make_label();
            put_label(loop_begin);
            _ cond_val = gen_exp(c.children[0], nctx);
            prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                         nctx.break_label(), loop_body);
            put_label(loop_body);
            gen_statement(c.children[1], nctx);
            put_label(nctx.loop_body_end());
            prog.br(loop_begin);
            put_label(nctx.break_label());
        } else if (c.uu == "do_while") {
            _ nctx = ctx;
            nctx.break_label(make_label());
            nctx.loop_body_end(make_label());
            _ loop_begin = make_label();
            put_label(loop_begin);
            gen_statement(c.children[0], nctx);
            put_label(nctx.loop_body_end());
            _ cond_val = gen_exp(c.children[1], nctx);
            prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                         nctx.break_label(), loop_begin);
            put_label(nctx.break_label());
        } else if (c.uu == "for") {
            _ loop_begin = make_label();
            _ loop_body = make_label();
            _ nctx = ctx;
            nctx.break_label(make_label());
            nctx.loop_body_end(make_label());
            _ init_exp = c[0];
            if (not init_exp.children.empty()) {
                gen_exp(init_exp[0], nctx);
            }
            put_label(loop_begin);
            _& ctrl_exp = c[1];
            if (not ctrl_exp.children.empty()) {
                _ cond_val = gen_exp(ctrl_exp[0], nctx);
                prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                             nctx.break_label(), loop_body);
                put_label(loop_body, false);
            } else {
                put_label(loop_body);
            }
            gen_statement(c[3], nctx);
            put_label(nctx.loop_body_end());
            _& post_exp = c[2];
            if (not post_exp.children.empty()) {
                gen_exp(post_exp[0], nctx);
            }
            prog.br(loop_begin);
            put_label(nctx.break_label());
        } else if (c.uu == "break") {
            if (ctx.break_label() == "") {
                err("break not in loop or switch", c.loc);
            }
            prog.br(ctx.break_label());
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
            for (_& c : ast.children) {
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
        for (_& c : ast.children) {
            def_labels(c, ctx);
        }
    }

    _ gen_function(const t_ast& ast) {
        _& func_name = ast.vv;
        assert(func_name == "main");
        t_ctx ctx;
        ctx.func_name(func_name);
        def_labels(ast, ctx);
        for (_& c : ast.children) {
            gen_block_item(c, ctx);
        }
        prog.ret({"i32", "0"});
        prog.def_main();
    }
}

str make_label() {
    return prog.make_label();
}

void put_label(const str& s, bool f) {
    prog.put_label(s, f);
}

str unpack_declarator(t_type& type, const t_ast& t, t_ctx& ctx) {
    _ p = &(t[0]);
    while (not (*p).children.empty()) {
        type = make_pointer_type(type);
        p = &((*p)[0]);
    }
    if (t[1].uu == "opt" and t[1].children.empty()) {
        return "";
    }
    _& dd = (t[1].uu == "opt") ? t[1][0] : t[1];
    assert(dd.children.size() >= 1);
    _ i = dd.children.size() - 1;
    while (i != 0) {
        _& op = dd[i].uu;
        if (op == "array_size") {
            if (dd[i].children.size() != 0) {
                _& size_exp = dd[i][0];
                _ size_val = gen_exp(size_exp, ctx);
                if (not is_integral_constant(size_val)) {
                    err("bad array size", size_exp.loc);
                }
                if (size_val.type().is_signed()
                    and size_val.s_val() <= 0) {
                    err("array size must be positive", size_exp.loc);
                }
                type = make_array_type(type, size_val.u_val());
            } else {
                type = make_array_type(type);
            }
        }
        i--;
    }
    if (dd[0].uu == "identifier") {
        return dd[0].vv;
    } else {
        return unpack_declarator(type, dd[0], ctx);
    }
}

t_type make_base_type(const t_ast& ast, t_ctx& ctx) {
    t_type res;
    std::set<str> specifiers;
    if (ast.children.size() == 1
        and ast[0].uu == "struct_or_union_specifier") {
        _ struct_name = ast[0].vv;
        if (ast[0].children.empty()) {
            try {
                return ctx.scope_get_type_data(struct_name).type;
            } catch (t_undefined_name_error) {
                _ type = make_struct_type(struct_name);
                ctx.put_struct(struct_name, {type, prog.make_new_id()});
                return type;
            }
        }
        vec<str> field_name;
        vec<t_type> field_type;
        for (_& c : ast[0][0].children) {
            _ base = make_base_type(c[0], ctx);
            for (size_t i = 1; i < c.children.size(); i++) {
                _ type = base;
                _ name = unpack_declarator(type, c[i][0], ctx);
                if (type.is_incomplete()) {
                    type = ctx.complete_type(type);
                }
                if (type.is_incomplete()) {
                    err("field has incomplete type", c[i].loc);
                }
                field_name.push_back(name);
                field_type.push_back(type);
            }
        }
        if (struct_name.empty()) {
            struct_name = make_anon_type_id();
        }
        _ type = make_struct_type(struct_name, field_name,
                                  std::move(field_type));
        str id;
        try {
            _ data = ctx.scope_get_type_data(struct_name);
            id = data.asm_id;
            if (not (data.type.is_struct()
                     and data.type.fields().empty())) {
                err("redefinition", ast.loc);
            }
        } catch (t_undefined_name_error) {
            id = prog.make_new_id();
        }
        ctx.put_struct(struct_name, {type, id});
        prog.def_struct(id, ctx.asmt(type, true));
        return type;
    } else if (ast.children.size() == 1 and ast[0].uu == "enum") {
        _ name = (ast[0].vv == "") ? make_anon_type_id() : ast[0].vv;
        if (ast[0].children.empty()) {
            try {
                _ type = ctx.get_type_data(name).type;
                if (not type.is_enum()) {
                    err(name + " is not an enumeration", ast[0].loc);
                }
                return type;
            } catch (t_undefined_name_error) {
                err("undefined enum", ast.loc);
            }
        }
        _ cnt = 0;
        for (_& e : ast[0].children) {
            if (e.children.size() == 1) {
                cnt = stoi(gen_exp(e[0], ctx).asm_id());
            }
            ctx.def_var(e.vv, cnt);
            cnt++;
        }
        _ type = make_enum_type(name);
        try {
            ctx.def_enum(name, type);
        } catch (t_redefinition_error) {
            err("redefinition of enum " + name, ast.loc);
        }
        return type;
    }
    for (_& c : ast.children) {
        if (not (c.uu == "type_specifier"
                 and has(type_specifiers, c.vv))) {
            err("bad type specifier", c.loc);
        }
        if (specifiers.count(c.vv) != 0) {
            err("duplicate type specifier", c.loc);
        }
        specifiers.insert(c.vv);
    }
    _ cmp = [&specifiers](const std::set<str>& x) {
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

str gen_asm(const t_ast& ast) {
    for (_& c : ast.children) {
        gen_function(c);
    }
    return prog.assemble();
}
