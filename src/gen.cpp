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
    std::unordered_map<str, bool> func_is_defined;

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

    void gen_initialization(const t_val& x, const t_ast& y, t_ctx& ctx) {
        if (x.type().is_scalar()) {
            const t_ast* yy;
            if (y[0].uu != "initializer") {
                yy = &y[0];
            } else {
                if (not (y[0].children.size() == 1
                         and y[0][0].uu != "initializer")) {
                    err("this cannot be an initializer for a scalar", y.loc);
                }
                yy = &y[0][0];
            }
            gen_convert_assign(x, gen_exp(*yy, ctx), ctx);
        } else {
            if (x.type().is_struct() and y[0].uu != "initializer") {
                gen_assign(x, gen_exp(y[0], ctx), ctx);
            } else {
                gen_init(x, y, ctx);
            }
        }
    }

    t_linkage linkage(t_storage_class sc, const str& name,
                      bool is_func, const t_ctx& ctx) {
        if (sc == t_storage_class::_typedef
            or (not is_func and not ctx.is_global()
                and sc != t_storage_class::_extern)) {
            return t_linkage::none;
        }
        if (sc == t_storage_class::_static and ctx.is_global()) {
            return t_linkage::internal;
        }
        if (sc == t_storage_class::_extern
            or (is_func and sc == t_storage_class::_none)) {
            try {
                _ prv = ctx.get_global_id_data(name);
                return prv.linkage;
            } catch (t_undefined_name_error) {
                return t_linkage::external;
            }
        }
        if (not is_func and ctx.is_global() and sc == t_storage_class::_none) {
            return t_linkage::external;
        }
        return t_linkage::none;
    }

    void gen_declaration(const t_ast& ast, t_ctx& ctx) {
        if (ast.children.size() == 1 and ast[0].children.size() == 1
            and ast[0][0].uu == "struct_or_union_specifier"
            and ast[0][0].children.empty()) {
            _& struct_name = ast[0][0].vv;
            try {
                ctx.scope_get_tag_data(struct_name).type;
            } catch (t_undefined_name_error) {
                _ type = make_struct_type(struct_name, prog.make_new_id());
                ctx.put_struct(struct_name, {type, type.as()});
            }
            return;
        }
        _ sc = storage_class(ast[0]);
        _ base = make_base_type(ast[0], ctx);

        for (size_t i = 1; i < ast.children.size(); i++) {
            _ type = base;
            _ name = unpack_declarator(type, ast[i][0], ctx);
            type = ctx.complete_type(type);
            _ _linkage = linkage(sc, name, type.is_function(), ctx);
            _ is_static = (_linkage != t_linkage::none
                           or sc == t_storage_class::_static);
            if (type.is_function()) {
                ctx.put_id(name, t_val(ctx.as(name), type), _linkage);
                if (_linkage == t_linkage::external) {
                    vec<str> params;
                    for (_& p : type.params()) {
                        params.push_back(p.as());
                    }
                    if (type.is_variadic()) {
                        params.push_back("...");
                    }
                }
                if (func_is_defined.count(name) == 0) {
                    func_is_defined[name] = false;
                }
            } else {
                if (_linkage == t_linkage::external) {
                    prog.declare_external(name, type.as());
                } else if (_linkage == t_linkage::none) {
                    if (sc == t_storage_class::_typedef) {
                        ctx.def_typedef_id(name, type);
                    } else {
                        if (not type.size()) {
                            err("type has an unknown size", ast[i].loc);
                        }
                        _ id = prog.def(type.as(), is_static);
                        _ val = t_val(id, type, true);
                        ctx.def_id(name, val);
                        if (ast[i].children.size() > 1) {
                            gen_initialization(val, ast[i][1], ctx);
                        }
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
        prog.switch_(ctx.as(x), default_label, ctx.get_asm_cases());
        gen_statement(ast[1], ctx);
        put_label(ctx.break_label());
    }

    void gen_while(const t_ast& ast, t_ctx ctx) {
        ctx.break_label(make_label());
        ctx.loop_body_end(make_label());
        _ loop_begin = make_label();
        _ loop_body = make_label();
        put_label(loop_begin);
        _ cond_val = gen_exp(ast[0], ctx);
        prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                     ctx.break_label(), loop_body);
        put_label(loop_body);
        gen_statement(ast[1], ctx);
        put_label(ctx.loop_body_end());
        prog.br(loop_begin);
        put_label(ctx.break_label());
    }

    void gen_do_while(const t_ast& ast, t_ctx ctx) {
        ctx.break_label(make_label());
        ctx.loop_body_end(make_label());
        _ loop_begin = make_label();
        put_label(loop_begin);
        gen_statement(ast[0], ctx);
        put_label(ctx.loop_body_end());
        _ cond_val = gen_exp(ast[1], ctx);
        prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                     ctx.break_label(), loop_begin);
        put_label(ctx.break_label());
    }

    void gen_for(const t_ast& ast, t_ctx ctx) {
        _ loop_begin = make_label();
        _ loop_body = make_label();
        ctx.break_label(make_label());
        ctx.loop_body_end(make_label());
        _ init_exp = ast[0];
        if (not init_exp.children.empty()) {
            gen_exp(init_exp[0], ctx);
        }
        put_label(loop_begin);
        _& ctrl_exp = ast[1];
        if (not ctrl_exp.children.empty()) {
            _ cond_val = gen_exp(ctrl_exp[0], ctx);
            prog.cond_br(gen_is_zero_i1(cond_val, ctx),
                         ctx.break_label(), loop_body);
            put_label(loop_body, false);
        } else {
            put_label(loop_body);
        }
        gen_statement(ast[3], ctx);
        put_label(ctx.loop_body_end());
        _& post_exp = ast[2];
        if (not post_exp.children.empty()) {
            gen_exp(post_exp[0], ctx);
        }
        prog.br(loop_begin);
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
            if (c.children.size() != 0) {
                _ val = gen_exp(c[0], ctx);
                gen_convert_assign(ctx.return_var(), val, ctx);
            }
            prog.br(ctx.func_end());
        } else if (c.uu == "compound_statement") {
            gen_compound_statement(c, ctx);
        } else if (c.uu == "while") {
            gen_while(c, ctx);
        } else if (c.uu == "do_while") {
            gen_do_while(c, ctx);
        } else if (c.uu == "for") {
            gen_for(c, ctx);
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

    _ gen_function(const t_ast& ast, t_ctx& octx) {
        _ type = make_base_type(ast[0], octx);
        _ ctx = octx;
        _ func_name = unpack_declarator(type, ast[1], ctx, true);
        func_is_defined[func_name] = true;
        if (not type.is_function()) {
            err("identifier does not have a function type", ast.loc);
        }
        type = ctx.complete_type(type);
        _ ret_tp = type.return_type();

        _ as_name = ctx.as(func_name);
        prog.func_name(as_name);
        prog.func_return_type(ret_tp.as());

        _ sc = storage_class(ast[0]);

        if (not (sc == t_storage_class::_none or sc == t_storage_class::_extern
                 or sc == t_storage_class::_static)) {
            err("function storage-class specifier can only be extern"
                " or static", ast[0].loc);
        }
        _ _linkage = linkage(sc, func_name, true, ctx);
        prog.func_internal(_linkage == t_linkage::internal);
        octx.put_id(func_name, t_val(as_name, type), _linkage);

        ctx.func_end(make_label());
        if (ret_tp != void_type) {
            ctx.return_var(t_val(prog.def(ret_tp.as()), ret_tp, true));
        }
        if (func_name == "main") {
            gen_assign(ctx.return_var(), t_val(0), ctx);
        }

        def_labels(ast[2], ctx);
        for (_& c : ast[2].children) {
            gen_block_item(c, ctx);
        }
        put_label(ctx.func_end());
        if (ret_tp == void_type) {
            prog.ret();
        } else {
            _ ret_as = prog.load(ctx.as(ctx.return_var()));
            prog.ret({ret_tp.as(), ret_as});
        }
        prog.end_func();
    }
}

str make_label() {
    return prog.make_label();
}

void put_label(const str& s, bool f) {
    prog.put_label(s, f);
}

str unpack_declarator(t_type& type, const t_ast& ast, t_ctx& ctx,
                      bool is_func_def) {
    _& kind = ast.uu;
    if (kind == "pointer") {
        type = make_pointer_type(type);
        return unpack_declarator(type, ast[0], ctx, is_func_def);
    } else if (kind == "array") {
        if (ast.children.size() == 2) {
            _& size_exp = ast[1];
            _ size_val = gen_exp(size_exp, ctx);
            if (not is_integral_constant(size_val)) {
                err("array size is not an integral constant", size_exp.loc);
            }
            if (size_val.type().is_signed() and size_val.s_val() <= 0) {
                err("array size must be positive", size_exp.loc);
            }
            type = make_array_type(type, size_val.u_val());
        } else {
            type = make_array_type(type);
        }
        return unpack_declarator(type, ast[0], ctx, is_func_def);
    } else if (kind == "function") {
        if (ast.children.size() == 2) {
            _ is_variadic = false;
            vec<t_type> params;
            for (_& p : ast[1].children) {
                if (p.uu == "...") {
                    is_variadic = true;
                } else {
                    _ param_type = make_base_type(p[0], ctx);
                    str param_name;
                    if (p.children.size() == 2) {
                        param_name = unpack_declarator(param_type, p[1], ctx);
                    }
                    param_type = ctx.complete_type(param_type);
                    if (param_type.is_function()) {
                        param_type = make_pointer_type(param_type);
                    } else if (param_type.is_array()) {
                        param_type = param_type.element_type();
                        param_type = make_pointer_type(param_type);
                    }
                    if (is_func_def and ast[0].uu == "identifier") {
                        _ p_as = prog.func_param(param_type.as());
                        ctx.def_id(param_name, t_val(p_as, param_type, true));
                    }
                    params.push_back(param_type);
                }
            }
            type = make_func_type(type, params, is_variadic);
        } else {
            type = make_func_type(type, {});
        }
        return unpack_declarator(type, ast[0], ctx, is_func_def);
    } else if (kind == "identifier") {
        return ast.vv;
    } else {
        throw std::logic_error(str(__func__) + " bad kind");
    }
}

t_type struct_specifier(const t_ast& ast, t_ctx& ctx) {
    _ struct_name = ast.vv;
    if (ast.children.empty()) {
        try {
            return ctx.get_tag_data(struct_name).type;
        } catch (t_undefined_name_error) {
            _ type = make_struct_type(struct_name, prog.make_new_id());
            ctx.put_struct(struct_name, {type, type.as()});
            return type;
        }
    }
    vec<str> field_name;
    vec<t_type> field_type;
    for (_& c : ast.children) {
        _ base = make_base_type(c[0], ctx);
        for (size_t i = 1; i < c.children.size(); i++) {
            _ type = base;
            _ name = unpack_declarator(type, c[i][0], ctx);
            type = ctx.complete_type(type);
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
    str id;
    try {
        _ data = ctx.scope_get_tag_data(struct_name);
        id = data.as;
        if (not (data.type.is_struct() and data.type.fields().empty())) {
            err("redefinition", ast.loc);
        }
    } catch (t_undefined_name_error) {
        id = prog.make_new_id();
    }
    _ type = make_struct_type(struct_name, std::move(field_name),
                              std::move(field_type), id);
    ctx.put_struct(struct_name, {type, id});
    prog.def_struct(id, type.as(true));
    return type;
}

t_type enum_specifier(const t_ast& ast, t_ctx& ctx) {
    _ name = (ast.vv == "") ? make_anon_type_id() : ast.vv;
    if (ast.children.empty()) {
        try {
            _ type = ctx.get_tag_data(name).type;
            if (not type.is_enum()) {
                err(name + " is not an enumeration", ast[0].loc);
            }
            return type;
        } catch (t_undefined_name_error) {
            err("undefined enum", ast.loc);
        }
    }
    _ cnt = 0;
    for (_& e : ast.children) {
        if (e.children.size() == 1) {
            cnt = stoi(gen_exp(e[0], ctx).as());
        }
        ctx.def_id(e.vv, cnt);
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

t_type simple_specifiers(const t_ast& ast, t_ctx&) {
    t_type res;
    std::set<str> specifiers;
    for (_& c : ast.children) {
        if (c.uu == "storage_class_specifier") {
            continue;
        }
        if (not (c.uu == "type_specifier"
                 and has(simple_type_specifiers, c.vv))) {
            cout << "c " << c.uu << " " << c.vv << "\n";
            err("bad specifier", c.loc);
        }
        if (specifiers.count(c.vv) != 0) {
            err("duplicate specifier", c.loc);
        }
        specifiers.insert(c.vv);
    }
    _ cmp = [&specifiers](const std::set<str>& x) {
        return specifiers == x;
    };
    if (cmp({"char"})) {
        res = char_type;
    } else if (cmp({"signed", "char"})) {
        res = s_char_type;
    } else if (cmp({"unsigned", "char"})) {
        res = u_char_type;
    } else if (cmp({"short"}) or cmp({"signed", "short"})
               or cmp({"short", "int"}) or cmp({"signed", "short", "int"})) {
        res = short_type;
    } else if (cmp({"unsigned", "short"})
               or cmp({"unsigned", "short", "int"})) {
        res = u_short_type;
    } else if (cmp({"int"}) or cmp({"signed"}) or cmp({"signed", "int"})) {
        res = int_type;
    } else if (cmp({"unsigned"}) or cmp({"unsigned", "int"})) {
        res = u_int_type;
    } else if (cmp({"long"}) or cmp({"signed", "long"})
               or cmp({"long", "int"}) or cmp({"signed", "long", "int"})) {
        res = long_type;
    } else if (cmp({"unsigned", "long"}) or cmp({"unsigned", "long", "int"})) {
        res = u_long_type;
    } else if (cmp({"float"})) {
        res = float_type;
    } else if (cmp({"double"})) {
        res = double_type;
    } else if (cmp({"long", "double"})) {
        res = long_double_type;
    } else if (cmp({"void"})) {
        res = void_type;
    } else {
        err("bad type ", ast.loc);
    }
    return res;
}

t_type make_base_type(const t_ast& ast, t_ctx& ctx) {
    for (_& c : ast.children) {
        if (c.uu == "storage_class_specifier") {
            continue;
        }
        if (c.uu == "struct_or_union_specifier") {
            return struct_specifier(c, ctx);
        } else if (c.uu == "enum") {
            return enum_specifier(c, ctx);
        } else if (c.uu == "typedef_name") {
            return ctx.get_typedef_type(c.vv);
        }
    }
    return simple_specifiers(ast, ctx);
}

t_storage_class storage_class(const t_ast& ast) {
    _ res = t_storage_class::_none;
    for (_& c : ast.children) {
        if (c.uu == "storage_class_specifier") {
            if (res != t_storage_class::_none) {
                err("more than one storage class specifier", ast.loc);
            }
            _& sc = c.vv;
            if (sc == "static") {
                return t_storage_class::_static;
            } else if (sc == "extern") {
                return t_storage_class::_extern;
            } else if (sc == "typedef") {
                return t_storage_class::_typedef;
            } else if (sc == "auto") {
                return t_storage_class::_auto;
            } else if (sc == "register") {
                return t_storage_class::_register;
            }
        }
    }
    return res;
}

void gen_program(const t_ast& ast) {
    t_ctx ctx;
    for (_& c : ast.children) {
        if (c.uu == "declaration") {
            gen_declaration(c, ctx);
        } else if (c.uu == "function_definition") {
            gen_function(c, ctx);
        }
    }
    for (_& [name, is_defined] : func_is_defined) {
        if (not is_defined) {
            _ t = ctx.get_id_data(name).val.type();
            vec<str> params_as;
            for (_& param : t.params()) {
                params_as.push_back(param.as());
            }
            prog.declare(t.return_type().as(), name, params_as);
        }
    }
}

str gen_asm(const t_ast& ast) {
    gen_program(ast);
    return prog.assemble();
}
