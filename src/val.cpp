#include <string>
#include <cassert>

#include "val.hpp"

void t_val::i_init(const t_type& t, unsigned long v) {
    if (t.is_unsigned() and t.size() < 8) {
        v &= (1ul << (t.size() * 8)) - 1;
    }
    _type = t;
    _is_constant = true;
    _i_val = v;
    if (t.is_pointer() and v == 0) {
        _as = "null";
    } else {
        _as = std::to_string(u_val());
    }
}

void t_val::f_init(const t_type& t, double v) {
    _as = std::to_string(v);
    _type = t;
    _is_constant = true;
    _f_val = v;
}

t_val::t_val(const str& id, const t_type& t, bool lv, bool n_is_constant) {
    _as = id;
    _type = t;
    _is_lvalue = lv;
    _is_constant = n_is_constant;
}

bool t_val::is_false() const {
    assert(is_constant());
    if (type().is_floating()) {
        return f_val() == 0;
    } else {
        return u_val() == 0;
    }
}

t_val t_val::operator+(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        return t_val(s_val() + x.s_val(), type());
    } else if (type().is_floating()) {
        return t_val(f_val() + x.f_val(), type());
    } else {
        return t_val(u_val() + x.u_val(), type());
    }
}

t_val t_val::operator-(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        return t_val(s_val() - x.s_val(), type());
    } else if (type().is_floating()) {
        return t_val(f_val() - x.f_val(), type());
    } else {
        return t_val(u_val() - x.u_val(), type());
    }
}

t_val t_val::operator*(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        return t_val(s_val() * x.s_val(), type());
    } else if (type().is_floating()) {
        return t_val(f_val() * x.f_val(), type());
    } else {
        return t_val(u_val() * x.u_val(), type());
    }
}

t_val t_val::operator&(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    return t_val(u_val() & x.u_val(), type());
}
t_val t_val::operator^(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    return t_val(u_val() ^ x.u_val(), type());
}
t_val t_val::operator|(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    return t_val(u_val() | x.u_val(), type());
}
t_val t_val::operator%(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        return t_val(s_val() % x.s_val(), type());
    } else {
        return t_val(u_val() % x.u_val(), type());
    }
}
t_val t_val::operator<(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        return t_val(s_val() < x.s_val());
    } else if (type().is_floating()) {
        return t_val(f_val() < x.f_val());
    } else {
        return t_val(u_val() < x.u_val());
    }
}
t_val t_val::operator==(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        return t_val(s_val() == x.s_val());
    } else if (type().is_floating()) {
        return t_val(f_val() == x.f_val());
    } else {
        return t_val(u_val() == x.u_val());
    }
}
t_val t_val::operator/(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        if (x.s_val() == 0) {
            return t_val(0l, type());
        }
        return t_val(s_val() / x.s_val(), type());
    } else if (type().is_floating()) {
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

t_val t_val::operator~() const {
    assert(is_constant());
    return t_val(~u_val(), type());
}
t_val t_val::operator<<(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    return t_val(u_val() << x.u_val(), type());
}
t_val t_val::operator>>(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (type().is_signed()) {
        return t_val(s_val() >> x.u_val(), type());
    } else {
        return t_val(u_val() >> x.u_val(), type());
    }
}

t_val make_null_pointer(t_type type) {
    return t_val(0ul, type);
}
