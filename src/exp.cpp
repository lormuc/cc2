#include <string>
#include <cassert>
#include <iostream>

#include "gen.hpp"
#include "exp.hpp"

class t_bad_operands {};

class t_conversion_error : public t_compile_error {
public:
    t_conversion_error(const t_type& a, const t_type& b)
        : t_compile_error("cannot convert " + stringify(a)
                          + " to " + stringify(b))
    {}
};

namespace {
    t_val gen_neg(const t_val& x, const t_ctx& ctx);
    t_val gen_eq(t_val x, t_val y, const t_ctx& ctx);
    t_val exp(const t_ast& ast, t_ctx& ctx, bool convert = true);

    void err(const str& str, t_loc loc) {
        throw t_compile_error(str, loc);
    }

    bool is_modifiable_lvalue(const t_val& x) {
        return x.is_lvalue();
    }

    t_val adr(const t_val& x) {
        return t_val(x.as(), make_pointer_type(x.type()),
                     false, x.is_constant());
    }

    bool is_null(const t_val& x) {
        return (x.is_constant() and x.is_false()
                and (x.type().is_integral() or x.type() == void_pointer_type));
    }

    t_val apply(str op, const t_val& x, const t_val& y, const t_ctx& ctx) {
        _ res_id = prog.apply(op, ctx.as(x), ctx.as(y));
        return t_val(res_id, x.type());
    }

    t_type make_type(const t_ast& ast, t_ctx& ctx) {
        _ res = make_base_type(ast[0], ctx);
        if (ast.children.size() == 2) {
            unpack_declarator(res, ast[1], ctx);
        }
        return res;
    }

    bool ptrs_to_compatible(const t_val& x, const t_val& y) {
        return (x.type().is_pointer() and y.type().is_pointer()
                and compatible(unqualify(x.type().pointee_type()),
                               unqualify(y.type().pointee_type())));
    }

    t_type common_arithmetic_type(t_type aa, t_type bb) {
        _ types = std::vector{
            long_double_type, double_type, float_type,
            u_long_type, long_type, u_int_type
        };
        for (_& tt : types) {
            if (aa == tt or bb == tt) {
                return tt;
            }
        }
        return int_type;
    }

    void gen_arithmetic_conversions(t_val& x, t_val& y, const t_ctx& ctx) {
        _ common_type = common_arithmetic_type(x.type(), y.type());
        x = gen_conversion(common_type, x, ctx);
        y = gen_conversion(common_type, y, ctx);
    }

    _ compile_time_eval(const t_ast& ast, t_ctx& ctx, bool c = true) {
        _ o = prog.silence();
        prog.silence(true);
        _ res = exp(ast, ctx, c);
        prog.silence(o);
        return res;
    }

    t_val gen_sub(t_val x, t_val y, const t_ctx& ctx) {
        _ args_are_constant = (x.is_constant() and y.is_constant());
        if (x.type().is_arithmetic() and y.type().is_arithmetic()) {
            gen_arithmetic_conversions(x, y, ctx);
            if (args_are_constant) {
                return x - y;
            }
            str op;
            if (x.type().is_signed_integer()) {
                op = "sub nsw";
            } else if (x.type().is_floating()) {
                op = "fsub";
            } else {
                op = "sub";
            }
            return apply(op, x, y, ctx);
        } else if (ptrs_to_compatible(x, y)) {
            _ w = x.type().pointee_type();
            x = gen_conversion(uintptr_t_type, x, ctx);
            y = gen_conversion(uintptr_t_type, y, ctx);
            _ v_id = apply("sub", x, y, ctx).as();
            _ v = t_val(v_id, ptrdiff_t_type);
            return apply("sdiv exact", v, w.size(), ctx);
        } else if (x.type().is_pointer()
                   and x.type().pointee_type().is_object()
                   and y.type().is_integral()) {
            y = gen_conversion(uintptr_t_type, y, ctx);
            y = gen_neg(y, ctx);
            _ res_id = prog.inc_ptr(ctx.as(x), ctx.as(y), args_are_constant);
            return t_val(res_id, x.type(), false, args_are_constant);
        } else {
            throw t_bad_operands();
        }
    }

    t_val convert_lvalue(const t_val& v, t_ctx& ctx) {
        if (v.is_lvalue()) {
            return t_val(prog.load(ctx.as(v)), v.type());
        } else {
            return v;
        }
    }

    t_val convert_array(const t_val& v, t_ctx& ctx) {
        if (v.type().is_array()) {
            return adr(gen_array_elt(v, 0, ctx));
        } else {
            return v;
        }
    }

    t_val convert_function(const t_val& v, t_ctx&) {
        if (v.type().is_function()) {
            return adr(v);
        } else {
            return v;
        }
    }

    t_val gen_neg(const t_val& x, const t_ctx& ctx) {
        return gen_sub(0, x, ctx);
    }

    t_val dereference(const t_val& v, const t_ctx& ctx) {
        _ type = ctx.complete_type(v.type().pointee_type());
        if (type.is_incomplete() and not type.is_function()) {
            throw t_bad_operands();
        }
        return t_val(v.as(), type, true, v.is_constant());
    }

    t_val struct_or_union_member(const t_val& x, const str& field_name,
                                 const t_ctx& ctx) {
        if (not (x.type().is_struct() or x.type().is_union())) {
            throw t_bad_operands();
        }
        _ idx = x.type().field_index(field_name);
        if (idx == t_type::bad_field_index) {
            throw t_bad_operands();
        }
        _ res_type = x.type().field(idx);
        str res_id;
        if (x.type().is_union()) {
            _ w = res_type;
            if (x.is_lvalue()) {
                w = make_pointer_type(w);
            }
            res_id = prog.convert("bitcast", ctx.as(x), w.as(),
                                  x.is_constant());
        } else {
            res_id = prog.member(ctx.as(x), idx, x.is_constant());
        }
        return t_val(res_id, res_type, x.is_lvalue(), x.is_constant());
    }

    t_val gen_lt(t_val x, t_val y, const t_ctx& ctx) {
        if (x.type().is_arithmetic() and y.type().is_arithmetic()) {
            gen_arithmetic_conversions(x, y, ctx);
            if (x.is_constant() and y.is_constant()) {
                return x < y;
            }
            str op;
            if (x.type().is_signed_integer()) {
                op = "icmp slt";
            } else if (x.type().is_floating()) {
                op = "fcmp olt";
            } else {
                op = "icmp ult";
            }
            _ res_id = prog.apply_rel(op, ctx.as(x), ctx.as(y));
            return t_val(res_id, int_type);
        } else if (ptrs_to_compatible(x, y)) {
            y = gen_conversion(x.type(), y, ctx);
            _ res_id = prog.apply_rel("icmp ult", ctx.as(x), ctx.as(y));
            return t_val(res_id, int_type);
        } else {
            throw t_bad_operands();
        }
    }

    _ gen_and(t_val x, t_val y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        if (not (x.type().is_integer() and y.type().is_integer())) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x & y;
        }
        return apply("and", x, y, ctx);
    }

    _ gen_xor(t_val x, t_val y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        if (not (x.type().is_integer() and y.type().is_integer())) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x ^ y;
        }
        return apply("xor", x, y, ctx);
    }

    _ gen_or(t_val x, t_val y, const t_ctx& ctx) {
        gen_arithmetic_conversions(x, y, ctx);
        if (not (x.type().is_integer() and y.type().is_integer())) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x | y;
        }
        return apply("or", x, y, ctx);
    }

    t_val gen_eq(t_val x, t_val y, const t_ctx& ctx) {
        if (x.type().is_arithmetic() and y.type().is_arithmetic()) {
            gen_arithmetic_conversions(x, y, ctx);
            if (x.is_constant() and y.is_constant()) {
                return x == y;
            }
            str op;
            if (x.type().is_floating()) {
                op = "fcmp oeq";
            } else {
                op = "icmp eq";
            }
            _ res_id = prog.apply_rel(op, ctx.as(x), ctx.as(y));
            return t_val(res_id, int_type);
        } else if (x.type().is_pointer() or y.type().is_pointer()) {
            if (ptrs_to_compatible(x, y)) {
                y = gen_conversion(x.type(), y, ctx);
            } else if (is_null(y) or (y.type().is_pointer_to_object()
                                      and unqualify(x.type()) == void_type)) {
                y = gen_conversion(x.type(), y, ctx);
            } else if (is_null(x) or (x.type().is_pointer_to_object()
                                      and unqualify(y.type()) == void_type)) {
                x = gen_conversion(y.type(), x, ctx);
            } else {
                throw t_bad_operands();
            }
            _ res_id = prog.apply_rel("icmp eq", ctx.as(x), ctx.as(y));
            return t_val(res_id, int_type);
        } else {
            throw t_bad_operands();
        }
    }

    t_val gen_mul(t_val x, t_val y, const t_ctx& ctx) {
        if (not (x.type().is_arithmetic()
                 and y.type().is_arithmetic())) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        if (x.is_constant() and y.is_constant()) {
            return x * y;
        }
        str op;
        if (x.type().is_signed_integer()) {
            op = "mul nsw";
        } else if (x.type().is_floating()) {
            op = "fmul";
        } else {
            op = "mul";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_mod(t_val x, t_val y, const t_ctx& ctx) {
        if (not (x.type().is_integral() and y.type().is_integral())) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        if (x.is_constant() and y.is_constant()) {
            return x % y;
        }
        str op;
        if (x.type().is_signed_integer()) {
            op = "srem";
        } else {
            op = "urem";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_shr(t_val x, t_val y, const t_ctx& ctx) {
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        if (not (x.type().is_integer() and y.type().is_integer())) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x >> y;
        }
        str op;
        if (x.type().is_signed_integer()) {
            op = "ashr";
        } else {
            op = "lshr";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_shl(t_val x, t_val y, const t_ctx& ctx) {
        gen_int_promotion(x, ctx);
        gen_int_promotion(y, ctx);
        if (not (x.type().is_integer() and y.type().is_integer())) {
            throw t_bad_operands();
        }
        if (x.is_constant() and y.is_constant()) {
            return x << y;
        }
        return apply("shl", x, y, ctx);
    }

    t_val gen_div(t_val x, t_val y, const t_ctx& ctx) {
        if (not (x.type().is_arithmetic() and y.type().is_arithmetic())) {
            throw t_bad_operands();
        }
        gen_arithmetic_conversions(x, y, ctx);
        if (x.is_constant() and y.is_constant()) {
            return x / y;
        }
        str op;
        if (x.type().is_floating()) {
            op = "fdiv";
        } else if (x.type().is_signed_integer()) {
            op = "sdiv";
        } else {
            op = "udiv";
        }
        return apply(op, x, y, ctx);
    }

    t_val gen_add(t_val x, t_val y, const t_ctx& ctx) {
        if (x.type().is_integral() and y.type().is_pointer()) {
            std::swap(x, y);
        }
        _ args_are_constant = (x.is_constant() and y.is_constant());
        if (x.type().is_arithmetic() and y.type().is_arithmetic()) {
            gen_arithmetic_conversions(x, y, ctx);
            if (args_are_constant) {
                return x + y;
            }
            str op;
            if (x.type().is_signed_integer()) {
                op = "add nsw";
            } else if (x.type().is_floating()) {
                op = "fadd";
            } else {
                op = "add";
            }
            return apply(op, x, y, ctx);
        } else if (x.type().is_pointer() and y.type().is_integral()) {
            y = gen_conversion(uintptr_t_type, y, ctx);
            _ res_id = prog.inc_ptr(ctx.as(x), ctx.as(y), args_are_constant);
            return t_val(res_id, x.type(), false, args_are_constant);
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

    t_val exp_(const t_ast& ast, t_ctx& ctx, bool convert) {
        _ assign_op = [&](_& op) {
            _ x = exp(ast[0], ctx, false);
            _ y = exp(ast[1], ctx);
            _ xv = convert_lvalue(x, ctx);
            _ z = op(xv, y, ctx);
            return gen_convert_assign(x, z, ctx);
        };
        _& op = ast.uu;
        _ arg_cnt = ast.children.size();
        t_val x;
        t_val y;
        t_val z;
        t_val res;
        if (op == "integer_constant") {
            unsigned long w;
            try {
                w = stoul(ast.vv, 0, 0);
            } catch (std::out_of_range) {
                err("unrepresentable value", ast.loc);
            } catch (std::invalid_argument) {
                err("bad value", ast.loc);
            }
            _ u_suf = (ast.vv.find('u') != str::npos
                       or ast.vv.find('U') != str::npos);
            _ l_suf = (ast.vv.find('l') != str::npos
                       or ast.vv.find('L') != str::npos);
            vec<t_type> types;
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
            for (_& type : types) {
                if (can_repr(type, w)) {
                    res = t_val(w, type);
                    break;
                }
            }
        } else if (op == "floating_constant") {
            _ w = stod(ast.vv);
            _ suffix = ast.vv.back();
            if (suffix == 'f' or suffix == 'F') {
                res = t_val(w, float_type);
            } else if (suffix == 'l' or suffix == 'L') {
                res = t_val(w, long_double_type);
            } else {
                res = t_val(w, double_type);
            }
        } else if (op == "string_literal") {
            _ id = prog.def_str(ast.vv);
            _ t = make_array_type(char_type, ast.vv.length() + 1);
            res = t_val(id, t, true, true);
        } else if (op == "char_constant") {
            res = t_val(int(ast.vv[0]));
        } else if (op == "identifier") {
            if (ast.vv == "printf") {
                _ tp = make_func_type(int_type, {string_type}, true);
                res = t_val(str("@printf"), tp);
            } else if (ast.vv == "snprintf") {
                _ tp = make_func_type(int_type,
                                      {string_type, size_t_type, string_type},
                                      true);
                res = t_val(str("@snprintf"), tp);
            } else if (ast.vv == "calloc") {
                _ tp = make_func_type(void_pointer_type,
                                      {size_t_type, size_t_type});
                res = t_val(str("@calloc"), tp);
            } else {
                res = ctx.get_id_data(ast.vv).val;
            }
        } else if (op == "un_plus" and arg_cnt == 1) {
            res = exp(ast[0], ctx);
            if (not res.type().is_arithmetic()) {
                throw t_bad_operands();
            }
            gen_int_promotion(res, ctx);
        } else if (op == "adr_op" and arg_cnt == 1) {
            _ w = exp(ast[0], ctx, false);
            if (not (w.is_lvalue() or w.type().is_function())) {
                throw t_bad_operands();
            }
            res = adr(w);
        } else if (op == "ind_op" and arg_cnt == 1) {
            _ e = exp(ast[0], ctx);
            if (not e.type().is_pointer()) {
                throw t_bad_operands();
            }
            res = dereference(e, ctx);
        } else if (op == "un_minus" and arg_cnt == 1) {
            _ e = exp(ast[0], ctx);
            if (not e.type().is_arithmetic()) {
                throw t_bad_operands();
            }
            res = gen_neg(e, ctx);
        } else if (op == "not_op" and arg_cnt == 1) {
            _ e = exp(ast[0], ctx);
            if (not e.type().is_scalar()) {
                throw t_bad_operands();
            }
            res = gen_is_zero(e, ctx);
        } else if (op == "bit_not_op" and arg_cnt == 1) {
            x = exp(ast[0], ctx);
            if (not x.type().is_integral()) {
                throw t_bad_operands();
            }
            gen_int_promotion(x, ctx);
            if (x.is_constant()) {
                return ~x;
            }
            res = t_val(prog.bit_not(ctx.as(x)), x.type());
        } else if (op == "=") {
            res = gen_convert_assign(exp(ast[0], ctx, false),
                                     exp(ast[1], ctx),
                                     ctx);
        } else if (op == "+") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_add(x, y, ctx);
        } else if (op == "-") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_sub(x, y, ctx);
        } else if (op == "*") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_mul(x, y, ctx);
        } else if (op == "/") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_div(x, y, ctx);
        } else if (op == "%") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_mod(x, y, ctx);
        } else if (op == "<<") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_shl(x, y, ctx);
        } else if (op == ">>") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_shr(x, y, ctx);
        } else if (op == "<=") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_is_zero(gen_lt(y, x, ctx), ctx);
        } else if (op == "<") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_lt(x, y, ctx);
        } else if (op == ">") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_lt(y, x, ctx);
        } else if (op == ">=") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_is_zero(gen_lt(x, y, ctx), ctx);
        } else if (op == "==") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_eq(x, y, ctx);
        } else if (op == "!=") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_is_zero(gen_eq(x, y, ctx), ctx);
        } else if (op == "&") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_and(x, y, ctx);
        } else if (op == "^") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_xor(x, y, ctx);
        } else if (op == "|") {
            x = exp(ast[0], ctx);
            y = exp(ast[1], ctx);
            res = gen_or(x, y, ctx);
        } else if (op == "&&") {
            _ xt = compile_time_eval(ast[0], ctx);
            _ yt = compile_time_eval(ast[1], ctx);
            if (xt.is_constant() and yt.is_constant()) {
                if (xt.is_false() or yt.is_false()) {
                    res = t_val(0);
                } else {
                    res = t_val(1);
                }
            } else {
                _ l0 = make_label();
                _ l1 = make_label();
                _ l2 = make_label();
                _ l3 = make_label();
                x = exp(ast[0], ctx);
                put_label(l0);
                prog.cond_br(gen_is_zero_i1(x, ctx), l1, l2);
                put_label(l2, false);
                y = exp(ast[1], ctx);
                if (not (x.type().is_scalar() and x.type().is_scalar())) {
                    throw t_bad_operands();
                }
                _ w = gen_is_nonzero(y, ctx);
                put_label(l3);
                put_label(l1);
                _ res_id = prog.phi({"i32", "0"}, l0, ctx.as(w), l3);
                res = t_val(res_id, int_type);
            }
        } else if (op == "||") {
            _ xt = compile_time_eval(ast[0], ctx);
            _ yt = compile_time_eval(ast[1], ctx);
            if (xt.is_constant() and yt.is_constant()) {
                if (xt.is_false() and yt.is_false()) {
                    res = t_val(0);
                } else {
                    res = t_val(1);
                }
            } else {
                _ l0 = make_label();
                _ l1 = make_label();
                _ l2 = make_label();
                _ l3 = make_label();
                x = exp(ast[0], ctx);
                put_label(l0);
                prog.cond_br(gen_is_zero_i1(x, ctx), l2, l1);
                put_label(l2, false);
                y = exp(ast[1], ctx);
                if (not (x.type().is_scalar() and y.type().is_scalar())) {
                    throw t_bad_operands();
                }
                _ w = gen_is_nonzero(y, ctx);
                put_label(l3);
                put_label(l1);
                _ res_id = prog.phi({"i32", "1"}, l0, ctx.as(w), l3);
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
            exp(ast[0], ctx);
            res = exp(ast[1], ctx);
        } else if (op == "func_call") {
            x = exp(ast[0], ctx);
            if (not (x.type().is_pointer() and
                     x.type().pointee_type().is_function())) {
                throw t_bad_operands();
            }
            _ t = x.type().pointee_type();
            _& params = t.params();
            _ arg_cnt = ast.children.size() - 1;
            if (not t.is_variadic() and params.size() != arg_cnt) {
                throw t_bad_operands();
            }
            vec<t_asm_val> args;
            for (size_t i = 0; i < arg_cnt; i++) {
                _ e = exp(ast[i+1], ctx);
                if (t.is_variadic() and i+1 >= params.size()) {
                    gen_int_promotion(e, ctx);
                    if (e.type() == float_type) {
                        e = gen_conversion(double_type, e, ctx);
                    }
                } else {
                    e = gen_conversion(params[i], e, ctx);
                }
                args.push_back(ctx.as(e));
            }
            _ rt = (t.is_variadic() ? t : t.return_type());
            if (t.return_type() == void_type) {
                prog.call_void(x.as(), args);
                res = t_val();
            } else {
                res = t_val(prog.call(rt.as(), x.as(), args), t.return_type());
            }
        } else if (op == "member") {
            x = exp(ast[0], ctx, false);
            res = struct_or_union_member(x, ast[1].vv, ctx);
        } else if (op == "arrow") {
            x = dereference(exp(ast[0], ctx), ctx);
            res = struct_or_union_member(x, ast[1].vv, ctx);
        } else if (op == "array_subscript") {
            _ z = gen_add(exp(ast[0], ctx),
                          exp(ast[1], ctx), ctx);
            res = dereference(z, ctx);
        } else if (op == "postfix_inc") {
            _ e = exp(ast[0], ctx, false);
            if (not unqualify(e.type()).is_scalar()
                or not is_modifiable_lvalue(e)) {
                throw t_bad_operands();
            }
            res = convert_lvalue(e, ctx);
            _ z = gen_add(res, 1, ctx);
            gen_convert_assign(e, z, ctx);
        } else if (op == "postfix_dec") {
            _ e = exp(ast[0], ctx, false);
            if (not unqualify(e.type()).is_scalar()
                or not is_modifiable_lvalue(e)) {
                throw t_bad_operands();
            }
            res = convert_lvalue(e, ctx);
            _ z = gen_sub(res, 1, ctx);
            gen_convert_assign(e, z, ctx);
        } else if (op == "prefix_inc") {
            _ e = exp(ast[0], ctx, false);
            if (not unqualify(e.type()).is_scalar()
                or not is_modifiable_lvalue(e)) {
                throw t_bad_operands();
            }
            _ z = convert_lvalue(e, ctx);
            res = gen_add(z, 1, ctx);
            gen_convert_assign(e, res, ctx);
        } else if (op == "prefix_dec") {
            _ e = exp(ast[0], ctx, false);
            if (not unqualify(e.type()).is_scalar()
                or not is_modifiable_lvalue(e)) {
                throw t_bad_operands();
            }
            _ z = convert_lvalue(e, ctx);
            res = gen_sub(z, 1, ctx);
            gen_convert_assign(e, res, ctx);
        } else if (op == "cast") {
            _ e = exp(ast[1], ctx);
            _ t = make_type(ast[0], ctx);
            if (not (t == void_type or (e.type().is_scalar()
                                        and unqualify(t).is_scalar()))) {
                throw t_bad_operands();
            }
            res = gen_conversion(t, e, ctx);
        } else if (op == "sizeof_op") {
            if (ast[0].uu == "type_name") {
                _ type = make_type(ast[0], ctx);
                res = t_val(type.size());
            } else {
                x = compile_time_eval(ast[0], ctx, false);
                res = t_val(x.type().size());
            }
        } else if (op == "?:") {
            x = exp(ast[0], ctx);
            constrain(x.type().is_scalar(), "operand is not a scalar",
                      ast[0].loc);
            _ yy = compile_time_eval(ast[1], ctx);
            _ yt = yy.type();
            _ zz = compile_time_eval(ast[2], ctx);
            _ zt = zz.type();
            _ common_type = yt;
            if (yt == zt) {
            } else if (yt.is_arithmetic() and zt.is_arithmetic()) {
                common_type = common_arithmetic_type(yt, zt);
            } else {
                constrain(false,
                          "could not bring the operands to a common type",
                          ast.loc);
            }
            if (x.is_constant()) {
                _ w = exp(ast[x.is_false() ? 2 : 1], ctx);
                res = gen_conversion(common_type, w, ctx);
            } else {
                _ cond_true = make_label();
                _ cond_false = make_label();
                _ end = make_label();

                prog.cond_br(gen_is_zero_i1(x, ctx),
                             cond_false, cond_true);

                put_label(cond_true, false);
                y = exp(ast[1], ctx);
                _ val_if_true = gen_conversion(common_type, y, ctx);
                prog.br(end);

                put_label(cond_false, false);
                z = exp(ast[2], ctx);
                _ val_if_false = gen_conversion(common_type, z, ctx);
                _ cond_false_end = make_label();
                put_label(cond_false_end);

                put_label(end);
                _ res_id = prog.phi(ctx.as(val_if_true), cond_true,
                                    ctx.as(val_if_false), cond_false_end);
                res = t_val(res_id, common_type);
            }
        } else {
            throw std::logic_error("unhandled operator " + op);
        }
        if (convert) {
            res = convert_function(res, ctx);
            res = convert_array(res, ctx);
            res = convert_lvalue(res, ctx);
        }
        return res;
    }

    t_val exp(const t_ast& ast, t_ctx& ctx, bool convert_lvalue) {
        t_val res;
        try {
            res = exp_(ast, ctx, convert_lvalue);
        } catch (t_bad_operands) {
            _ op = ast.uu;
            if (ast.vv != "") {
                op += " " + ast.vv;
            }
            err("bad operands to " + op, ast.loc);
        } catch (const t_conversion_error& e) {
            err(e.what(), ast.loc);
        }
        return res;
    }
}

t_val gen_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx) {
    prog.store(ctx.as(rhs), ctx.as(lhs));
    return lhs;
}

t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx) {
    return gen_assign(lhs, gen_conversion(lhs.type(), rhs, ctx), ctx);
}

t_val gen_is_zero(const t_val& x, const t_ctx& ctx) {
    return gen_eq(x, 0, ctx);
}

t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx) {
    _ w = gen_is_zero(x, ctx);
    return apply("xor", 1, w, ctx);
}

str gen_is_zero_i1(const t_val& x, const t_ctx& ctx) {
    _ w = gen_is_zero(x, ctx);
    return prog.convert("trunc", ctx.as(w), "i1");
}

str gen_is_nonzero_i1(const t_val& x, const t_ctx& ctx) {
    _ w = gen_is_nonzero(x, ctx);
    return prog.convert("trunc", ctx.as(w), "i1");
}

t_val gen_array_elt(const t_val& v, size_t i, t_ctx& ctx) {
    _ res_id = prog.member(ctx.as(v), i, v.is_constant());
    _ res_type = v.type().element_type();
    return t_val(res_id, res_type, true, v.is_constant());
}

void gen_int_promotion(t_val& x, const t_ctx& ctx) {
    if (x.type() == char_type or x.type() == s_char_type
        or x.type() == u_char_type or x.type() == short_type
        or x.type() == u_short_type or x.type().is_enum()) {
        x = gen_conversion(int_type, x, ctx);
    }
}

t_val gen_conversion(const t_type& t, const t_val& v, const t_ctx& ctx) {
    if (t == v.type()) {
        return v;
    }
    if (t == void_type) {
        return t_val();
    }
    if (compatible(t, v.type())) {
        _ res_id = prog.convert("bitcast", ctx.as(v), t.as());
        return t_val(res_id, t);
    }
    if (not t.is_scalar() or not v.type().is_scalar()) {
        throw t_conversion_error(v.type(), t);
    }
    if (is_null(v) and t.is_pointer()) {
        return make_null_pointer(t);
    }
    if (v.is_constant() and t.is_arithmetic() and v.type().is_arithmetic()) {
        if (t.is_integral() and v.type().is_integral()) {
            return t_val(v.u_val(), t);
        } else if (t.is_integral() and v.type().is_floating()) {
            if (t.is_signed()) {
                return t_val(long(v.f_val()), t);
            } else {
                return t_val((unsigned long)(v.f_val()), t);
            }
        } else if (t.is_floating() and v.type().is_floating()) {
            return t_val(v.f_val(), t);
        } else if (t.is_floating() and v.type().is_integral()) {
            return t_val(double(v.s_val()), t);
        }
    }
    _ x = ctx.as(v);
    _ w = t.as();
    _ op = str("bitcast");
    if (t.is_integral() and v.type().is_integral()) {
        if (t.size() < v.type().size()) {
            op = "trunc";
        } else if (t.size() > v.type().size()) {
            if (v.type().is_signed()) {
                op = "sext";
            } else {
                op = "zext";
            }
        }
    } else if (t.is_floating() and v.type().is_floating()) {
        if (t.size() < v.type().size()) {
            op = "fptrunc";
        } else if (t.size() > v.type().size()) {
            op = "fpext";
        }
    } else if (t.is_floating() and v.type().is_integral()) {
        if (v.type().is_signed()) {
            op = "sitofp";
        } else {
            op = "uitofp";
        }
    } else if (t.is_integral() and v.type().is_floating()) {
        if (t.is_signed()) {
            op = "fptosi";
        } else {
            op = "fptoui";
        }
    } else if (t.is_pointer() and v.type().is_integral()) {
        op = "inttoptr";
    } else if (t.is_pointer() and v.type().is_pointer()) {
        op = "bitcast";
    } else if (t.is_integral() and v.type().is_pointer()) {
        op = "ptrtoint";
    } else {
        throw t_conversion_error(v.type(), t);
    }
    _ res_id = prog.convert(op, x, w);
    return t_val(res_id, t);
}

t_val gen_exp(const t_ast& ast, t_ctx& ctx) {
    if (ast.uu == "exp" or ast.uu == "const_exp") {
        return gen_exp(ast[0], ctx);
    }
    return exp(ast, ctx);
}
