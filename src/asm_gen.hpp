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

class t_conversion_error : public t_compile_error {
public:
    t_conversion_error(const t_type& a, const t_type& b)
        : t_compile_error("cannot convert " + stringify(a)
                          + " to " + stringify(b))
    {}
};

class t_undefined_name_error {};
class t_redefinition_error {};
class t_bad_operands {};

template <class t>
class t_namespace {
    std::unordered_map<std::string, t> outer_scope;
    std::unordered_map<std::string, t> scope;
public:
    const t& get(const std::string& name) const {
        auto it = scope.find(name);
        if (it != scope.end()) {
            return (*it).second;
        }
        auto oit = outer_scope.find(name);
        if (oit != outer_scope.end()) {
            return (*oit).second;
        } else {
            throw t_undefined_name_error();
        }
    }
    void def(const std::string& name, const t& data) {
        auto it = scope.find(name);
        if (it != scope.end()) {
            throw t_redefinition_error();
        }
        scope[name] = data;
    }
    void enter_scope() {
        for (const auto& [name, data] : scope) {
            outer_scope[name] = data;
        }
        scope.clear();
    }
};

struct t_type_data {
    t_type type;
    std::string asm_id;
};

class t_ctx {
    t_namespace<t_type_data> types;
    t_namespace<t_val> vars;
    t_namespace<std::string> labels;
    std::string _loop_body_end;
    std::string _loop_end;
    std::string _func_name;
public:
    const std::string& get_label_data(const std::string& name) const {
        return labels.get(name);
    }
    void def_label(const std::string& name, const std::string& data) {
        labels.def(name, data);
    }
    const t_val& get_var_data(const std::string& name) const {
        return vars.get(name);
    }
    void def_var(const std::string& name, const t_val& data) {
        vars.def(name, data);
    }
    const t_type_data& get_type_data(const std::string& name) const {
        return types.get(name);
    }
    void def_type(const std::string& name, const t_type_data& data) {
        types.def(name, data);
    }

    auto loop_body_end(const std::string& x) {
        _loop_body_end = x;
    }
    const auto& loop_body_end() {
        return _loop_body_end;
    }
    auto loop_end(const std::string& x) {
        _loop_end = x;
    }
    const auto& loop_end() {
        return _loop_end;
    }
    auto func_name(const std::string& x) {
        _func_name = x;
    }
    const auto& func_name() {
        return _func_name;
    }

    t_type complete_type(const t_type& t) const;
    std::string asmt(const t_type& t, bool expand = false) const;
    t_asm_val asmv(const t_val& val) const;
    void enter_scope() {
        types.enter_scope();
        vars.enter_scope();
    }
};

void err(const std::string& str, t_loc loc = t_loc());
std::string make_new_id();
std::string make_label();
void put_label(const std::string& l, bool f = true);
std::string func_line(const std::string&);
t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_struct_member(const t_val& v, int i, t_ctx& ctx);
t_val gen_is_zero(const t_val& x, const t_ctx& ctx);
std::string gen_is_zero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx);
std::string gen_is_nonzero_i1(const t_val& x, const t_ctx& ctx);
t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx);
t_val gen_array_elt(const t_val& v, int i, t_ctx& ctx);
t_val gen_exp(const t_ast& ast, t_ctx& ctx, bool convert_lvalue = true);
t_type make_base_type(const t_ast& t, t_ctx& ctx);
std::string unpack_declarator(t_type& type, const t_ast& t);
std::string gen_asm(const t_ast&);

extern t_prog prog;
