#pragma once

#include <string>

#include "type.hpp"

class t_val {
    str _as;
    t_type _type;
    bool _is_lvalue = false;
    bool _is_constant = false;
    bool _is_void_null = false;
    unsigned long _i_val;
    double _f_val;

    void i_init(const t_type& t, unsigned long v);
    void f_init(const t_type& t, double v);

public:
    t_val() { _type = void_type; }
    t_val(const str&, const t_type&, bool n_is_lvalue = false);
    t_val(void* p);
    t_val(unsigned long x, const t_type& t) { i_init(t, x); }
    t_val(bool x, const t_type& t) { i_init(t, x); }
    t_val(long x, const t_type& t) { i_init(t, x); }
    t_val(double x, const t_type& t) { f_init(t, x); }
    t_val(char x) { i_init(char_type, x); }
    t_val(unsigned char x) { i_init(u_char_type, x); }
    t_val(unsigned short x) { i_init(u_short_type, x); }
    t_val(unsigned int x) { i_init(u_int_type, x); }
    t_val(unsigned long x) { i_init(u_long_type, x); }
    t_val(unsigned long long x) { i_init(u_long_type, x); }
    t_val(signed char x) { i_init(s_char_type, x); }
    t_val(signed short x) { i_init(short_type, x); }
    t_val(signed int x) { i_init(int_type, x); }
    t_val(signed long x) { i_init(long_type, x); }
    t_val(float x) { f_init(float_type, x); }
    t_val(double x) { f_init(double_type, x); }
    t_val(long double x) { f_init(long_double_type, x); }
    str as() const { return _as; }
    const t_type& type() const { return _type; }
    bool is_lvalue() const { return _is_lvalue; }
    bool is_constant() const { return _is_constant; }
    unsigned long u_val() const { return _i_val; }
    long s_val() const { return long(_i_val); }
    double f_val() const { return _f_val; }
    bool is_void_null() const { return _is_void_null; }
    void set_type(t_type t) { _type = t; }
    bool is_false() const;

    t_val operator~() const;
    t_val operator+(const t_val& x) const;
    t_val operator-(const t_val& x) const;
    t_val operator*(const t_val& x) const;
    t_val operator&(const t_val& x) const;
    t_val operator^(const t_val& x) const;
    t_val operator|(const t_val& x) const;
    t_val operator%(const t_val& x) const;
    t_val operator<(const t_val& x) const;
    t_val operator==(const t_val& x) const;
    t_val operator/(const t_val& x) const;
    t_val operator<<(const t_val& x) const;
    t_val operator>>(const t_val& x) const;
};
