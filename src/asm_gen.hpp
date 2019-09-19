#pragma once

#include <string>
#include <unordered_map>
#include <stdexcept>

#include "ast.hpp"
#include "type.hpp"
#include "prog.hpp"

const auto false_val = t_asm_val{"i32", "0"};
const auto true_val = t_asm_val{"i32", "1"};

struct t_val {
    std::string value;
    t_type type;
    bool is_lvalue = false;
};

class t_bad_operands : public t_compile_error {
public:
    t_bad_operands(const std::string& str, t_loc loc = t_loc())
        : t_compile_error("bad operands to " + str, loc)
    {}
    t_bad_operands(t_loc loc = t_loc())
        : t_compile_error("bad operands", loc)
    {}
};

class t_conversion_error : public std::runtime_error {
public:
    t_conversion_error(const t_type& a, const t_type& b)
        : runtime_error("cannot convert " + stringify(a)
                        + " to " + stringify(b))
    {}
};

class t_unknown_id_error {};
class t_redefinition_error {};

extern t_prog prog;

std::string make_new_id();
std::string make_label();
void put_label(const std::string& l, bool f = true);
void a(const std::string& s);
void a(const std::string& s0, const std::string& s1);
std::string fun(const std::string& func_name, const t_val& a0,
           const t_val& a1);
std::string fun(const std::string& func_name, const std::string& a0,
           const std::string& a1);
void let(const t_val& v, const std::string& s);
void let(const std::string& v, const std::string& s);
std::string func_line(const std::string&);

struct t_scope_map {
    struct t_data {
        t_type type;
        std::string asm_var;
    };
    std::unordered_map<std::string, t_data> map;
    std::unordered_map<std::string, t_data> outer_map;
    auto& get_data(const std::string& id) {
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
    const auto& get_data(const std::string& id) const {
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
    auto get_type(const std::string& id) const {
        return get_data(id).type;
    }
    auto get_asm_var(const std::string& id) const {
        return get_data(id).asm_var;
    }
    auto can_define(const std::string& id) const {
        return map.count(id) == 0;
    }
    auto has(const std::string& id) const {
        return map.count(id) != 0 or outer_map.count(id)!= 0;
    }
};

class t_ctx {
    t_scope_map var_map;
    t_scope_map type_map;
    std::string loop_body_end;
    std::string loop_end;
    std::string func_name;
    std::unordered_map<std::string, std::string> label_map;
public:
    void add_label(const t_ast& ast);

    std::string get_asm_label(const t_ast& ast) const;

    auto set_loop_body_end(const std::string& x) {
        loop_body_end = x;
    }

    const auto& get_loop_body_end() {
        return loop_body_end;
    }

    auto set_loop_end(const std::string& x) {
        loop_end = x;
    }

    const auto& get_loop_end() {
        return loop_end;
    }

    auto set_func_name(const std::string& x) {
        func_name = x;
    }

    const auto& get_func_name() {
        return func_name;
    }

    std::string get_asm_type(const t_type& t, bool expand = false) const;
    t_asm_val get_asm_val(const t_val& val) const;
    std::string define_struct(t_type type);
    t_type complete_type(const t_type& t) const;
    void define_var(const std::string& id, t_type type);

    auto can_define_type(const std::string& id) const {
        return type_map.can_define(id);
    }

    auto can_define_var(const std::string& id) const {
        return var_map.can_define(id);
    }

    auto has_type(const std::string& id) const {
        return type_map.has(id);
    }

    auto has_var(const std::string& id) const {
        return var_map.has(id);
    }

    auto get_var_asm_var(const std::string& id) const {
        return var_map.get_asm_var(id);
    }

    auto get_var_type(const std::string& id) const {
        return var_map.get_type(id);
    }

    auto get_type_asm_var(const std::string& id) const {
        return type_map.get_asm_var(id);
    }

    auto get_type(const std::string& id) const {
        return type_map.get_type(id);
    }

    auto make_asm_arg(const t_val& v) const {
        auto at = get_asm_type(v.type);
        if (v.is_lvalue) {
            at += "*";
        }
        return at + " " + v.value;
    }
};

t_val convert_lvalue_to_rvalue(const t_val& v, t_ctx& ctx);
t_val dereference(const t_val& v);
t_val gen_convert_assign(const t_val& lhs, const t_val& rhs,
                               t_ctx& ctx);
void gen_int_promotion(t_val& x, const t_ctx& ctx);
t_val gen_neg(const t_val& x, const t_ctx& ctx);
t_val gen_eq(t_val x, t_val y, const t_ctx& ctx);
t_val gen_conversion(const t_type& t, const t_val& v,
                           const t_ctx& ctx);
std::string add_string_literal(std::string str);
t_val gen_add(t_val x, t_val y, const t_ctx& ctx);
t_val gen_sub(t_val x, t_val y, const t_ctx& ctx);
t_val gen_mul(t_val x, t_val y, const t_ctx& ctx);
t_val gen_div(t_val x, t_val y, const t_ctx& ctx);
t_val gen_mod(t_val x, t_val y, const t_ctx& ctx);
t_val gen_shl(t_val x, t_val y, const t_ctx& ctx);
t_val gen_shr(t_val x, t_val y, const t_ctx& ctx);
t_val gen_struct_member(const t_val& v, int i, t_ctx& ctx);
t_val gen_is_zero(const t_val& x, const t_ctx& ctx);
std::string gen_is_zero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx);
std::string gen_is_nonzero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_convert_assign(const t_val& lhs, const t_val& rhs,
                               t_ctx& ctx);
t_val gen_assign(const t_val& lhs, const t_val& rhs,
                       t_ctx& ctx);
t_val gen_array_elt(const t_val& v, int i, t_ctx& ctx);
t_val gen_exp(const t_ast& ast, t_ctx& ctx,
                    bool convert_lvalue = true);
void gen_cond_br(const t_val& v, const std::string& a1,
                 const std::string& a2);
t_type make_base_type(const t_ast& t, const t_ctx& ctx);
std::string unpack_declarator(t_type& type, const t_ast& t);

std::string gen_asm(const t_ast&);
