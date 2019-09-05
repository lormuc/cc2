#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cctype>
#include <cassert>

#include "asm_gen.hpp"
#include "type.hpp"
#include "misc.hpp"

using namespace std;

namespace {
    string asm_funcs;
    t_ast ast;
    string global_storage;
    string asm_type_defs;

    string make_new_id();
    ull eval_const_exp(const t_ast&);

    struct t_exp_value {
        string value;
        t_type type;
        bool is_lvalue = false;
    };

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

    auto a(const string& s) {
        asm_funcs += "    "; asm_funcs += s; asm_funcs += "\n";
    }

    auto a(const string& s0, const string& s1) {
        asm_funcs += "    "; asm_funcs += s0; asm_funcs += " ";
        asm_funcs += s1; asm_funcs += "\n";
    }

    auto a(const string& x, const string& y, const string& z) {
        asm_funcs += "    "; asm_funcs += x; asm_funcs += " ";
        asm_funcs += y; asm_funcs += ", "; asm_funcs += z;
        asm_funcs += "\n";
    }

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
                    throw runtime_error(string("unknown id <") + id + ">");
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
                    throw runtime_error(string("unknown id <") + id + ">");
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
    public:
        auto set_loop_body_end(const string& x) {
            loop_body_end = x;
        }

        auto get_loop_body_end() {
            return loop_body_end;
        }

        auto set_loop_end(const string& x) {
            loop_end = x;
        }

        auto get_loop_end() {
            return loop_end;
        }

        auto set_func_name(const string& x) {
            func_name = x;
        }

        auto get_func_name() {
            return func_name;
        }

        string get_asm_type(const t_type& t) {
            string res;
            if (t == unsigned_long_type) {
                res = "i64";
            } else if (t == char_type) {
                res = "i8";
            } else if (t == int_type) {
                res = "i32";
            } else if (t == float_type) {
                res = "float";
            } else if (t == double_type) {
                res = "double";
            } else if (t == void_pointer_type) {
                res += "i8*";
            } else if (is_pointer_type(t)) {
                res += get_asm_type(pointer_get_referenced_type(t));
                res += "*";
            } else if (is_array_type(t)) {
                res += "[";
                res += to_string(array_get_length(t)) + " x ";
                res += get_asm_type(array_get_element_type(t));
                res += "]";
            } else if (is_struct_type(t)) {
                res += type_map.get_asm_var(struct_get_name(t));
            }
            return res;
        }

        string define_struct(t_type type) {
            assert(is_struct_type(type));
            auto res = make_new_id();
            auto& id = type.vv;
            if (type_map.map.count(id) != 0) {
                if (not struct_is_complete(type_map.map[id].type)) {
                    type_map.map[id].type = type;
                    res = type_map.map[id].asm_var;
                } else if (struct_is_complete(type)) {
                    throw runtime_error("struct redefinition");
                }
            } else {
                type_map.map[id] = {type, res};
            }
            auto mm = struct_get_members(type);
            if (not mm.empty()) {
                string asm_type_def;
                asm_type_def += res + " = type { ";
                auto start = true;
                for (auto& m : mm) {
                    if (not start) {
                        asm_type_def += ", ";
                    }
                    start = false;
                    if (is_struct_type(m.type) and struct_is_complete(m.type)) {
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

        auto complete_type(t_type& t) const {
            if (not is_struct_type(t)) {
                return;
            }
            if (not struct_is_complete(t)) {
                t = type_map.get_type(t.vv);
            }
            for (auto& m : t.children) {
                complete_type(m[0]);
            }
        }

        auto define_var(const string& id, t_type type) {
            complete_type(type);
            var_map.map[id] = {type, make_new_id()};
            a(var_map.map[id].asm_var + " = alloca " + get_asm_type(type));
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

        auto make_asm_arg(const t_exp_value& v) {
            auto at = get_asm_type(v.type);
            if (v.is_lvalue) {
                at += "*";
            }
            return at + " " + v.value;
        }
    };

    auto put_label(const string& l, bool f = true) {
        if (f) {
            a("br label", l);
        }
        asm_funcs += l.substr(1) + ":\n";
    }

    unsigned get_size(const t_ast& type) {
        if (type.uu == "array") {
            auto arr_size_exp =  type.children[1];
            // if (arr_size_exp.uu != "constant") {
            //     throw_ln("bad array size");
            // }
            auto arr_size = unsigned(stoul(arr_size_exp.vv));
            return get_size(type.children[0]) * arr_size;
        } else {
            return 8u;
        }
    }

    string make_new_id() {
        static ull aux_var_cnt = 0;
        aux_var_cnt++;
        return string("%_") + to_string(aux_var_cnt);
    }

    void noop() {
        asm_funcs += "    ";
        asm_funcs += make_new_id() + " = add i1 0, 0\n";
    }

    auto gen_conversion(const t_type& t, const t_exp_value& v, t_ctx& ctx) {
        t_exp_value res;
        if (not equal(t, v.type)) {
            res.type = t;
            res.value = make_new_id();
            if (is_integral_type(v.type) and is_pointer_type(t)) {
                a(res.value + " = inttoptr " + ctx.make_asm_arg(v)
                  + " to " + ctx.get_asm_type(t));
            } else if (is_pointer_type(t) and is_pointer_type(v.type)) {
                a(res.value + " = bitcast " + ctx.make_asm_arg(v)
                  + " to " + ctx.get_asm_type(t));
            } else if (t == double_type and v.type == float_type) {
                a(res.value + " = fpext " + ctx.make_asm_arg(v)
                  + " to double");
            } else if (t == float_type and v.type == double_type) {
                a(res.value + " = fptrunc " + ctx.make_asm_arg(v)
                  + " to float");
            } else if (t == float_type and v.type == int_type) {
                a(res.value + " = sitofp " + ctx.make_asm_arg(v)
                  + " to float");
            } else if (t == double_type and v.type == int_type) {
                a(res.value + " = sitofp " + ctx.make_asm_arg(v)
                  + " to double");
            } else if (t == unsigned_long_type and v.type == int_type) {
                a(res.value + " = sext " + ctx.make_asm_arg(v)
                  + " to i64");
            } else {
                throw runtime_error("unknown conversion");
            }
        } else {
            res = v;
        }
        return res;
    }

    void gen_arithmetic_conversions(t_exp_value& x, t_exp_value& y,
                                    t_ctx& ctx) {
        auto promote = [&](auto& ta, auto& tb) {
            if (x.type == ta and y.type == tb) {
                x = gen_conversion(tb, x, ctx);
            } else if (x.type == tb and y.type == ta) {
                y = gen_conversion(tb, y, ctx);
            }
        };
        promote(float_type, double_type);
        promote(int_type, float_type);
        promote(int_type, double_type);
    }

    auto gen_add(const t_exp_value& x, const t_exp_value& y) {
        t_exp_value res;
        res.type = x.type;
        res.value = make_new_id();
        if (x.type == int_type) {
            a(res.value + " = add nsw i32 " + x.value + ", " + y.value);
        } else if (x.type == double_type) {
            a(res.value + " = fadd double " + x.value + ", " + y.value);
        } else if (x.type == float_type) {
            a(res.value + " = fadd float " + x.value + ", " + y.value);
        }
        return res;
    }

    auto gen_div(const t_exp_value& x, const t_exp_value& y) {
        t_exp_value res;
        res.type = x.type;
        res.value = make_new_id();
        if (x.type == int_type) {
            a(res.value + " = sdiv i32 " + x.value + ", " + y.value);
        } else if (x.type == double_type) {
            a(res.value + " = fdiv double " + x.value + ", " + y.value);
        } else if (x.type == float_type) {
            a(res.value + " = fdiv float " + x.value + ", " + y.value);
        }
        return res;
    }

    auto gen_mul(const t_exp_value& x, const t_exp_value& y) {
        t_exp_value res;
        res.type = x.type;
        res.value = make_new_id();
        if (x.type == int_type) {
            a(res.value + " = mul nsw i32 " + x.value + ", " + y.value);
        } else if (x.type == double_type) {
            a(res.value + " = fmul double " + x.value + ", " + y.value);
        } else if (x.type == float_type) {
            a(res.value + " = fmul float " + x.value + ", " + y.value);
        }
        return res;
    }

    auto gen_sub(const t_exp_value& x, const t_exp_value& y) {
        t_exp_value res;
        res.type = x.type;
        res.value = make_new_id();
        if (x.type == int_type) {
            a(res.value + " = sub nsw i32 " + x.value + ", " + y.value);
        } else if (x.type == double_type) {
            a(res.value + " = fsub double " + x.value + ", " + y.value);
        } else if (x.type == float_type) {
            a(res.value + " = fsub float " + x.value + ", " + y.value);
        }
        return res;
    }

    auto gen_lte(const t_exp_value& x, const t_exp_value& y) {
        t_exp_value res;
        res.type = x.type;
        res.value = make_new_id();
        if (x.type == int_type) {
            auto tmp = make_new_id();
            a(tmp + " = icmp sle i32 " + x.value + ", " + y.value);
            a(res.value + " = zext i1 " + tmp + " to i32");
        } else if (x.type == double_type) {
            auto tmp = make_new_id();
            a(tmp + " = fcmp ole double " + x.value + ", " + y.value);
            a(res.value + " = zext i1 " + tmp + " to i32");
        } else if (x.type == float_type) {
            auto tmp = make_new_id();
            a(tmp + " = fcmp ole float " + x.value + ", " + y.value);
            a(res.value + " = zext i1 " + tmp + " to i32");
        }
        return res;
    }

    auto gen_slt(const t_exp_value& x, const t_exp_value& y) {
        t_exp_value res;
        res.type = x.type;
        res.value = make_new_id();
        if (x.type == int_type) {
            auto tmp = make_new_id();
            a(tmp + " = icmp slt i32 " + x.value + ", " + y.value);
            a(res.value + " = zext i1 " + tmp + " to i32");
        } else if (x.type == double_type) {
            auto tmp = make_new_id();
            a(tmp + " = fcmp olt double " + x.value + ", " + y.value);
            a(res.value + " = zext i1 " + tmp + " to i32");
        } else if (x.type == float_type) {
            auto tmp = make_new_id();
            a(tmp + " = fcmp olt float " + x.value + ", " + y.value);
            a(res.value + " = zext i1 " + tmp + " to i32");
        }
        return res;
    }

    auto gen_elt_adr(const t_exp_value& v, ull i, t_ctx& ctx) {
        t_exp_value res;
        res.value = make_new_id();
        auto len_str = to_string(array_get_length(v.type));
        auto elt_type = ctx.get_asm_type(array_get_element_type(v.type));
        auto at = string() + "[" + len_str + " x " + elt_type + "]";
        string s;
        s += res.value + " = getelementptr inbounds ";
        s += at + ", " + ctx.make_asm_arg(v);
        s += ", i64 " + to_string(i) + ", i64 0";
        a(s);
        res.type = make_pointer_type(array_get_element_type(v.type));
        return res;
    }

    auto add(t_exp_value x, t_exp_value y, t_ctx& ctx) {
        t_exp_value res;
        if (is_integral_type(x.type) and is_pointer_type(y.type)) {
            swap(x, y);
        }
        if (is_arithmetic_type(x.type)
            and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            res = gen_add(x, y);
        } else if (is_pointer_type(x.type)
                   and is_integral_type(y.type)) {
            auto v = make_new_id();
            auto t = ctx.get_asm_type(pointer_get_referenced_type(x.type));
            y = gen_conversion(unsigned_long_type, y, ctx);
            a(v + " =  getelementptr inbounds " + t
              + ", " + ctx.make_asm_arg(x) + ", " + ctx.make_asm_arg(y));
            res.value = v;
            res.type = x.type;
        } else {
            err("bad operand types for +", ast[0].loc);
        }
        return res;
    }

    auto dereference(const t_exp_value& v) {
        t_exp_value res;
        res = v;
        if (not is_pointer_type(res.type)) {
            err("* operand is not a pointer", ast.loc);
        }
        res.type = pointer_get_referenced_type(res.type);
        res.is_lvalue = true;
        return res;
    }

    t_exp_value gen_exp(const t_ast& ast, t_ctx& ctx,
                        bool convert_lvalue = true) {
        t_exp_value res;
        res.is_lvalue = false;
        if (ast.uu == "un_op") {
            if (ast.vv == "&") {
                res = gen_exp(ast[0], ctx, false);
                if (not res.is_lvalue) {
                    err("& operand is not an lvalue", ast.loc);
                }
                res.type = make_pointer_type(res.type);
                res.is_lvalue = false;
            } else if (ast.vv == "*") {
                res = dereference(gen_exp(ast[0], ctx));
                // auto type = gen_exp(ast.children[0], var_map);
                // if (type.uu != "pointer") {
                //     throw_ln("* operand not a pointer");
                // }
            } else if (ast.vv == "-") {
                auto x = gen_exp(ast[0], ctx);
                res.value = make_new_id();
                res.type = x.type;
                if (x.type == int_type) {
                    a(res.value + " = sub nsw i32 0, " + x.value);
                } else {
                    err("non-int operand not supported", ast.loc);
                }
            } else if (ast.vv == "!") {
                auto x = gen_exp(ast[0], ctx);
                res.value = make_new_id();
                res.type = int_type;
                if (x.type == int_type) {
                    auto tmp = make_new_id();
                    a(tmp + " = icmp eq i32 " + x.value + ", 0");
                    a(res.value + " = zext i1 " + tmp + " to i32");
                } else {
                    err("non-int operand not supported", ast.loc);
                }
            } else if (ast.vv == "~") {
                auto x = gen_exp(ast[0], ctx);
                res.value = make_new_id();
                res.type = int_type;
                if (x.type == int_type) {
                    a(res.value + " = xor i32 -1, " + x.value);
                } else {
                    err("non-int operand not supported", ast.loc);
                }
            } else {
                err("unknown unary operator", ast.loc);
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
            // auto& var_name = ast.vv;
            // if (not var_map.contains(var_name)) {
            //     throw_ln("undeclared identifier");
            // }
            // res_type = get_var_type(var_name);
            // auto adr = var_map.get_var_adr(var_name);
            // a("lea", adr, "%rax");
        } else if (ast.uu == "bin_op") {
            if (ast.vv == "=") {
                auto rhs = gen_exp(ast[1], ctx);
                auto lhs = gen_exp(ast[0], ctx, false);
                auto val = gen_conversion(lhs.type, rhs, ctx);
                a("store", ctx.make_asm_arg(val), ctx.make_asm_arg(lhs));
                res = val;
            } else {
                if (ast.vv == "+") {
                    res = add(gen_exp(ast[0], ctx), gen_exp(ast[1], ctx), ctx);
                } else if (ast.vv == "*") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    gen_arithmetic_conversions(x, y, ctx);
                    res = gen_mul(x, y);
                } else if (ast.vv == "-") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    gen_arithmetic_conversions(x, y, ctx);
                    res = gen_sub(x, y);
                } else if (ast.vv == "/") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    gen_arithmetic_conversions(x, y, ctx);
                    res = gen_div(x, y);
                } else if (ast.vv == "<=") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    gen_arithmetic_conversions(x, y, ctx);
                    res = gen_lte(x, y);
                } else if (ast.vv == "<") {
                    auto x = gen_exp(ast[0], ctx);
                    auto y = gen_exp(ast[1], ctx);
                    gen_arithmetic_conversions(x, y, ctx);
                    res = gen_slt(x, y);
                } else {
                    err("unknown binary operator", ast.loc);
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
                err("dot operand is not a struct", ast.loc);
            }
            auto struct_name = struct_get_name(x.type);
            auto t = ctx.get_type(struct_name);
            assert(struct_is_complete(t));
            auto idx = struct_get_member_idx(t, member_id);
            if (idx == size_t(-1)) {
                err("struct has no such member", ast[1].loc);
            }
            if (x.is_lvalue) {
                res.is_lvalue = true;
            }
            res.type = struct_get_member_type(t, idx);
            res.value = make_new_id();
            auto at = ctx.get_type_asm_var(struct_name);
            a(res.value + " = getelementptr inbounds " + at
              + ", " + at + "* " + x.value + ", i32 0, i32 " + to_string(idx));
        } else if (ast.uu == "array_subscript") {
            res = dereference(add(gen_exp(ast[0], ctx), gen_exp(ast[1], ctx), ctx));
        }
        if (convert_lvalue and res.is_lvalue) {
            if (res.type.uu == "array") {
                res = gen_elt_adr(res, 0, ctx);
            } else {
                auto v = make_new_id();
                res.type = res.type;
                auto at = ctx.get_asm_type(res.type);
                a(v + " = load " + at + ", " + at + "* " + res.value);
                res.value = v;
                res.is_lvalue = false;
            }
        }
        return res;
    }

    void gen_compound_statement(const t_ast&, const t_ctx&);

    namespace {
        void unpack_declarator(t_type& type, string& name, const t_ast& t) {
            auto p = &(t[0]);
            while (not (*p).children.empty()) {
                type = make_pointer_type(type);
                p = &((*p)[0]);
            }
            auto& dd = t[1];
            assert(dd.children.size() >= 1);
            auto i = dd.children.size() - 1;
            while (i != 0) {
                if (dd[i].uu == "array_size") {
                    if (dd[i].children.size() != 0) {
                        auto arr_len = eval_const_exp(dd[i][0]);
                        type = make_array_type(type, arr_len);
                    } else {
                        type = make_array_type(type);
                    }
                }
                i--;
            }
            if (dd[0].uu == "identifier") {
                name = dd[0].vv;
            } else {
                unpack_declarator(type, name, dd[0]);
            }
        }
    }

    t_type make_base_type(const t_ast& t) {
        t_type res;
        auto ts = [](auto s) {
            return t_ast("type_specifier", s);
        };
        if (t.children.size() == 1) {
            if (t[0].uu == "struct_or_union_specifier") {
                vector<t_struct_member> members;
                if (t[0].children.empty()) {
                    return make_struct_type(t[0].vv);
                }
                for (auto& c : t[0][0].children) {
                    auto base = make_base_type(c[0]);
                    for (size_t i = 1; i < c.children.size(); i++) {
                        auto type = base;
                        string id;
                        unpack_declarator(type, id, c[i][0]);
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
            } else if (t[0] == ts("int")) {
                return int_type;
            } else if (t[0] == ts("float")) {
                return float_type;
            } else if (t[0] == ts("double")) {
                return double_type;
            } else if (t[0] == ts("char")) {
                return char_type;
            } else if (t[0] == ts("void")) {
                return void_type;
            }
        }
        err("bad type ", t.loc);
    }

    ull eval_const_exp(const t_ast& t) {
        ull res;
        if (t.uu == "integer_constant") {
            res = stoull(t.vv);
        } else {
            err("only simple array size exprs are supported", t.loc);
        }
        return res;
    }

    void gen_declaration(const t_ast& ast, t_ctx& ctx) {
        auto base = make_base_type(ast[0]);
        if (is_struct_type(base)) {
            ctx.define_struct(base);
        }
        for (size_t i = 1; i < ast.children.size(); i++) {
            auto type = base;
            string id;
            unpack_declarator(type, id, ast[i][0]);
            ctx.define_var(id, type);
        }
    }

    auto gen_cond_br(const string& a0, const string& a1, const string& a2) {
        a("br i1", a0 + ", label " + a1 + ", label " + a2);
    }

    auto gen_br(const string& l) {
        a("br label", l);
    }

    void gen_statement(const t_ast& c, t_ctx& ctx) {
        if (c.uu == "if") {
            auto cond_true = make_label();
            auto cond_false = make_label();
            auto end = make_label();
            auto cond_val = gen_exp(c.children[0], ctx);
            auto cmp_res = make_new_id();
            a(cmp_res + " = icmp eq i32 0, " + cond_val.value);
            a(string("br i1 ") + cmp_res + ", label " + cond_false
              + ", label " + cond_true);
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
            auto cond_false = make_new_id();
            a(cond_false + " = icmp eq i32 0, " + cond_val.value);
            a("br i1", cond_false + ", label "
              + nctx.get_loop_end() + ", label " + loop_body);
            put_label(loop_body);
            gen_statement(c.children[1], nctx);
            put_label(nctx.get_loop_body_end());
            gen_br(loop_begin);
            put_label(nctx.get_loop_end());
        } else if (c.uu == "do_while") {
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
                auto cond_false = make_new_id();
                a(cond_false + " = icmp eq i32 0, " + cond_val.value);
                gen_cond_br(cond_false, nctx.get_loop_end(), loop_body);
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
        } else if (c.uu == "continue") {
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

    auto gen_function(const t_ast& ast) {
        auto& func_name = ast.vv;
        asm_funcs += "define i32 @"; asm_funcs += func_name;
        asm_funcs += "() {\n";
        t_ctx ctx;
        ctx.set_func_name(func_name);
        for (auto& c : ast.children) {
            gen_block_item(c, ctx);
        }
        a("ret i32 0");
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
