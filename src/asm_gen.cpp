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

using namespace std;

namespace {
    string asm_funcs;
    string func_body;
    string global_storage;
    string asm_type_defs;
    string func_var_alloc;

    string make_new_id();

    class t_bad_operands : public runtime_error {
    public:
        t_bad_operands(const string& str)
            : runtime_error("bad operands to " + str)
        {}
    };

    struct t_exp_value {
        string value;
        t_type type;
        bool is_lvalue = false;
    };

    auto err(const string& str, t_loc loc = t_loc()) {
        throw t_compile_error(str, loc);
    }

    string make_anon_struct_id() {
        static auto count = 0u;
        string res;
        res += "#";
        res += to_string(count);
        count++;
        return res;
    }

    auto add_string_literal(string str) {
        static auto count = 0u;
        string res;
        res += "@str_";
        res += to_string(count);
        count++;
        global_storage += res;
        global_storage += " = private unnamed_addr constant [";
        global_storage += to_string(str.length() + 1);
        global_storage += " x i8] c\"";
        global_storage += print_bytes(str);
        global_storage += "\\00\"\n";
        return res;
    }

    string make_label() {
        static auto count = 0u;
        count++;
        return string("%l") + to_string(count);
    }

    auto func_line(const string& x) {
        return string("    ") + x + "\n";
    }

    auto a(const string& s) {
        func_body += func_line(s);
    }

    auto a(const string& s0, const string& s1) {
        func_body += func_line(s0 + " " + s1);
    }

    auto fun(const string& func_name, const t_exp_value& a0,
             const t_exp_value& a1) {
        return func_name + " " + a0.value + ", " + a1.value;
    }

    auto fun(const string& func_name, const string& a0,
             const string& a1) {
        return func_name + " " + a0 + ", " + a1;
    }

    auto let(const t_exp_value& v, const string& s) {
        a(v.value + " = " + s);
    }

    auto let(const string& v, const string& s) {
        a(v + " = " + s);
    }

    class t_unknown_id_error {};
    class t_redefinition_error {};

    class t_ctx {
        struct t_scope_map {
            struct t_data {
                t_type type;
                string asm_var;
            };
            unordered_map<string, t_data> map;
            unordered_map<string, t_data> outer_map;
            auto& get_data(const string& id) {
                auto it = map.find(id);
                if (it != map.end()) {
                    return (*it).second;
                }
                auto oit = outer_map.find(id);
                if (oit != outer_map.end()) {
                    return (*oit).second;
                } else {
                    throw t_unknown_id_error();
                }
            }
            const auto& get_data(const string& id) const {
                auto it = map.find(id);
                if (it != map.end()) {
                    return (*it).second;
                }
                auto oit = outer_map.find(id);
                if (oit != outer_map.end()) {
                    return (*oit).second;
                } else {
                    throw t_unknown_id_error();
                }
            }
            t_scope_map() {}
            t_scope_map(const t_scope_map& x) {
                outer_map = x.outer_map;
                for (const auto& [id, data] : x.map) {
                    outer_map[id] = data;
                }
            }
            auto get_type(const string& id) const {
                return get_data(id).type;
            }
            auto get_asm_var(const string& id) const {
                return get_data(id).asm_var;
            }
            auto can_define(const string& id) const {
                return map.count(id) == 0;
            }
            auto has(const string& id) const {
                return map.count(id) != 0 or outer_map.count(id)!= 0;
            }
        };
        t_scope_map var_map;
        t_scope_map type_map;
        string loop_body_end;
        string loop_end;
        string func_name;
        unordered_map<string, string> label_map;
    public:
        void add_label(const t_ast& ast) {
            auto it = label_map.find(ast.vv);
            if (it != label_map.end()) {
                throw t_redefinition_error();
            }
            label_map[ast.vv] = make_label();
        }

        string get_asm_label(const t_ast& ast) const {
            auto it = label_map.find(ast.vv);
            if (it == label_map.end()) {
                throw t_unknown_id_error();
            }
            return (*it).second;
        }

        auto set_loop_body_end(const string& x) {
            loop_body_end = x;
        }

        const auto& get_loop_body_end() {
            return loop_body_end;
        }

        auto set_loop_end(const string& x) {
            loop_end = x;
        }

        const auto& get_loop_end() {
            return loop_end;
        }

        auto set_func_name(const string& x) {
            func_name = x;
        }

        const auto& get_func_name() {
            return func_name;
        }

        string get_asm_type(const t_type& t) const {
            string res;
            if (t == bool_type) {
                res = "i1";
            } else if (t == char_type or t == u_char_type
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
                res += type_map.get_asm_var(t.get_name());
            } else {
                throw logic_error("get_asm_type " + stringify(t) + " fail");
            }
            return res;
        }

        string define_struct(t_type type) {
            assert(is_struct_type(type));
            auto res = make_new_id();
            auto& id = type.get_name();
            if (type_map.map.count(id) != 0) {
                if (not type_map.map[id].type.is_complete()) {
                    type_map.map[id].type = type;
                    res = type_map.map[id].asm_var;
                }
            } else {
                type_map.map[id] = {type, res};
            }
            auto& mm = type.get_members();
            if (not mm.empty()) {
                string asm_type_def;
                asm_type_def += res + " = type { ";
                auto start = true;
                for (auto& m : mm) {
                    if (not start) {
                        asm_type_def += ", ";
                    }
                    start = false;
                    if (is_struct_type(m.type) and m.type.is_complete()) {
                        asm_type_def += define_struct(m.type);
                    } else {
                        asm_type_def += get_asm_type(m.type);
                    }
                }
                asm_type_def += " }\n";
                asm_type_defs += asm_type_def;
            }
            return res;
        }

        t_type complete_type(const t_type& t) const {
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

        auto define_var(const string& id, t_type type) {
            type = complete_type(type);
            var_map.map[id] = {type, make_new_id()};
            func_var_alloc += func_line(var_map.map[id].asm_var
                                        + " = alloca "
                                        + get_asm_type(type));
        }

        auto can_define_type(const string& id) const {
            return type_map.can_define(id);
        }

        auto can_define_var(const string& id) const {
            return var_map.can_define(id);
        }

        auto has_type(const string& id) const {
            return type_map.has(id);
        }

        auto has_var(const string& id) const {
            return var_map.has(id);
        }

        auto get_var_asm_var(const string& id) const {
            return var_map.get_asm_var(id);
        }

        auto get_var_type(const string& id) const {
            return var_map.get_type(id);
        }

        auto get_type_asm_var(const string& id) const {
            return type_map.get_asm_var(id);
        }

        auto get_type(const string& id) const {
            return type_map.get_type(id);
        }

        auto make_asm_arg(const t_exp_value& v) const {
            auto at = get_asm_type(v.type);
            if (v.is_lvalue) {
                at += "*";
            }
            return at + " " + v.value;
        }
    };

    t_exp_value gen_sub(t_exp_value x, t_exp_value y, const t_ctx& ctx);
    t_exp_value gen_neg(const t_exp_value& x, const t_ctx& ctx);

    auto put_label(const string& l, bool f = true) {
        if (f) {
            a("br label", l);
        }
        func_body += l.substr(1) + ":\n";
    }

    string make_new_id() {
        static int aux_var_cnt = 0;
        aux_var_cnt++;
        return string("%_") + to_string(aux_var_cnt);
    }

    auto& unqualify(auto& x) {
        return x;
    }

    auto is_modifiable_lvalue(auto& x) {
        return true;
    }

    auto is_object_type(auto& x) {
        return true;
    }

    auto noop() {
        let(make_new_id(), fun("add i1", "0", "0"));
    }

    class t_conversion_error : public runtime_error {
    public:
        t_conversion_error(const t_type& a, const t_type& b)
            : runtime_error("cannot convert " + stringify(a)
                            + " to " + stringify(b))
        {}
    };

    t_type make_base_type(const t_ast&, const t_ctx&);
    string unpack_declarator(t_type& type, const t_ast& t);

    auto make_type(const t_ast& ast, const t_ctx& ctx) {
        cout << "make_type " << ast[0].uu << "\n";
        auto res = make_base_type(ast[0], ctx);
        if (ast.children.size() == 2) {
            unpack_declarator(res, ast[1]);
        }
        return res;
    }

    auto gen_conversion(const t_type& t, const t_exp_value& v,
                        const t_ctx& ctx) {
        t_exp_value res;
        if (t == int_type and v.type == bool_type) {
            res.type = t;
            res.value = make_new_id();
            let(res, ("zext " + ctx.make_asm_arg(v) + " to "
                      + ctx.get_asm_type(t)));
            return res;
        }
        if (t == void_type) {
            res.type = t;
            return res;
        }
        if (not is_scalar_type(t) or not is_scalar_type(v.type)) {
            throw t_conversion_error(v.type, t);
        }
        if (not compatible(t, v.type)) {
            res.type = t;
            res.value = make_new_id();
            string op;
            if (is_integral_type(t) and is_integral_type(v.type)) {
                if (t.get_size() < v.type.get_size()) {
                    op = "trunc";
                } else if (t.get_size() > v.type.get_size()) {
                    if (is_signed_integer_type(v.type)) {
                        op = "sext";
                    } else {
                        op = "zext";
                    }
                } else {
                    return v;
                }
            } else if (is_pointer_type(t) and is_integral_type(v.type)) {
                op = "inttoptr";
            } else if (is_pointer_type(t) and is_pointer_type(v.type)) {
                op = "bitcast";
            } else if (is_integral_type(t) and is_pointer_type(v.type)) {
                op = "ptrtoint";
            } else if (is_floating_type(t) and is_floating_type(v.type)) {
                if (t.get_size() < v.type.get_size()) {
                    op = "fptrunc";
                } else if (t.get_size() > v.type.get_size()) {
                    op = "fpext";
                } else {
                    return v;
                }
            } else if (is_floating_type(t) and is_integral_type(v.type)) {
                if (is_signed_integer_type(v.type)) {
                    op = "sitofp";
                } else {
                    op = "uitofp";
                }
            } else if (is_integral_type(t) and is_floating_type(v.type)) {
                if (is_signed_integer_type(v.type)) {
                    op = "fptosi";
                } else {
                    op = "fptoui";
                }
            } else {
                throw t_conversion_error(v.type, t);
            }
            let(res, (op + " " + ctx.make_asm_arg(v) + " to "
                      + ctx.get_asm_type(t)));
        } else {
            res = v;
        }
        return res;
    }

    void gen_int_promotion(t_exp_value& x, const t_ctx& ctx) {
        if (x.type == char_type or x.type == s_char_type
            or x.type == u_char_type or x.type == short_type
            or x.type == u_short_type) {
            x = gen_conversion(int_type, x, ctx);
        }
    }

    void gen_arithmetic_conversions(t_exp_value& x, t_exp_value& y,
                                    const t_ctx& ctx) {
        auto promote = [&](auto& t) {
            if (y.type == t) {
                x = gen_conversion(t, x, ctx);
                return;
            }
            if (x.type == t) {
                y = gen_conversion(t, y, ctx);
                return;
            }
        };
        promote(long_double_type);
        promote(double_type);
        promote(float_type);
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        promote(u_long_type);
        promote(long_type);
        promote(u_int_type);
    }

    auto gen_div(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        if (not (is_arithmetic_type(x.type) and is_arithmetic_type(y.type))) {
            throw t_bad_operands("/");
        }
        gen_arithmetic_conversions(x, y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        string op;
        if (is_floating_type(x.type)) {
            op = "fdiv";
        } else if (is_signed_integer_type(x.type)) {
            op = "sdiv";
        } else {
            op = "udiv";
        }
        let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    auto gen_mod(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands("%");
        }
        gen_arithmetic_conversions(x, y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "srem";
        } else {
            op = "urem";
        }
        let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    auto gen_shl(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands("<<");
        }
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        let(res, fun("shl " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    auto gen_shr(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands(">>");
        }
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "ashr";
        } else {
            op = "lshr";
        }
        let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    auto gen_mul(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        if (not (is_arithmetic_type(x.type) and is_arithmetic_type(y.type))) {
            throw t_bad_operands("binary *");
        }
        gen_arithmetic_conversions(x, y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "mul nsw";
        } else if (is_floating_type(x.type)) {
            op = "fmul";
        } else {
            op = "mul";
        }
        let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    auto gen_array_elt(const t_exp_value& v, int i, t_ctx& ctx) {
        t_exp_value res;
        res.value = make_new_id();
        auto len_str = to_string(v.type.get_length());
        auto elt_type = ctx.get_asm_type(v.type.get_element_type());
        auto at = string() + "[" + len_str + " x " + elt_type + "]";
        string s;
        s += res.value + " = getelementptr inbounds ";
        s += at + ", " + ctx.make_asm_arg(v);
        s += ", i64 0, i64 " + to_string(i);
        a(s);
        res.type = v.type.get_element_type();
        res.is_lvalue = true;
        return res;
    }

    auto gen_add(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        t_exp_value res;
        if (is_integral_type(x.type) and is_pointer_type(y.type)) {
            swap(x, y);
        }
        if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            res.type = x.type;
            res.value = make_new_id();
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "add nsw";
            } else if (is_floating_type(x.type)) {
                op = "fadd";
            } else {
                op = "add";
            }
            let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
        } else if (is_pointer_type(x.type) and is_integral_type(y.type)) {
            auto v = make_new_id();
            auto t = ctx.get_asm_type(x.type.get_pointee_type());
            y = gen_conversion(u_long_type, y, ctx);
            a(v + " = getelementptr inbounds " + t
              + ", " + ctx.make_asm_arg(x) + ", " + ctx.make_asm_arg(y));
            res.value = v;
            res.type = x.type;
        } else {
            throw t_bad_operands("binary +");
        }
        return res;
    }

    t_exp_value gen_neg(const t_exp_value& x, const t_ctx& ctx) {
        return gen_sub({"0", int_type}, x, ctx);
    }

    t_exp_value gen_sub(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        t_exp_value res;
        if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            res.type = x.type;
            res.value = make_new_id();
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "sub nsw";
            } else if (is_floating_type(x.type)) {
                op = "fsub";
            } else {
                op = "sub";
            }
            let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
        } else if (is_pointer_type(x.type) and is_pointer_type(y.type)
                   and compatible(unqualify(x.type.get_pointee_type()),
                                  unqualify(y.type.get_pointee_type()))) {
            res.type = long_type;
            res.value = make_new_id();
            auto s = x.type.get_pointee_type();
            x = gen_conversion(u_long_type, x, ctx);
            y = gen_conversion(u_long_type, y, ctx);
            auto z = make_new_id();
            let(z, fun("sub i64", x, y));
            let(res, fun("sdiv exact i64", z, to_string(s.get_size() / 8)));
        } else if (is_pointer_type(x.type)
                   and is_object_type(x.type.get_pointee_type())
                   and is_integral_type(y.type)) {
            res.type = x.type;
            res.value = make_new_id();
            auto t = ctx.get_asm_type(x.type.get_pointee_type());
            y = gen_conversion(u_long_type, y, ctx);
            y = gen_neg(y, ctx);
            a(res.value + " = getelementptr inbounds " + t
              + ", " + ctx.make_asm_arg(x) + ", " + ctx.make_asm_arg(y));
        } else {
            throw t_bad_operands("binary -");
        }
        return res;
    }

    auto gen_assign(const t_exp_value& lhs, const t_exp_value& rhs, t_ctx& ctx) {
        a(fun("store", ctx.make_asm_arg(rhs), ctx.make_asm_arg(lhs)));
        return lhs;
    }

    auto gen_convert_assign(const t_exp_value& lhs, const t_exp_value& rhs,
                            t_ctx& ctx) {
        return gen_assign(lhs, gen_conversion(lhs.type, rhs, ctx), ctx);
    }

    auto gen_struct_member(const t_exp_value& v, int i, t_ctx& ctx) {
        t_exp_value res;
        res.type = v.type.get_member_type(i);
        res.value = make_new_id();
        res.is_lvalue = true;
        auto vt = ctx.get_asm_type(v.type);
        a(res.value + " = getelementptr inbounds " + vt
          + ", " + vt + "* " + v.value + ", i32 0, i32 " + to_string(i));
        return res;
    }

    auto dereference(const t_exp_value& v) {
        return t_exp_value{v.value, v.type.get_pointee_type(), true};
    }

    auto convert_lvalue_to_rvalue(const t_exp_value& v, t_ctx& ctx) {
        t_exp_value res;
        res.value = make_new_id();
        res.type = v.type;
        auto at = ctx.get_asm_type(res.type);
        a(res.value + " = load " + at + ", " + at + "* " + v.value);
        res.is_lvalue = false;
        return res;
    }

    auto is_qualified_void(const t_type& t) {
        return t.get_kind() == t_type_kind::_void;
    }

    auto gen_eq_ptr_conversions(t_exp_value& x, t_exp_value& y,
                                const t_ctx& ctx) {
        if (is_pointer_type(x.type) and is_pointer_type(y.type)) {
            if (is_qualified_void(y.type.get_pointee_type())) {
                x = gen_conversion(y.type, x, ctx);
            } else if (is_qualified_void(x.type.get_pointee_type())) {
                y = gen_conversion(x.type, y, ctx);
            }
        } else if (is_pointer_type(x.type) and is_integral_type(y.type)) {
            y = gen_conversion(x.type, y, ctx);
        } else if (is_pointer_type(y.type) and is_integral_type(x.type)) {
            x = gen_conversion(y.type, x, ctx);
        }
    }

    auto gen_cond_br(const t_exp_value& v, const string& a1,
                     const string& a2) {
        a("br i1 " + v.value + ", label " + a1 + ", label " + a2);
    }

    auto gen_br(const string& l) {
        a("br label", l);
    }

    auto gen_eq(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        auto res = t_exp_value{make_new_id(), bool_type};
        if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_floating_type(x.type)) {
                op = "fcmp oeq";
            } else {
                op = "icmp eq";
            }
            let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
        } else if (is_pointer_type(x.type) or is_pointer_type(y.type)) {
            gen_eq_ptr_conversions(x, y, ctx);
            let(res, fun("icmp eq " + ctx.get_asm_type(x.type), x, y));
        } else {
            throw t_bad_operands("==");
        }
        return res;
    }

    auto gen_is_zero(const t_exp_value& x, const t_ctx& ctx) {
        return gen_eq(x, {"0", int_type}, ctx);
    }

    auto gen_is_nonzero(const t_exp_value& x, const t_ctx& ctx) {
        auto y = gen_eq(x, {"0", int_type}, ctx);
        auto res = t_exp_value{make_new_id(), bool_type};
        let(res, fun("xor i1", y.value, "1"));
        return res;
    }

    auto gen_and(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        let(res, fun("and " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    auto gen_xor(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        let(res, fun("xor " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    auto gen_or(t_exp_value x, t_exp_value y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        auto res = t_exp_value{make_new_id(), x.type};
        let(res, fun("or " + ctx.get_asm_type(x.type), x, y));
        return res;
    }

    t_exp_value gen_exp(const t_ast& ast, t_ctx& ctx,
                        bool convert_lvalue = true) {
        auto assign_op = [&](auto& op) {
            auto x = gen_exp(ast[0], ctx, false);
            auto y = gen_exp(ast[1], ctx);
            auto xv = convert_lvalue_to_rvalue(x, ctx);
            auto z = op(xv, y, ctx);
            return gen_convert_assign(x, z, ctx);
        };
        t_exp_value res;
        if (ast.uu == "un_op") {
            if (ast.vv == "+") {
                res = gen_exp(ast[0], ctx);
                if (not is_arithmetic_type(res.type)) {
                    throw t_bad_operands("unary +");
                }
                gen_int_promotion(res, ctx);
            } else if (ast.vv == "&") {
                res = gen_exp(ast[0], ctx, false);
                if (not res.is_lvalue) {
                    throw t_bad_operands("unary &");
                }
                res.type = make_pointer_type(res.type);
                res.is_lvalue = false;
            } else if (ast.vv == "*") {
                auto e = gen_exp(ast[0], ctx);
                if (not is_pointer_type(e.type)) {
                    throw t_bad_operands("unary *");
                }
                res = dereference(e);
            } else if (ast.vv == "-") {
                auto e = gen_exp(ast[0], ctx);
                if (not is_arithmetic_type(e.type)) {
                    throw t_bad_operands("unary -");
                }
                res = gen_neg(e, ctx);
            } else if (ast.vv == "!") {
                auto e = gen_exp(ast[0], ctx);
                if (not is_scalar_type(e.type)) {
                    throw t_bad_operands("!");
                }
                auto z = gen_eq(e, {"0", int_type}, ctx);
                res = gen_conversion(int_type, z, ctx);
            } else if (ast.vv == "~") {
                auto x = gen_exp(ast[0], ctx);
                if (not is_integral_type(x.type)) {
                    throw t_bad_operands("!");
                }
                gen_int_promotion(x, ctx);
                res.value = make_new_id();
                res.type = x.type;
                let(res, fun("xor " + ctx.get_asm_type(x.type), "-1", x.value));
                res.type = int_type;
            } else {
                throw logic_error("unhandled unary operator " + ast.vv);
            }
        } else if (ast.uu == "integer_constant") {
            res.value = ast.vv;
            res.type = int_type;
        } else if (ast.uu == "floating_constant") {
            res.value = ast.vv;
            res.type = double_type;
        } else if (ast.uu == "string_literal") {
            res.value = add_string_literal(ast.vv);
            res.type = make_array_type(char_type, ast.vv.length() + 1);
            res.is_lvalue = true;
        } else if (ast.uu == "identifier") {
            auto& id = ast.vv;
            res.value += ctx.get_var_asm_var(id);
            res.type = ctx.get_var_type(id);
            res.is_lvalue = true;
        } else if (ast.uu == "bin_op") {
            if (ast.vv == "=") {
                res = gen_convert_assign(gen_exp(ast[0], ctx, false),
                                         gen_exp(ast[1], ctx),
                                         ctx);
            } else {
                if (ast.vv == "+") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    res = gen_add(x, y, ctx);
                } else if (ast.vv == "-") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    res = gen_sub(x, y, ctx);
                } else if (ast.vv == "*") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    res = gen_mul(x, y, ctx);
                } else if (ast.vv == "/") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    res = gen_div(x, y, ctx);
                } else if (ast.vv == "%") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    res = gen_mod(x, y, ctx);
                } else if (ast.vv == "<<") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    res = gen_shl(x, y, ctx);
                } else if (ast.vv == ">>") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    res = gen_shr(x, y, ctx);
                } else if (ast.vv == "<=") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (is_arithmetic_type(x.type)
                        and is_arithmetic_type(y.type)) {
                        gen_arithmetic_conversions(x, y, ctx);
                        string op;
                        if (is_signed_integer_type(x.type)) {
                            op = "icmp sle";
                        } else if (is_floating_type(x.type)) {
                            op = "fcmp ole";
                        } else {
                            op = "icmp ule";
                        }
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun(op + " " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else if (is_pointer_type(x.type)
                               and is_pointer_type(y.type)
                               and compatible(unqualify(x.type),
                                              unqualify(y.type))) {
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun("icmp ule " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else {
                        throw t_bad_operands("<=");
                    }
                } else if (ast.vv == "<") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (is_arithmetic_type(x.type)
                        and is_arithmetic_type(y.type)) {
                        gen_arithmetic_conversions(x, y, ctx);
                        string op;
                        if (is_signed_integer_type(x.type)) {
                            op = "icmp slt";
                        } else if (is_floating_type(x.type)) {
                            op = "fcmp olt";
                        } else {
                            op = "icmp ult";
                        }
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun(op + " " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else if (is_pointer_type(x.type)
                               and is_pointer_type(y.type)
                               and compatible(unqualify(x.type),
                                              unqualify(y.type))) {
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun("icmp ult " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else {
                        throw t_bad_operands("<");
                    }
                } else if (ast.vv == ">") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (is_arithmetic_type(x.type)
                        and is_arithmetic_type(y.type)) {
                        gen_arithmetic_conversions(x, y, ctx);
                        string op;
                        if (is_signed_integer_type(x.type)) {
                            op = "icmp sgt";
                        } else if (is_floating_type(x.type)) {
                            op = "fcmp ogt";
                        } else {
                            op = "icmp ugt";
                        }
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun(op + " " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else if (is_pointer_type(x.type)
                               and is_pointer_type(y.type)
                               and compatible(unqualify(x.type),
                                              unqualify(y.type))) {
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun("icmp ugt " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else {
                        throw t_bad_operands(">");
                    }
                } else if (ast.vv == ">=") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (is_arithmetic_type(x.type)
                        and is_arithmetic_type(y.type)) {
                        gen_arithmetic_conversions(x, y, ctx);
                        string op;
                        if (is_signed_integer_type(x.type)) {
                            op = "icmp sge";
                        } else if (is_floating_type(x.type)) {
                            op = "fcmp oge";
                        } else {
                            op = "icmp uge";
                        }
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun(op + " " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else if (is_pointer_type(x.type)
                               and is_pointer_type(y.type)
                               and compatible(unqualify(x.type),
                                              unqualify(y.type))) {
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun("icmp uge " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else {
                        throw t_bad_operands(">=");
                    }
                } else if (ast.vv == "==") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    auto tmp = gen_eq(x, y, ctx);
                    res = gen_conversion(int_type, tmp, ctx);
                } else if (ast.vv == "!=") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (is_arithmetic_type(x.type)
                        and is_arithmetic_type(y.type)) {
                        gen_arithmetic_conversions(x, y, ctx);
                        string op;
                        if (is_floating_type(x.type)) {
                            op = "fcmp one";
                        } else {
                            op = "icmp ne";
                        }
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun(op + " " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else if (is_pointer_type(x.type)
                               or is_pointer_type(y.type)) {
                        gen_eq_ptr_conversions(x, y, ctx);
                        auto tmp = t_exp_value{make_new_id(), bool_type};
                        let(tmp,
                            fun("icmp eq " + ctx.get_asm_type(x.type), x, y));
                        res = gen_conversion(int_type, tmp, ctx);
                    } else {
                        throw t_bad_operands("!=");
                    }
                } else if (ast.vv == "&") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (not (is_integral_type(x.type)
                             and is_integral_type(y.type))) {
                        throw t_bad_operands("binary &");
                    }
                    gen_arithmetic_conversions(x, y, ctx);
                    res.value = make_new_id();
                    res.type = x.type;
                    let(res, fun("and " + ctx.get_asm_type(x.type), x, y));
                } else if (ast.vv == "^") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (not (is_integral_type(x.type)
                             and is_integral_type(y.type))) {
                        throw t_bad_operands("^");
                    }
                    gen_arithmetic_conversions(x, y, ctx);
                    res.value = make_new_id();
                    res.type = x.type;
                    let(res, fun("xor " + ctx.get_asm_type(x.type), x, y));
                } else if (ast.vv == "|") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    if (not (is_integral_type(x.type)
                             and is_integral_type(y.type))) {
                        throw t_bad_operands("|");
                    }
                    gen_arithmetic_conversions(x, y, ctx);
                    res.value = make_new_id();
                    res.type = x.type;
                    let(res, fun("or " + ctx.get_asm_type(x.type), x, y));
                } else if (ast.vv == "&&") {
                    auto l0 = make_label();
                    auto l1 = make_label();
                    auto l2 = make_label();
                    auto l3 = make_label();
                    auto x = gen_exp(ast[0], ctx);
                    put_label(l0);
                    gen_cond_br(gen_is_zero(x, ctx), l1, l2);
                    put_label(l2, false);
                    auto y = gen_exp(ast[1], ctx);
                    if (not (is_scalar_type(x.type)
                             and is_scalar_type(y.type))) {
                        throw t_bad_operands("|");
                    }
                    auto z = gen_is_nonzero(y, ctx);
                    put_label(l3);
                    put_label(l1);
                    auto tmp = t_exp_value{make_new_id(), bool_type};
                    let(tmp, ("phi i1 [ false, " + l0 + " ], [ "
                              + z.value + ", " + l3 + " ]"));
                    res = gen_conversion(int_type, tmp, ctx);
                } else if (ast.vv == "||") {
                    auto l0 = make_label();
                    auto l1 = make_label();
                    auto l2 = make_label();
                    auto l3 = make_label();
                    auto x = gen_exp(ast[0], ctx);
                    put_label(l0);
                    gen_cond_br(gen_is_zero(x, ctx), l2, l1);
                    put_label(l2, false);
                    auto y = gen_exp(ast[1], ctx);
                    if (not (is_scalar_type(x.type)
                             and is_scalar_type(y.type))) {
                        throw t_bad_operands("|");
                    }
                    auto z = gen_is_nonzero(y, ctx);
                    put_label(l3);
                    put_label(l1);
                    auto tmp = t_exp_value{make_new_id(), bool_type};
                    let(tmp, ("phi i1 [ true, " + l0 + " ], [ "
                              + z.value + ", " + l3 + " ]"));
                    res = gen_conversion(int_type, tmp, ctx);
                } else if (ast.vv == "*=") {
                    res = assign_op(gen_mul);
                } else if (ast.vv == "/=") {
                    res = assign_op(gen_div);
                } else if (ast.vv == "%=") {
                    res = assign_op(gen_mod);
                } else if (ast.vv == "+=") {
                    res = assign_op(gen_add);
                } else if (ast.vv == "-=") {
                    res = assign_op(gen_sub);
                } else if (ast.vv == "<<=") {
                    res = assign_op(gen_shl);
                } else if (ast.vv == ">>=") {
                    res = assign_op(gen_shr);
                } else if (ast.vv == "&=") {
                    res = assign_op(gen_and);
                } else if (ast.vv == "^=") {
                    res = assign_op(gen_xor);
                } else if (ast.vv == "|=") {
                    res = assign_op(gen_or);
                } else if (ast.vv == ",") {
                    gen_exp(ast[0], ctx);
                    res = gen_exp(ast[1], ctx);
                } else {
                    throw logic_error("unhandled operator " + ast.vv);
                }
            }
        } else if (ast.uu == "function_call") {
            if (ast[0] == t_ast("identifier", "printf")) {
                res.type = int_type;
                res.value = make_new_id();
                string args_str;
                for (auto i = size_t(1); i < ast.children.size(); i++) {
                    auto val = gen_exp(ast[i], ctx);
                    if (val.type == float_type) {
                        val = gen_conversion(double_type, val, ctx);
                    }
                    auto arg = ctx.get_asm_type(val.type) + " " + val.value;
                    if (args_str.empty()) {
                        args_str += arg;
                    } else {
                        args_str += ", " + arg;
                    }
                }
                a(res.value
                  + " = call i32 (i8*, ...) @printf(" + args_str + ")");
            }
        } else if (ast.uu == "struct_member") {
            auto x = gen_exp(ast[0], ctx, false);
            auto member_id = ast[1].vv;
            if (not is_struct_type(x.type)) {
                throw t_bad_operands(".");
            }
            auto struct_name = x.type.get_name();
            auto t = ctx.get_type(struct_name);
            assert(t.is_complete());
            auto idx = t.get_member_index(member_id);
            if (idx == -1) {
                throw t_bad_operands(".");
            }
            if (x.is_lvalue) {
                res.is_lvalue = true;
            }
            res.type = t.get_member_type(idx);
            res.value = make_new_id();
            auto at = ctx.get_type_asm_var(struct_name);
            a(res.value + " = getelementptr inbounds " + at
              + ", " + at + "* " + x.value + ", i32 0, i32 " + to_string(idx));
        } else if (ast.uu == "array_subscript") {
            try {
                auto z = gen_add(gen_exp(ast[0], ctx),
                                 gen_exp(ast[1], ctx), ctx);
                res = dereference(z);
            } catch (const t_bad_operands& e) {
                throw t_bad_operands("[]");
            }
        } else if (ast.uu == "postfix_increment") {
            auto e = gen_exp(ast[0], ctx, false);
            res = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(res.type))
                or not is_modifiable_lvalue(res.type)) {
                throw t_bad_operands("postfix ++");
            }
            auto z = gen_add(res, {"1", int_type}, ctx);
            gen_assign(e, z, ctx);
        } else if (ast.uu == "postfix_decrement") {
            auto e = gen_exp(ast[0], ctx, false);
            res = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(res.type))
                or not is_modifiable_lvalue(res.type)) {
                throw t_bad_operands("postfix --");
            }
            auto z = gen_sub(res, {"1", int_type}, ctx);
            gen_assign(e, z, ctx);
        } else if (ast.uu == "prefix_increment") {
            auto e = gen_exp(ast[0], ctx, false);
            auto z = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(z.type))
                or not is_modifiable_lvalue(z.type)) {
                throw t_bad_operands("prefix ++");
            }
            res = gen_add(z, {"1", int_type}, ctx);
            gen_assign(e, res, ctx);
        } else if (ast.uu == "prefix_decrement") {
            auto e = gen_exp(ast[0], ctx, false);
            auto z = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(z.type))
                or not is_modifiable_lvalue(z.type)) {
                throw t_bad_operands("prefix --");
            }
            res = gen_sub(z, {"1", int_type}, ctx);
            gen_assign(e, res, ctx);
        } else if (ast.uu == "cast") {
            auto e = gen_exp(ast[1], ctx);
            auto t = make_type(ast[0], ctx);
            res = gen_conversion(t, e, ctx);
        } else {
            throw logic_error("unhandled operator " + ast.vv);
        }
        if (convert_lvalue and res.is_lvalue) {
            if (is_array_type(res.type)) {
                res = gen_array_elt(res, 0, ctx);
                res.type = make_pointer_type(res.type);
                res.is_lvalue = false;
            } else {
                res = convert_lvalue_to_rvalue(res, ctx);
            }
        }
        return res;
    }

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

    void gen_init(const t_exp_value& v, const t_ast& ini, t_ctx& ctx) {
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
                auto v = t_exp_value{ctx.get_var_asm_var(id), type, true};
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
            auto cmp_res = make_new_id();
            gen_cond_br(gen_is_zero(cond_val, ctx), cond_false, cond_true);
            put_label(cond_true, false);
            gen_statement(c.children[1], ctx);
            gen_br(end);
            put_label(cond_false, false);
            if (c.children.size() == 3) {
                gen_statement(c.children[2], ctx);
            } else {
                noop();
            }
            put_label(end);
            noop();
        } else if (c.uu == "exp_statement") {
            if (not c.children.empty()) {
                gen_exp(c.children[0], ctx);
            }
        } else if (c.uu == "return") {
            auto val = gen_exp(c.children[0], ctx);
            a("ret", ctx.make_asm_arg(val));
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
            gen_cond_br(gen_is_zero(cond_val, ctx),
                        nctx.get_loop_end(), loop_body);
            put_label(loop_body);
            gen_statement(c.children[1], nctx);
            put_label(nctx.get_loop_body_end());
            gen_br(loop_begin);
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
            gen_cond_br(gen_is_zero(cond_val, ctx),
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
                gen_cond_br(gen_is_zero(cond_val, ctx),
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
            gen_br(loop_begin);
            put_label(nctx.get_loop_end());
        } else if (c.uu == "break") {
            if (ctx.get_loop_end() == "") {
                err("break not in loop", c.loc);
            }
            gen_br(ctx.get_loop_end());
        } else if (c.uu == "continue") {
            if (ctx.get_loop_body_end() == "") {
                err("continue not in loop", c.loc);
            }
            gen_br(ctx.get_loop_body_end());
        } else if (c.uu == "goto") {
            gen_br(ctx.get_asm_label(c[0]));
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
            noop();
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
        asm_funcs += "define i32 @"; asm_funcs += func_name;
        asm_funcs += "() {\n";
        t_ctx ctx;
        ctx.set_func_name(func_name);
        add_labels(ast, ctx);
        func_body = "";
        func_var_alloc = "";
        for (auto& c : ast.children) {
            gen_block_item(c, ctx);
        }
        a("ret i32 0");
        asm_funcs += func_var_alloc;
        asm_funcs += func_body;
        asm_funcs += "}\n";
    }
}

string gen_asm(const t_ast& ast) {
    string res;
    for (auto& c : ast.children) {
        gen_function(c);
    }
    res += asm_type_defs;
    res += "\n";
    res += global_storage;
    res += "\n";
    res += asm_funcs;
    res += "\n";
    res += "declare i32 @printf(i8* nocapture readonly, ...)\n";
    return res;
}
