#pragma once

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <functional>

#include "ast.hpp"
#include "type.hpp"
#include "prog.hpp"

const auto false_val = t_asm_val{"i32", "0"};
const auto true_val = t_asm_val{"i32", "1"};

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

class t_val {
    std::string _asm_id;
    t_type _type;
    bool _is_lvalue = false;
    bool _is_constant = false;
    bool _is_void_null = false;
    union {
        unsigned long _i_val;
        double _f_val;
    };

    void i_init(const t_type& t, unsigned long v) {
        _type = t;
        _is_constant = true;
        _i_val = v;
        if (is_signed_type(type())) {
            _asm_id = std::to_string(s_val());
        } else {
            _asm_id = std::to_string(u_val());
        }
    }
    void f_init(const t_type& t, double v) {
        _asm_id = std::to_string(v);
        _type = t;
        _is_constant = true;
        _f_val = v;
    }

public:
    t_val() {
        _type = void_type;
    }
    t_val(const std::string& n_asm_id, const t_type& n_type,
          bool n_is_lvalue = false) {
        _asm_id = n_asm_id;
        _type = n_type;
        _is_lvalue = n_is_lvalue;
    }
    t_val(void* p) {
        assert(p == (void*)0);
        _type = void_pointer_type;
        _is_void_null = true;
        _is_constant = true;
        _asm_id = "null";
    }
    t_val(unsigned long x, const t_type& t) { i_init(t, x); }
    t_val(bool x, const t_type& t) { i_init(t, x); }
    t_val(long x, const t_type& t) { i_init(t, x); }
    t_val(double x, const t_type& t) { f_init(t, x); }
    t_val(char x) { i_init(char_type, x); }
    t_val(unsigned char x) { i_init(u_char_type, x); }
    t_val(unsigned short x) { i_init(u_short_type, x); }
    t_val(unsigned int x) { i_init(u_int_type, x); }
    t_val(unsigned long x) { i_init(u_long_type, x); }
    t_val(ull x) { i_init(u_long_type, x); }

    t_val(signed char x) { i_init(s_char_type, x); }
    t_val(signed short x) { i_init(short_type, x); }
    t_val(signed int x) { i_init(int_type, x); }
    t_val(signed long x) { i_init(long_type, x); }

    t_val(float x) { f_init(float_type, x); }
    t_val(double x) { f_init(double_type, x); }
    t_val(long double x) { f_init(long_double_type, x); }

    std::string asm_id() const { return _asm_id; }
    const t_type& type() const { return _type; }
    bool is_lvalue() const { return _is_lvalue; }
    bool is_constant() const { return _is_constant; }
    unsigned long u_val() const { return _i_val; }
    long s_val() const { return long(_i_val); }
    double f_val() const { return _f_val; }

    bool is_void_null() const { return _is_void_null; }

    bool is_false() const {
        assert(is_constant());
        if (is_void_null()) {
            return true;
        }
        if (is_floating_type(type())) {
            return f_val() == 0;
        } else {
            return u_val() == 0;
        }
    }

#define ar_op(op)                                       \
    t_val operator op(const t_val& x) const {           \
        assert(is_constant() and x.is_constant());      \
        if (is_signed_type(type())) {                   \
            return t_val(s_val() op x.s_val(), type()); \
        } else if (is_floating_type(type())) {          \
            return t_val(f_val() op x.f_val(), type()); \
        } else {                                        \
            return t_val(u_val() op x.u_val(), type()); \
        }                                               \
    }
    ar_op(+); ar_op(-); ar_op(*);
#undef ar_op

    t_val operator&(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        return t_val(u_val() & x.u_val(), type());
    }
    t_val operator^(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        return t_val(u_val() ^ x.u_val(), type());
    }
    t_val operator|(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        return t_val(u_val() | x.u_val(), type());
    }
    t_val operator%(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        if (is_signed_type(type())) {
            return t_val(s_val() % x.s_val(), type());
        } else {
            return t_val(u_val() % x.u_val(), type());
        }
    }
    t_val operator<(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        if (is_signed_type(type())) {
            return t_val(s_val() < x.s_val(), int_type);
        } else if (is_floating_type(type())) {
            return t_val(f_val() < x.f_val(), int_type);
        } else {
            return t_val(u_val() < x.u_val(), int_type);
        }
    }
    t_val operator==(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        if (is_signed_type(type())) {
            return t_val(s_val() == x.s_val(), int_type);
        } else if (is_floating_type(type())) {
            return t_val(f_val() == x.f_val(), int_type);
        } else {
            return t_val(u_val() == x.u_val(), int_type);
        }
    }
    t_val operator/(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        if (is_signed_type(type())) {
            if (x.s_val() == 0) {
                return t_val(0l, type());
            }
            return t_val(s_val() / x.s_val(), type());
        } else if (is_floating_type(type())) {
            if (x.f_val() == 0) {
                return t_val(0.0, type());
            }
            return t_val(f_val() / x.f_val(), type());
        } else {
            if (x.u_val() == 0) {
                return t_val(0ul, type());
            }
            return t_val(u_val() / x.u_val(), type());
        }
    }

    t_val operator~() const {
        assert(is_constant());
        return t_val(~u_val(), type());
    }
    t_val operator<<(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        return t_val(u_val() << x.u_val(), type());
    }
    t_val operator>>(const t_val& x) const {
        assert(is_constant() and x.is_constant());
        if (is_signed_type(type())) {
            return t_val(s_val() >> x.u_val(), type());
        } else {
            return t_val(u_val() >> x.u_val(), type());
        }
    }
};

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

std::string make_new_id();
std::string make_label();
void put_label(const std::string& l, bool f = true);
std::string func_line(const std::string&);
t_type make_base_type(const t_ast& t, t_ctx& ctx);
std::string unpack_declarator(t_type& type, const t_ast& t, t_ctx& ctx);
std::string gen_asm(const t_ast&);

extern t_prog prog;
