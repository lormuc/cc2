#include <string>
#include <cassert>

#include "val.hpp"

void t_val::i_init(const t_type& t, unsigned long v) {
    if (is_unsigned_type(t) and t.get_size() < 8) {
        v &= (1ul << (t.get_size() * 8)) - 1;
    }
    _type = t;
    _is_constant = true;
    _i_val = v;
    if (is_signed_type(type())) {
        _asm_id = std::to_string(s_val());
    } else {
        _asm_id = std::to_string(u_val());
    }
}

void t_val::f_init(const t_type& t, double v) {
    _asm_id = std::to_string(v);
    _type = t;
    _is_constant = true;
    _f_val = v;
}

t_val::t_val(const std::string& id, const t_type& t, bool lv) {
    _asm_id = id;
    _type = t;
    _is_lvalue = lv;
}

t_val::t_val(void* p) {
    assert(p == (void*)0);
    _type = void_pointer_type;
    _is_void_null = true;
    _is_constant = true;
    _asm_id = "null";
}

bool t_val::is_false() const {
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

t_val t_val::operator+(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (is_signed_type(type())) {
        return t_val(s_val() + x.s_val(), type());
    } else if (is_floating_type(type())) {
        return t_val(f_val() + x.f_val(), type());
    } else {
        return t_val(u_val() + x.u_val(), type());
    }
}

t_val t_val::operator-(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (is_signed_type(type())) {
        return t_val(s_val() - x.s_val(), type());
    } else if (is_floating_type(type())) {
        return t_val(f_val() - x.f_val(), type());
    } else {
        return t_val(u_val() - x.u_val(), type());
    }
}

t_val t_val::operator*(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (is_signed_type(type())) {
        return t_val(s_val() * x.s_val(), type());
    } else if (is_floating_type(type())) {
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
    if (is_signed_type(type())) {
        return t_val(s_val() % x.s_val(), type());
    } else {
        return t_val(u_val() % x.u_val(), type());
    }
}
t_val t_val::operator<(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (is_signed_type(type())) {
        return t_val(s_val() < x.s_val(), int_type);
    } else if (is_floating_type(type())) {
        return t_val(f_val() < x.f_val(), int_type);
    } else {
        return t_val(u_val() < x.u_val(), int_type);
    }
}
t_val t_val::operator==(const t_val& x) const {
    assert(is_constant() and x.is_constant());
    if (is_signed_type(type())) {
        return t_val(s_val() == x.s_val(), int_type);
    } else if (is_floating_type(type())) {
        return t_val(f_val() == x.f_val(), int_type);
    } else {
        return t_val(u_val() == x.u_val(), int_type);
    }
}
t_val t_val::operator/(const t_val& x) const {
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
    if (is_signed_type(type())) {
        return t_val(s_val() >> x.u_val(), type());
    } else {
        return t_val(u_val() >> x.u_val(), type());
    }
}
