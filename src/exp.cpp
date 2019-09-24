#include <string>
#include <cassert>
#include <iostream>

#include "asm_gen.hpp"
#include "exp.hpp"

using namespace std;

namespace {
    t_val gen_conversion(const t_type& t, const t_val& v, const t_ctx& ctx);
    t_val gen_neg(const t_val& x, const t_ctx& ctx);
    t_val gen_eq(t_val x, t_val y, const t_ctx& ctx);

    void err(const string& str, t_loc loc) {
        throw t_compile_error(str, loc);
    }
    bool is_modifiable_lvalue(const t_val& x) {
        return true;
    }

    t_val adr(const t_val& x) {
        assert(x.is_lvalue());
        return t_val(x.asm_id(), make_pointer_type(x.type()));
    }

    bool is_null(const t_val& x) {
        return (x.is_constant()
                and ((is_integral_type(x.type()) and x.is_false())
                     or x.is_void_null()));
    }

    bool is_pointer_to_object_type(const t_type& x) {
        return true;
    }

    t_val apply(string op, const t_val& x, const t_val& y, const t_ctx& ctx) {
        auto res_id = prog.apply(op, ctx.asmv(x), ctx.asmv(y));
        return t_val(res_id, x.type());
    }

    t_type make_type(const t_ast& ast, t_ctx& ctx) {
        auto res = make_base_type(ast[0], ctx);
        if (ast.children.size() == 2) {
            unpack_declarator(res, ast[1], ctx);
        }
        return res;
    }

    bool ptrs_to_compatible(const t_val& x, const t_val& y) {
        return (is_pointer_type(x.type()) and is_pointer_type(y.type())
                and compatible(unqualify(x.type().get_pointee_type()),
                               unqualify(y.type().get_pointee_type())));
    }

    void gen_int_promotion(t_val& x, const t_ctx& ctx) {
        if (x.type() == char_type or x.type() == s_char_type
            or x.type() == u_char_type or x.type() == short_type
            or x.type() == u_short_type or is_enum_type(x.type())) {
            x = gen_conversion(int_type, x, ctx);
        }
    }

    t_val gen_conversion(const t_type& t, const t_val& v, const t_ctx& ctx) {
        if (t == void_type) {
            return t_val();
        }
        if (not is_scalar_type(t) or not is_scalar_type(v.type())) {
            throw t_conversion_error(v.type(), t);
        }
        auto res_id = v.asm_id();
        if (not compatible(t, v.type())) {
            auto x = ctx.asmv(v);
            auto w = ctx.asmt(t);
            string op;
            if (is_integral_type(t) and is_integral_type(v.type())) {
                if (t.get_size() < v.type().get_size()) {
                    op = "trunc";
                } else if (t.get_size() > v.type().get_size()) {
                    if (is_signed_type(v.type())) {
                        op = "sext";
                    } else {
                        op = "zext";
                    }
                }
            } else if (is_floating_type(t) and is_floating_type(v.type())) {
                if (t.get_size() < v.type().get_size()) {
                    op = "fptrunc";
                } else if (t.get_size() > v.type().get_size()) {
                    op = "fpext";
                }
            } else if (is_floating_type(t) and is_integral_type(v.type())) {
                if (is_signed_type(v.type())) {
                    op = "sitofp";
                } else {
                    op = "uitofp";
                }
            } else if (is_integral_type(t) and is_floating_type(v.type())) {
                if (is_signed_type(v.type())) {
                    op = "fptosi";
                } else {
                    op = "fptoui";
                }
            } else if (is_pointer_type(t) and is_integral_type(v.type())) {
                op = "inttoptr";
            } else if (is_pointer_type(t) and is_pointer_type(v.type())) {
                op = "bitcast";
            } else if (is_integral_type(t) and is_pointer_type(v.type())) {
                op = "ptrtoint";
            } else {
                throw t_conversion_error(v.type(), t);
            }
            if (op != "") {
                res_id = prog.convert(op, x, w);
            }
        }
        return t_val(res_id, t);
    }

    void gen_arithmetic_conversions(t_val& x, t_val& y, const t_ctx& ctx) {
        auto promote = [&](auto& t) {
            if (y.type() == t) {
                x = gen_conversion(t, x, ctx);
                return;
            }
            if (x.type() == t) {
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

    auto silent_gen_exp(const t_ast& ast, t_ctx& ctx, bool c = true) {
        auto o = prog.silence();
        prog.silence(true);
        auto res = gen_exp(ast, ctx, c);
        prog.silence(o);
        return res;
    }

    t_val gen_sub(t_val x, t_val y, const t_ctx& ctx) {
        if (is_arithmetic_type(x.type()) and is_arithmetic_type(y.type())) {
            gen_arithmetic_conversions(x, y, ctx);
            if (x.is_constant() and y.is_constant()) {
                return x - y;
            }
            string op;
            if (is_signed_integer_type(x.type())) {
                op = "sub nsw";
            } else if (is_floating_type(x.type())) {
                op = "fsub";
            } else {
                op = "sub";
            }
            return apply(op, x, y, ctx);
        } else if (ptrs_to_compatible(x, y)) {
            auto& w = x.type().get_pointee_type();
            x = gen_conversion(uintptr_t_type, x, ctx);
            y = gen_conversion(uintptr_t_type, y, ctx);
            auto v_id = apply("sub", x, y, ctx).asm_id();
            auto v = t_val(v_id, ptrdiff_t_type);
            return apply("sdiv exact", v, w.get_size(), ctx);
        } else if (is_pointer_type(x.type())
                   and is_object_type(x.type().get_pointee_type())
                   and is_integral_type(y.type())) {
            y = gen_conversion(uintptr_t_type, y, ctx);
            y = gen_neg(y, ctx);
            auto res_id = prog.inc_ptr(ctx.asmv(x), ctx.asmv(y));
            return t_val(res_id, x.type());
        } else {
            throw t_bad_operands();
        }
    }

    t_val convert_lvalue_to_rvalue(const t_val& v, t_ctx& ctx) {
        assert(v.is_lvalue());
        return t_val(prog.load(ctx.asmv(v)), v.type());
    }

    t_val gen_neg(const t_val& x, const t_ctx& ctx) {
        return gen_sub(0, x, ctx);
    }

    t_val dereference(const t_val& v) {
        return t_val(v.asm_id(), v.type().get_pointee_type(), true);
    }

    t_val gen_lt(t_val x, t_val y, const t_ctx& ctx) {
        if (is_arithmetic_type(x.type()) and is_arithmetic_type(y.type())) {
            gen_arithmetic_conversions(x, y, ctx);
            if (x.is_constant() and y.is_constant()) {
                return x < y;
            }
            string op;
            if (is_signed_integer_type(x.type())) {
                op = "icmp slt";
            } else if (is_floating_type(x.type())) {
                op = "fcmp olt";
            } else {
                op = "icmp ult";
            }
            auto res_id = prog.apply_rel(op, ctx.asmv(x), ctx.asmv(y));
            return t_val(res_id, int_type);
        } else if (ptrs_to_compatible(x, y)) {
            y = gen_conversion(x.type(), y, ctx);
            auto res_id = prog.apply_rel("icmp ult", ctx.asmv(x), ctx.asmv(y));
            return t_val(res_id, int_type);
        } else {
            throw t_bad_operands();
        }
    }

    auto gen_and(t_val x, t_val y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        if (not (is_integer_type(x.type()) and is_integer_type(y.type()))) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x & y;
        }
        return apply("and", x, y, ctx);
    }

    auto gen_xor(t_val x, t_val y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        if (not (is_integer_type(x.type()) and is_integer_type(y.type()))) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x ^ y;
        }
        return apply("xor", x, y, ctx);
    }

    auto gen_or(t_val x, t_val y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        if (not (is_integer_type(x.type()) and is_integer_type(y.type()))) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x | y;
        }
        return apply("or", x, y, ctx);
    }

    t_val gen_eq(t_val x, t_val y, const t_ctx& ctx) {
        if (is_arithmetic_type(x.type()) and is_arithmetic_type(y.type())) {
            gen_arithmetic_conversions(x, y, ctx);
            if (x.is_constant() and y.is_constant()) {
                return x == y;
            }
            string op;
            if (is_floating_type(x.type())) {
                op = "fcmp oeq";
            } else {
                op = "icmp eq";
            }
            auto res_id = prog.apply_rel(op, ctx.asmv(x), ctx.asmv(y));
            return t_val(res_id, int_type);
        } else if (is_pointer_type(x.type()) or is_pointer_type(y.type())) {
            if (ptrs_to_compatible(x, y)) {
                y = gen_conversion(x.type(), y, ctx);
            } else if (is_null(y) or (is_pointer_to_object_type(y.type())
                                      and unqualify(x.type()) == void_type)) {
                y = gen_conversion(x.type(), y, ctx);
            } else if (is_null(x) or (is_pointer_to_object_type(x.type())
                                      and unqualify(y.type()) == void_type)) {
                x = gen_conversion(y.type(), x, ctx);
            } else {
                throw t_bad_operands();
            }
            auto res_id = prog.apply_rel("icmp eq", ctx.asmv(x), ctx.asmv(y));
            return t_val(res_id, int_type);
        } else {
            throw t_bad_operands();
        }
    }

    t_val gen_mul(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_arithmetic_type(x.type())
                 and is_arithmetic_type(y.type()))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        if (x.is_constant() and y.is_constant()) {
            return x * y;
        }
        string op;
        if (is_signed_integer_type(x.type())) {
            op = "mul nsw";
        } else if (is_floating_type(x.type())) {
            op = "fmul";
        } else {
            op = "mul";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_mod(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type()) and is_integral_type(y.type()))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        if (x.is_constant() and y.is_constant()) {
            return x % y;
        }
        string op;
        if (is_signed_integer_type(x.type())) {
            op = "srem";
        } else {
            op = "urem";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_shr(t_val x, t_val y, const t_ctx& ctx) {
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        if (not (is_integer_type(x.type()) and is_integer_type(y.type()))) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x >> y;
        }
        string op;
        if (is_signed_integer_type(x.type())) {
            op = "ashr";
        } else {
            op = "lshr";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_shl(t_val x, t_val y, const t_ctx& ctx) {
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        if (not (is_integer_type(x.type()) and is_integer_type(y.type()))) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x << y;
        }
        return apply("shl", x, y, ctx);
    }

    t_val gen_div(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_arithmetic_type(x.type()) and is_arithmetic_type(y.type()))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        if (x.is_constant() and y.is_constant()) {
            return x / y;
        }
        string op;
        if (is_floating_type(x.type())) {
            op = "fdiv";
        } else if (is_signed_integer_type(x.type())) {
            op = "sdiv";
        } else {
            op = "udiv";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_add(t_val x, t_val y, const t_ctx& ctx) {
        if (is_integral_type(x.type()) and is_pointer_type(y.type())) {
            swap(x, y);
        }
        if (is_arithmetic_type(x.type()) and is_arithmetic_type(y.type())) {
            gen_arithmetic_conversions(x, y, ctx);
            if (x.is_constant() and y.is_constant()) {
                return x + y;
            }
            string op;
            if (is_signed_integer_type(x.type())) {
                op = "add nsw";
            } else if (is_floating_type(x.type())) {
                op = "fadd";
            } else {
                op = "add";
            }
            return apply(op, x, y, ctx);
        } else if (is_pointer_type(x.type()) and is_integral_type(y.type())) {
            y = gen_conversion(uintptr_t_type, y, ctx);
            auto res_id = prog.inc_ptr(ctx.asmv(x), ctx.asmv(y));
            return t_val(res_id, x.type());
        } else {
            throw t_bad_operands();
        }
    }

    bool can_repr(const t_type& type, unsigned long w) {
        if (type == int_type) {
            return w < (1ul << 31);
        } else if (type == u_int_type) {
            return w < (1ul << 32);
        } else if (type == long_type) {
            return w < (1ul << 63);
        } else {
            return true;
        }
    }

    t_val gen_exp_(const t_ast& ast, t_ctx& ctx,
                   bool convert_lvalue) {
        auto assign_op = [&](auto& op) {
            auto x = gen_exp(ast[0], ctx, false);
            auto y = gen_exp(ast[1], ctx);
            auto xv = convert_lvalue_to_rvalue(x, ctx);
            auto z = op(xv, y, ctx);
            return gen_convert_assign(x, z, ctx);
        };
        auto& op = ast.uu;
        auto arg_cnt = ast.children.size();
        t_val x;
        t_val y;
        t_val res;
        if (op == "integer_constant") {
            unsigned long w;
            try {
                w = stoul(ast.vv, 0, 0);
            } catch (out_of_range) {
                err("unrepresentable value", ast.loc);
            } catch (invalid_argument) {
                err("bad value", ast.loc);
            }
            auto u_suf = (ast.vv.find('u') != string::npos
                          or ast.vv.find('U') != string::npos);
            auto l_suf = (ast.vv.find('l') != string::npos
                          or ast.vv.find('L') != string::npos);
            vector<t_type> types;
            if (not u_suf and not l_suf) {
                if (ast.vv[0] == '0') {
                    types = {int_type, u_int_type, long_type, u_long_type};
                } else {
                    types = {int_type, long_type, u_long_type};
                }
            } else if (u_suf and not l_suf) {
                types = {u_int_type, u_long_type};
            } else if (not u_suf and l_suf) {
                types = {long_type, u_long_type};
            } else if (u_suf and l_suf) {
                types = {u_long_type};
            }
            for (auto& type : types) {
                if (can_repr(type, w)) {
                    res = t_val(w, type);
                    break;
                }
            }
        } else if (op == "floating_constant") {
            auto w = stod(ast.vv);
            auto suffix = ast.vv.back();
            if (suffix == 'f' or suffix == 'F') {
                res = t_val(w, float_type);
            } else if (suffix == 'l' or suffix == 'L') {
                res = t_val(w, long_double_type);
            } else {
                res = t_val(w, double_type);
            }
        } else if (op == "string_literal") {
            auto id = prog.def_str(ast.vv);
            auto t = make_array_type(char_type, ast.vv.length() + 1);
            res = t_val(id, t, true);
        } else if (op == "identifier") {
            res = ctx.get_var_data(ast.vv);
        } else if (op == "+" and arg_cnt == 1) {
            res = gen_exp(ast[0], ctx);
            if (not is_arithmetic_type(res.type())) {
                throw t_bad_operands();
            }
            gen_int_promotion(res, ctx);
        } else if (op == "&" and arg_cnt == 1) {
            auto w = gen_exp(ast[0], ctx, false);
            if (not w.is_lvalue()) {
                throw t_bad_operands();
            }
            res = adr(w);
        } else if (op == "*" and arg_cnt == 1) {
            auto e = gen_exp(ast[0], ctx);
            if (not is_pointer_type(e.type())) {
                throw t_bad_operands();
            }
            res = dereference(e);
        } else if (op == "-" and arg_cnt == 1) {
            auto e = gen_exp(ast[0], ctx);
            if (not is_arithmetic_type(e.type())) {
                throw t_bad_operands();
            }
            res = gen_neg(e, ctx);
        } else if (op == "!" and arg_cnt == 1) {
            auto e = gen_exp(ast[0], ctx);
            if (not is_scalar_type(e.type())) {
                throw t_bad_operands();
            }
            res = gen_is_zero(e, ctx);
        } else if (op == "~" and arg_cnt == 1) {
            x = gen_exp(ast[0], ctx);
            if (not is_integral_type(x.type())) {
                throw t_bad_operands();
            }
            gen_int_promotion(x, ctx);
            if (x.is_constant()) {
                return ~x;
            }
            res = t_val(prog.bit_not(ctx.asmv(x)), x.type());
        } else if (op == "=") {
            res = gen_convert_assign(gen_exp(ast[0], ctx, false),
                                     gen_exp(ast[1], ctx),
                                     ctx);
        } else if (op == "+") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_add(x, y, ctx);
        } else if (op == "-") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_sub(x, y, ctx);
        } else if (op == "*") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_mul(x, y, ctx);
        } else if (op == "/") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_div(x, y, ctx);
        } else if (op == "%") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_mod(x, y, ctx);
        } else if (op == "<<") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_shl(x, y, ctx);
        } else if (op == ">>") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_shr(x, y, ctx);
        } else if (op == "<=") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_is_zero(gen_lt(y, x, ctx), ctx);
        } else if (op == "<") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_lt(x, y, ctx);
        } else if (op == ">") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_lt(y, x, ctx);
        } else if (op == ">=") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_is_zero(gen_lt(x, y, ctx), ctx);
        } else if (op == "==") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_eq(x, y, ctx);
        } else if (op == "!=") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_is_zero(gen_eq(x, y, ctx), ctx);
        } else if (op == "&") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_and(x, y, ctx);
        } else if (op == "^") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_xor(x, y, ctx);
        } else if (op == "|") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_or(x, y, ctx);
        } else if (op == "&&") {
            auto xt = silent_gen_exp(ast[0], ctx);
            auto yt = silent_gen_exp(ast[1], ctx);
            if (xt.is_constant() and yt.is_constant()) {
                if (xt.is_false() or yt.is_false()) {
                    res = t_val(0);
                } else {
                    res = t_val(1);
                }
            } else {
                auto l0 = make_label();
                auto l1 = make_label();
                auto l2 = make_label();
                auto l3 = make_label();
                x = gen_exp(ast[0], ctx);
                put_label(l0);
                prog.cond_br(gen_is_zero_i1(x, ctx), l1, l2);
                put_label(l2, false);
                y = gen_exp(ast[1], ctx);
                if (not (is_scalar_type(x.type())
                         and is_scalar_type(y.type()))) {
                    throw t_bad_operands();
                }
                auto w = gen_is_nonzero(y, ctx);
                put_label(l3);
                put_label(l1);
                auto res_id = prog.phi(false_val, l0, ctx.asmv(w), l3);
                res = t_val(res_id, int_type);
            }
        } else if (op == "||") {
            auto xt = silent_gen_exp(ast[0], ctx);
            auto yt = silent_gen_exp(ast[1], ctx);
            if (xt.is_constant() and yt.is_constant()) {
                if (xt.is_false() and yt.is_false()) {
                    res = t_val(0);
                } else {
                    res = t_val(1);
                }
            } else {
                auto l0 = make_label();
                auto l1 = make_label();
                auto l2 = make_label();
                auto l3 = make_label();
                x = gen_exp(ast[0], ctx);
                put_label(l0);
                prog.cond_br(gen_is_zero_i1(x, ctx), l2, l1);
                put_label(l2, false);
                y = gen_exp(ast[1], ctx);
                if (not (is_scalar_type(x.type())
                         and is_scalar_type(y.type()))) {
                    throw t_bad_operands();
                }
                auto w = gen_is_nonzero(y, ctx);
                put_label(l3);
                put_label(l1);
                auto res_id = prog.phi(true_val, l0, ctx.asmv(w), l3);
                res = t_val(res_id, int_type);
            }
        } else if (op == "*=") {
            res = assign_op(gen_mul);
        } else if (op == "/=") {
            res = assign_op(gen_div);
        } else if (op == "%=") {
            res = assign_op(gen_mod);
        } else if (op == "+=") {
            res = assign_op(gen_add);
        } else if (op == "-=") {
            res = assign_op(gen_sub);
        } else if (op == "<<=") {
            res = assign_op(gen_shl);
        } else if (op == ">>=") {
            res = assign_op(gen_shr);
        } else if (op == "&=") {
            res = assign_op(gen_and);
        } else if (op == "^=") {
            res = assign_op(gen_xor);
        } else if (op == "|=") {
            res = assign_op(gen_or);
        } else if (op == ",") {
            gen_exp(ast[0], ctx);
            res = gen_exp(ast[1], ctx);
        } else if (op == "function_call") {
            if (ast[0] == t_ast("identifier", "printf")) {
                vector<t_asm_val> args;
                for (auto i = size_t(1); i < ast.children.size(); i++) {
                    auto e = gen_exp(ast[i], ctx);
                    if (e.type() == float_type) {
                        e = gen_conversion(double_type, e, ctx);
                    }
                    args.push_back(ctx.asmv(e));
                }
                res = t_val(prog.call_printf(args), int_type);
            } else {
                throw runtime_error("unknown function");
            }
        } else if (op == "struct_member") {
            x = gen_exp(ast[0], ctx, false);
            auto m = ast[1].vv;
            if (not is_struct_type(x.type())) {
                throw t_bad_operands();
            }
            auto struct_name = x.type().get_name();
            auto type = ctx.get_type_data(struct_name).type;
            auto idx = type.get_member_index(m);
            if (idx == -1) {
                throw t_bad_operands();
            }
            auto res_type = type.get_member_type(idx);
            auto res_id = prog.member(ctx.asmv(x), idx);
            res = t_val(res_id, res_type, x.is_lvalue());
        } else if (op == "array_subscript") {
            auto z = gen_add(gen_exp(ast[0], ctx),
                             gen_exp(ast[1], ctx), ctx);
            res = dereference(z);
        } else if (op == "postfix_increment") {
            auto e = gen_exp(ast[0], ctx, false);
            res = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(res.type()))
                or not is_modifiable_lvalue(res)) {
                throw t_bad_operands();
            }
            auto z = gen_add(res, 1, ctx);
            gen_assign(e, z, ctx);
        } else if (op == "postfix_decrement") {
            auto e = gen_exp(ast[0], ctx, false);
            res = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(res.type()))
                or not is_modifiable_lvalue(res)) {
                throw t_bad_operands();
            }
            auto z = gen_sub(res, 1, ctx);
            gen_assign(e, z, ctx);
        } else if (op == "prefix_increment") {
            auto e = gen_exp(ast[0], ctx, false);
            auto z = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(z.type()))
                or not is_modifiable_lvalue(z)) {
                throw t_bad_operands();
            }
            res = gen_add(z, 1, ctx);
            gen_assign(e, res, ctx);
        } else if (op == "prefix_decrement") {
            auto e = gen_exp(ast[0], ctx, false);
            auto z = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(z.type()))
                or not is_modifiable_lvalue(z)) {
                throw t_bad_operands();
            }
            res = gen_sub(z, 1, ctx);
            gen_assign(e, res, ctx);
        } else if (op == "cast") {
            auto e = gen_exp(ast[1], ctx);
            auto t = make_type(ast[0], ctx);
            if (not (t == void_type or (is_scalar_type(e.type())
                                        and is_scalar_type(unqualify(t))))) {
                throw t_bad_operands();
            }
            res = gen_conversion(t, e, ctx);
        } else if (op == "sizeof_type") {
            auto type = make_type(ast[0], ctx);
            res = t_val(type.get_size());
        } else if (op == "sizeof_exp") {
            x = silent_gen_exp(ast[0], ctx, false);
            res = t_val(x.type().get_size());
        } else {
            throw logic_error("unhandled operator " + op);
        }
        if (convert_lvalue and res.is_lvalue()) {
            if (is_array_type(res.type())) {
                res = adr(gen_array_elt(res, 0, ctx));
            } else {
                res = convert_lvalue_to_rvalue(res, ctx);
            }
        }
        return res;
    }
}

t_val gen_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx) {
    prog.store(ctx.asmv(rhs), ctx.asmv(lhs));
    return lhs;
}

t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx) {
    return gen_assign(lhs, gen_conversion(lhs.type(), rhs, ctx), ctx);
}

t_val gen_is_zero(const t_val& x, const t_ctx& ctx) {
    return gen_eq(x, 0, ctx);
}

t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx) {
    auto w = gen_is_zero(x, ctx);
    return apply("xor", 1, w, ctx);
}

string gen_is_zero_i1(const t_val& x, const t_ctx& ctx) {
    auto w = gen_is_zero(x, ctx);
    return prog.convert("trunc", ctx.asmv(w), "i1");
}

string gen_is_nonzero_i1(const t_val& x, const t_ctx& ctx) {
    auto w = gen_is_nonzero(x, ctx);
    return prog.convert("trunc", ctx.asmv(w), "i1");
}

t_val gen_array_elt(const t_val& v, int i, t_ctx& ctx) {
    auto res_id = prog.member(ctx.asmv(v), i);
    auto res_type = v.type().get_element_type();
    return t_val(res_id, res_type, true);
}

t_val gen_struct_member(const t_val& v, int i, t_ctx& ctx) {
    auto res_type = v.type().get_member_type(i);
    auto res_id = prog.member(ctx.asmv(v), i);
    return t_val(res_id, res_type, true);
}

t_val gen_exp(const t_ast& ast, t_ctx& ctx, bool convert_lvalue) {
    try {
        return gen_exp_(ast, ctx, convert_lvalue);
    } catch (t_bad_operands) {
        auto op = ast.uu;
        if (ast.vv != "") {
            op += " " + ast.vv;
        }
        err("bad operands to " + op, ast.loc);
    } catch (const t_conversion_error& e) {
        err(e.what(), ast.loc);
    }
}
