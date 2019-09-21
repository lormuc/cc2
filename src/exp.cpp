#include <string>
#include <cassert>

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
        return t_val{x.value, make_pointer_type(x.type)};
    }

    t_val apply(string op, const t_val& x, const t_val& y, const t_ctx& ctx) {
        t_val res;
        res.type = x.type;
        res.value = prog.apply(op, ctx.asmv(x), ctx.asmv(y));
        return res;
    }

    t_type make_type(const t_ast& ast, t_ctx& ctx) {
        auto res = make_base_type(ast[0], ctx);
        if (ast.children.size() == 2) {
            unpack_declarator(res, ast[1]);
        }
        return res;
    }

    void gen_int_promotion(t_val& x, const t_ctx& ctx) {
        if (x.type == char_type or x.type == s_char_type
            or x.type == u_char_type or x.type == short_type
            or x.type == u_short_type or is_enum_type(x.type)) {
            x = gen_conversion(int_type, x, ctx);
        }
    }

    t_val gen_conversion(const t_type& t, const t_val& v, const t_ctx& ctx) {
        auto res = v;
        res.type = t;
        if (t == void_type) {
            return res;
        }
        if (not is_scalar_type(t) or not is_scalar_type(v.type)) {
            throw t_conversion_error(v.type, t);
        }
        if (not compatible(t, v.type)) {
            auto x = ctx.asmv(v);
            auto w = ctx.asmt(t);
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
                    return res;
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
                    return res;
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
            res.value = prog.convert(op, x, w);
        }
        return res;
    }

    void gen_arithmetic_conversions(t_val& x, t_val& y, const t_ctx& ctx) {
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

    t_val gen_sub(t_val x, t_val y, const t_ctx& ctx) {
        t_val res;
        if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "sub nsw";
            } else if (is_floating_type(x.type)) {
                op = "fsub";
            } else {
                op = "sub";
            }
            res = apply(op, x, y, ctx);
        } else if (is_pointer_type(x.type) and is_pointer_type(y.type)
                   and compatible(unqualify(x.type.get_pointee_type()),
                                  unqualify(y.type.get_pointee_type()))) {
            auto& w = x.type.get_pointee_type();
            x = gen_conversion(uintptr_t_type, x, ctx);
            y = gen_conversion(uintptr_t_type, y, ctx);
            auto v = apply("sub", x, y, ctx);
            v.type = ptrdiff_t_type;
            res = apply("sdiv exact", v, make_constant(w.get_size()), ctx);
        } else if (is_pointer_type(x.type)
                   and is_object_type(x.type.get_pointee_type())
                   and is_integral_type(y.type)) {
            y = gen_conversion(uintptr_t_type, y, ctx);
            y = gen_neg(y, ctx);
            res.type = x.type;
            res.value = prog.inc_ptr(ctx.asmv(x), ctx.asmv(y));
        } else {
            throw t_bad_operands();
        }
        return res;
    }

    t_val convert_lvalue_to_rvalue(const t_val& v, t_ctx& ctx) {
        t_val res;
        res.type = v.type;
        res.value = prog.load(ctx.asmv(v));
        res.is_lvalue = false;
        return res;
    }

    t_val gen_neg(const t_val& x, const t_ctx& ctx) {
        return gen_sub(make_constant(0), x, ctx);
    }

    auto gen_eq_ptr_conversions(t_val& x, t_val& y,
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

    t_val dereference(const t_val& v) {
        return t_val{v.value, v.type.get_pointee_type(), true};
    }

    t_val gen_rel(const string& rel, t_val x, t_val y, const t_ctx& ctx) {
        if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "icmp s";
            } else if (is_floating_type(x.type)) {
                op = "fcmp o";
            } else {
                op = "icmp u";
            }
            op += rel;
            t_val res;
            res.value = prog.apply_rel(op,
                                       ctx.asmv(x),
                                       ctx.asmv(y));
            res.type = int_type;
            return res;
        } else if (is_pointer_type(x.type)
                   and is_pointer_type(y.type)
                   and compatible(unqualify(x.type),
                                  unqualify(y.type))) {
            t_val res;
            res.value = prog.apply_rel("icmp u" + rel,
                                       ctx.asmv(x),
                                       ctx.asmv(y));
            res.type = int_type;
            return res;
        } else {
            throw t_bad_operands();
        }
    }

    auto gen_and(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        return apply("and", x, y, ctx);
    }

    auto gen_xor(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        return apply("xor", x, y, ctx);
    }

    auto gen_or(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type)
                 and is_integral_type(y.type))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        t_val res;
        res.type = x.type;
        res.value = prog.apply("or", ctx.asmv(x), ctx.asmv(y));
        return res;
    }

    t_val gen_eq(t_val x, t_val y, const t_ctx& ctx) {
        t_val res;
        res.type = int_type;
        if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_floating_type(x.type)) {
                op = "fcmp oeq";
            } else {
                op = "icmp eq";
            }
            res.value = prog.apply_rel(op, ctx.asmv(x), ctx.asmv(y));
        } else if (is_pointer_type(x.type) or is_pointer_type(y.type)) {
            gen_eq_ptr_conversions(x, y, ctx);
            res.value = prog.apply_rel("icmp eq",
                                       ctx.asmv(x),
                                       ctx.asmv(y));
        } else {
            throw t_bad_operands();
        }
        return res;
    }

    t_val gen_mul(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_arithmetic_type(x.type) and is_arithmetic_type(y.type))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "mul nsw";
        } else if (is_floating_type(x.type)) {
            op = "fmul";
        } else {
            op = "mul";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_mod(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "srem";
        } else {
            op = "urem";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_shr(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands();
        }
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "ashr";
        } else {
            op = "lshr";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_shl(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
            throw t_bad_operands();
        }
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        return apply("shl", x, y, ctx);
    }

    t_val gen_div(t_val x, t_val y, const t_ctx& ctx) {
        if (not (is_arithmetic_type(x.type) and is_arithmetic_type(y.type))) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        string op;
        if (is_floating_type(x.type)) {
            op = "fdiv";
        } else if (is_signed_integer_type(x.type)) {
            op = "sdiv";
        } else {
            op = "udiv";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_add(t_val x, t_val y, const t_ctx& ctx) {
        if (is_integral_type(x.type) and is_pointer_type(y.type)) {
            swap(x, y);
        }
        if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "add nsw";
            } else if (is_floating_type(x.type)) {
                op = "fadd";
            } else {
                op = "add";
            }
            return apply(op, x, y, ctx);
        } else if (is_pointer_type(x.type) and is_integral_type(y.type)) {
            y = gen_conversion(uintptr_t_type, y, ctx);
            t_val res;
            res.value = prog.inc_ptr(ctx.asmv(x), ctx.asmv(y));
            res.type = x.type;
            return res;
        } else {
            throw t_bad_operands();
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
            res = make_constant(stoi(ast.vv));
        } else if (op == "floating_constant") {
            res = make_constant(stod(ast.vv));
        } else if (op == "string_literal") {
            res.value = prog.def_str(ast.vv);
            res.type = make_array_type(char_type, ast.vv.length() + 1);
            res.is_lvalue = true;
        } else if (op == "identifier") {
            res = ctx.get_var_data(ast.vv);
        } else if (op == "+" and arg_cnt == 1) {
            res = gen_exp(ast[0], ctx);
            if (not is_arithmetic_type(res.type)) {
                throw t_bad_operands();
            }
            gen_int_promotion(res, ctx);
        } else if (op == "&" and arg_cnt == 1) {
            res = gen_exp(ast[0], ctx, false);
            if (not res.is_lvalue) {
                throw t_bad_operands();
            }
            res.type = make_pointer_type(res.type);
            res.is_lvalue = false;
        } else if (op == "*" and arg_cnt == 1) {
            auto e = gen_exp(ast[0], ctx);
            if (not is_pointer_type(e.type)) {
                throw t_bad_operands();
            }
            res = dereference(e);
        } else if (op == "-" and arg_cnt == 1) {
            auto e = gen_exp(ast[0], ctx);
            if (not is_arithmetic_type(e.type)) {
                throw t_bad_operands();
            }
            res = gen_neg(e, ctx);
        } else if (op == "!" and arg_cnt == 1) {
            auto e = gen_exp(ast[0], ctx);
            if (not is_scalar_type(e.type)) {
                throw t_bad_operands();
            }
            res = gen_is_zero(e, ctx);
        } else if (op == "~" and arg_cnt == 1) {
            x = gen_exp(ast[0], ctx);
            if (not is_integral_type(x.type)) {
                throw t_bad_operands();
            }
            gen_int_promotion(x, ctx);
            res.value = prog.bit_not(ctx.asmv(x));
            res.type = x.type;
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
            res = gen_rel("le", x, y, ctx);
        } else if (op == "<") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_rel("lt", x, y, ctx);
        } else if (op == ">") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_rel("gt", x, y, ctx);
        } else if (op == ">=") {
            x = gen_exp(ast[0], ctx);
            y = gen_exp(ast[1], ctx);
            res = gen_rel("ge", x, y, ctx);
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
            auto l0 = make_label();
            auto l1 = make_label();
            auto l2 = make_label();
            auto l3 = make_label();
            x = gen_exp(ast[0], ctx);
            put_label(l0);
            prog.cond_br(gen_is_zero_i1(x, ctx), l1, l2);
            put_label(l2, false);
            y = gen_exp(ast[1], ctx);
            if (not (is_scalar_type(x.type)
                     and is_scalar_type(y.type))) {
                throw t_bad_operands();
            }
            auto w = gen_is_nonzero(y, ctx);
            put_label(l3);
            put_label(l1);
            res.type = int_type;
            res.value = prog.phi(false_val, l0, ctx.asmv(w), l3);
        } else if (op == "||") {
            auto l0 = make_label();
            auto l1 = make_label();
            auto l2 = make_label();
            auto l3 = make_label();
            x = gen_exp(ast[0], ctx);
            put_label(l0);
            prog.cond_br(gen_is_zero_i1(x, ctx), l2, l1);
            put_label(l2, false);
            y = gen_exp(ast[1], ctx);
            if (not (is_scalar_type(x.type)
                     and is_scalar_type(y.type))) {
                throw t_bad_operands();
            }
            auto w = gen_is_nonzero(y, ctx);
            put_label(l3);
            put_label(l1);
            res.type = int_type;
            res.value = prog.phi(true_val, l0, ctx.asmv(w), l3);
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
                    if (e.type == float_type) {
                        e = gen_conversion(double_type, e, ctx);
                    }
                    args.push_back(ctx.asmv(e));
                }
                res.value = prog.call_printf(args);
                res.type = int_type;
            } else {
                throw runtime_error("unknown function");
            }
        } else if (op == "struct_member") {
            x = gen_exp(ast[0], ctx, false);
            auto m = ast[1].vv;
            if (not is_struct_type(x.type)) {
                throw t_bad_operands();
            }
            auto struct_name = x.type.get_name();
            auto type = ctx.get_type_data(struct_name).type;
            auto idx = type.get_member_index(m);
            if (idx == -1) {
                throw t_bad_operands();
            }
            if (x.is_lvalue) {
                res.is_lvalue = true;
            }
            res.type = type.get_member_type(idx);
            res.value = prog.member(ctx.asmv(x), idx);
        } else if (op == "array_subscript") {
            auto z = gen_add(gen_exp(ast[0], ctx),
                             gen_exp(ast[1], ctx), ctx);
            res = dereference(z);
        } else if (op == "postfix_increment") {
            auto e = gen_exp(ast[0], ctx, false);
            res = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(res.type))
                or not is_modifiable_lvalue(res)) {
                throw t_bad_operands();
            }
            auto z = gen_add(res, make_constant(1), ctx);
            gen_assign(e, z, ctx);
        } else if (op == "postfix_decrement") {
            auto e = gen_exp(ast[0], ctx, false);
            res = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(res.type))
                or not is_modifiable_lvalue(res)) {
                throw t_bad_operands();
            }
            auto z = gen_sub(res, make_constant(1), ctx);
            gen_assign(e, z, ctx);
        } else if (op == "prefix_increment") {
            auto e = gen_exp(ast[0], ctx, false);
            auto z = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(z.type))
                or not is_modifiable_lvalue(z)) {
                throw t_bad_operands();
            }
            res = gen_add(z, make_constant(1), ctx);
            gen_assign(e, res, ctx);
        } else if (op == "prefix_decrement") {
            auto e = gen_exp(ast[0], ctx, false);
            auto z = convert_lvalue_to_rvalue(e, ctx);
            if (not is_scalar_type(unqualify(z.type))
                or not is_modifiable_lvalue(z)) {
                throw t_bad_operands();
            }
            res = gen_sub(z, make_constant(1), ctx);
            gen_assign(e, res, ctx);
        } else if (op == "cast") {
            auto e = gen_exp(ast[1], ctx);
            auto t = make_type(ast[0], ctx);
            res = gen_conversion(t, e, ctx);
        } else if (op == "sizeof_type") {
            auto type = make_type(ast[0], ctx);
            res = make_constant(type.get_size());
        } else if (op == "sizeof_exp") {
            prog.silence();
            x = gen_exp(ast[0], ctx, false);
            prog.silence();
            res = make_constant(x.type.get_size());
        } else {
            throw logic_error("unhandled operator " + op);
        }
        if (convert_lvalue and res.is_lvalue) {
            if (is_array_type(res.type)) {
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
    return gen_assign(lhs, gen_conversion(lhs.type, rhs, ctx), ctx);
}

t_val gen_is_zero(const t_val& x, const t_ctx& ctx) {
    return gen_eq(x, make_constant(0), ctx);
}

t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx) {
    auto w = gen_is_zero(x, ctx);
    return apply("xor", make_constant(1), w, ctx);
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
    t_val res;
    res.value = prog.member(ctx.asmv(v), i);
    res.type = v.type.get_element_type();
    res.is_lvalue = true;
    return res;
}

t_val make_constant(ull x) {
    t_val res;
    res.value = std::to_string(x);
    res.type = u_long_type;
    return res;
}

t_val make_constant(int x) {
    t_val res;
    res.value = std::to_string(x);
    res.type = int_type;
    return res;
}

t_val make_constant(double x) {
    t_val res;
    res.value = std::to_string(x);
    res.type = double_type;
    return res;
}

t_val gen_struct_member(const t_val& v, int i, t_ctx& ctx) {
    t_val res;
    res.type = v.type.get_member_type(i);
    res.value = prog.member(ctx.asmv(v), i);
    res.is_lvalue = true;
    return res;
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
